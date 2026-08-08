#include <cmath>
#include <cstdint>
#include <iostream>

#include "fridge_controller.h"

uint32_t g_fake_millis = 0;

namespace {

int failures = 0;
int expected_limitations = 0;

void check(bool condition, const char* description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    failures++;
  }
}

void limitation(bool condition, const char* description) {
  if (condition) {
    std::cout << "LIMITATION: " << description << '\n';
    expected_limitations++;
  } else {
    std::cerr << "FAIL: expected limitation not reproduced: " << description
              << '\n';
    failures++;
  }
}

struct StuckTemperatureMonitor {
  float last_changed_c = NAN;
  uint32_t last_change_ms = 0;
  bool stuck = false;

  bool update(uint32_t now, bool valid, float raw_c) {
    if (!valid || !std::isfinite(raw_c)) {
      last_changed_c = NAN;
      last_change_ms = now;
      stuck = false;
      return false;
    }
    if (!std::isfinite(last_changed_c)) {
      last_changed_c = raw_c;
      last_change_ms = now;
      stuck = false;
      return false;
    }
    if (std::fabs(raw_c - last_changed_c) >= hw::kTemperatureStuckChangeC) {
      last_changed_c = raw_c;
      last_change_ms = now;
      stuck = false;
      return false;
    }
    stuck = now - last_change_ms >= hw::kTemperatureStuckTimeoutMs;
    return stuck;
  }
};

struct FirmwareSim {
  ControllerSettings settings;
  FridgeController controller;
  EmergencySpilloverController emergency;
  ControllerOutput output;
  bool temperature_alarm_armed = true;
  uint32_t spillover_started_ms = 0;

  bool critical_probe_alarm = false;
  bool temperature_alarm = false;
  bool spillover_long_run = false;

  void step(uint32_t now, float fridge_c, float freezer_c,
            bool fridge_valid = true, bool freezer_valid = true) {
    g_fake_millis = now;
    const float fridge_input = fridge_valid ? fridge_c : NAN;
    const float freezer_input = freezer_valid ? freezer_c : NAN;

    output = controller.update(fridge_input, freezer_input, settings, true);
    if (!fridge_valid) {
      output.spillover = emergency.update(now, true, freezer_valid,
                                          freezer_input, settings);
      output.circulation = false;
    } else {
      emergency.update(now, false, false, NAN, settings);
    }

    if (output.spillover) {
      if (spillover_started_ms == 0) spillover_started_ms = now;
    } else {
      spillover_started_ms = 0;
    }
    spillover_long_run =
        spillover_started_ms != 0 &&
        now - spillover_started_ms >= hw::kLongFanRunMs;

    critical_probe_alarm = !fridge_valid;
    temperature_alarm =
        temperature_alarm_armed &&
        ((fridge_valid && fridge_c >= settings.fridge_alarm_c) ||
         (freezer_valid && freezer_c >= settings.freezer_alarm_c));
  }
};

// Deliberately simple deterministic thermal plant. It is not intended to
// predict real compartment temperatures; it only gives the firmware changing
// inputs so failure responses can be exercised end-to-end.
struct ThermalPlant {
  float fridge_c = 5.0f;
  float freezer_c = -10.0f;
  float ambient_c = 20.0f;
  bool compressor_ok = true;
  bool spillover_fan_ok = true;
  bool circulation_fan_ok = true;
  bool spillover_stuck_on = false;
  bool circulation_stuck_on = false;

  bool physical_spillover(bool command) const {
    return spillover_stuck_on || (command && spillover_fan_ok);
  }
  bool physical_circulation(bool command) const {
    return circulation_stuck_on || (command && circulation_fan_ok);
  }

  void advance_one_minute(bool spillover_command, bool circulation_command) {
    const bool spill = physical_spillover(spillover_command);
    const bool circ = physical_circulation(circulation_command);
    (void)circ;  // circulation affects mixing, not mean temperature in this model

    // Passive heat leak toward ambient.
    fridge_c += (ambient_c - fridge_c) * 0.004f;
    freezer_c += (ambient_c - freezer_c) * 0.0015f;

    // A healthy compressor provides strong net freezer cooling. A failed
    // compressor contributes no cooling and the freezer warms naturally.
    if (compressor_ok) freezer_c -= 0.12f;

    // Spillover cools the fridge but loads the freezer with returned heat.
    if (spill) {
      fridge_c -= 0.22f;
      freezer_c += 0.05f;
    }
  }
};

void test_fridge_sensor_missing_default_fails_safe() {
  FirmwareSim sim;
  sim.settings.emergency_spillover_on_min = 0;
  sim.step(1000, 8.0f, -10.0f, false, true);
  check(sim.critical_probe_alarm,
        "missing fridge probe raises the critical probe alarm");
  check(!sim.output.spillover,
        "missing fridge probe leaves spillover OFF when GET-HOME is OFF");
  check(!sim.output.circulation,
        "missing fridge probe leaves circulation OFF");
}

void test_fridge_sensor_missing_get_home_duty_cycle_and_lockout() {
  FirmwareSim sim;
  sim.settings.emergency_spillover_on_min = 10;
  sim.step(1000, 8.0f, -10.0f, false, true);
  check(sim.output.spillover,
        "GET-HOME starts its selected ON phase immediately");
  sim.step(1000 + 10UL * 60UL * 1000UL, 8.0f, -10.0f, false, true);
  check(!sim.output.spillover,
        "GET-HOME turns spillover OFF after the selected minutes per hour");

  FirmwareSim warm_freezer;
  warm_freezer.settings.emergency_spillover_on_min = 20;
  warm_freezer.step(2000, 8.0f, -2.0f, false, true);
  check(!warm_freezer.output.spillover,
        "valid warm freezer blocks GET-HOME spillover");
}

void test_freezer_sensor_missing_keeps_fridge_control_available() {
  FirmwareSim sim;
  sim.settings.fan_delay_s = 5;
  sim.step(1000, 8.0f, -10.0f, true, false);
  sim.step(7000, 8.0f, -10.0f, true, false);
  check(sim.output.spillover,
        "missing freezer probe does not disable fridge spillover control");
  check(!sim.output.freezer_lockout,
        "missing freezer probe cannot assert freezer lockout");
}

void test_stuck_temperature_advisory_timing() {
  StuckTemperatureMonitor monitor;
  check(!monitor.update(1000, true, 4.00f),
        "unchanged-temperature monitor initializes quietly");
  check(!monitor.update(1000 + hw::kTemperatureStuckTimeoutMs - 1, true, 4.00f),
        "unchanged temperature does not warn before 30 minutes");
  check(monitor.update(1000 + hw::kTemperatureStuckTimeoutMs, true, 4.00f),
        "unchanged temperature warns at 30 minutes");
  check(!monitor.update(1000 + hw::kTemperatureStuckTimeoutMs + 5000,
                        true, 4.25f),
        "one DS18B20 10-bit count of movement clears the warning");
}

void test_spillover_fan_failed_off_is_eventually_observable() {
  FirmwareSim sim;
  ThermalPlant plant;
  plant.fridge_c = 7.0f;
  plant.spillover_fan_ok = false;
  sim.settings.fan_delay_s = 5;

  bool saw_command = false;
  bool saw_fridge_alarm = false;
  bool saw_long_run = false;
  for (uint32_t minute = 0; minute <= 120; ++minute) {
    const uint32_t now = 1000 + minute * 60UL * 1000UL;
    sim.step(now, plant.fridge_c, plant.freezer_c);
    saw_command = saw_command || sim.output.spillover;
    saw_fridge_alarm = saw_fridge_alarm || sim.temperature_alarm;
    saw_long_run = saw_long_run || sim.spillover_long_run;
    plant.advance_one_minute(sim.output.spillover, sim.output.circulation);
  }

  check(saw_command, "firmware continues commanding a failed spillover fan");
  check(saw_fridge_alarm,
        "failed spillover fan eventually produces a high-fridge alarm in the model");
  check(saw_long_run,
        "failed spillover fan eventually produces the >60 minute command-run fault");
}

void test_spillover_fan_stuck_on_is_not_directly_detectable() {
  FirmwareSim sim;
  ThermalPlant plant;
  plant.fridge_c = 3.0f;
  plant.spillover_stuck_on = true;

  sim.step(1000, plant.fridge_c, plant.freezer_c);
  const bool commanded_off = !sim.output.spillover;
  const bool physically_on = plant.physical_spillover(sim.output.spillover);
  limitation(commanded_off && physically_on && !sim.spillover_long_run,
             "spillover MOSFET/fan stuck ON has no direct feedback; firmware can command OFF while the fan remains physically ON");
}

void test_circulation_fan_failures_are_not_directly_detectable() {
  FirmwareSim sim;
  ThermalPlant failed_off;
  failed_off.fridge_c = 8.0f;
  failed_off.circulation_fan_ok = false;
  sim.settings.fan_delay_s = 5;
  sim.step(1000, failed_off.fridge_c, failed_off.freezer_c);
  sim.step(7000, failed_off.fridge_c, failed_off.freezer_c);
  limitation(sim.output.circulation &&
                 !failed_off.physical_circulation(sim.output.circulation),
             "circulation fan failed OFF is not distinguishable from a healthy commanded fan without RPM/current feedback");

  FirmwareSim stuck_sim;
  ThermalPlant stuck_on;
  stuck_on.fridge_c = 5.0f;
  stuck_on.circulation_stuck_on = true;
  stuck_sim.step(1000, stuck_on.fridge_c, stuck_on.freezer_c);
  limitation(!stuck_sim.output.circulation &&
                 stuck_on.physical_circulation(stuck_sim.output.circulation),
             "circulation fan stuck ON is not directly detectable without feedback");
}

void test_compressor_failure_lockout_then_alarm() {
  FirmwareSim sim;
  ThermalPlant plant;
  plant.fridge_c = 7.0f;
  plant.freezer_c = -10.0f;
  plant.compressor_ok = false;
  sim.settings.fan_delay_s = 5;

  bool saw_spillover_before_lockout = false;
  bool saw_lockout = false;
  bool saw_freezer_alarm = false;
  bool spillover_off_at_lockout = false;

  for (uint32_t minute = 0; minute <= 180; ++minute) {
    const uint32_t now = 1000 + minute * 60UL * 1000UL;
    sim.step(now, plant.fridge_c, plant.freezer_c);
    if (!sim.output.freezer_lockout && sim.output.spillover) {
      saw_spillover_before_lockout = true;
    }
    if (sim.output.freezer_lockout) {
      saw_lockout = true;
      spillover_off_at_lockout = spillover_off_at_lockout || !sim.output.spillover;
    }
    if (plant.freezer_c >= sim.settings.freezer_alarm_c &&
        sim.temperature_alarm) {
      saw_freezer_alarm = true;
    }
    plant.advance_one_minute(sim.output.spillover, sim.output.circulation);
  }

  check(saw_spillover_before_lockout,
        "spillover may run while failed-compressor freezer is still cold enough");
  check(saw_lockout,
        "compressor failure eventually reaches freezer lockout in the model");
  check(spillover_off_at_lockout,
        "freezer lockout stops spillover immediately during compressor failure");
  check(saw_freezer_alarm,
        "continued freezer warming eventually raises the freezer high-temperature alarm");
}

void test_compressor_failure_with_missing_freezer_probe_loses_protection() {
  FirmwareSim sim;
  sim.settings.fan_delay_s = 5;
  sim.step(1000, 8.0f, -1.0f, true, false);
  sim.step(7000, 8.0f, -1.0f, true, false);

  limitation(sim.output.spillover && !sim.output.freezer_lockout &&
                 !sim.temperature_alarm,
             "compressor failure plus missing freezer probe cannot be inferred: freezer lockout/alarm are unavailable and fridge control continues");
}

void test_compressor_failure_with_stuck_cold_freezer_probe() {
  FirmwareSim sim;
  StuckTemperatureMonitor freezer_monitor;
  const float reported_freezer_c = -10.0f;

  bool stuck_warning = false;
  for (uint32_t minute = 0; minute <= 35; ++minute) {
    const uint32_t now = 1000 + minute * 60UL * 1000UL;
    sim.step(now, 8.0f, reported_freezer_c, true, true);
    stuck_warning = freezer_monitor.update(now, true, reported_freezer_c);
  }

  check(stuck_warning,
        "stuck-cold freezer probe produces the 30-minute unchanged-temperature advisory");
  limitation(!sim.output.freezer_lockout && !sim.temperature_alarm,
             "a plausible but stuck-cold freezer reading can mask a real compressor failure; firmware has only the unchanged-temperature advisory without independent freezer feedback");
}

void test_startup_alarm_grace_during_compressor_failure() {
  FirmwareSim sim;
  sim.temperature_alarm_armed = false;
  sim.step(30UL * 60UL * 1000UL, 6.0f, 0.0f);
  check(sim.output.freezer_lockout,
        "freezer lockout remains active even when startup temperature alarm is not armed");
  check(!sim.temperature_alarm,
        "temperature alarm remains suppressed while startup alarm grace is active");

  sim.temperature_alarm_armed = true;
  sim.step(hw::kStartupAlarmGraceMs, 6.0f, 0.0f);
  check(sim.temperature_alarm,
        "warm freezer raises temperature alarm once startup alarm is armed");
}

}  // namespace

int main() {
  test_fridge_sensor_missing_default_fails_safe();
  test_fridge_sensor_missing_get_home_duty_cycle_and_lockout();
  test_freezer_sensor_missing_keeps_fridge_control_available();
  test_stuck_temperature_advisory_timing();
  test_spillover_fan_failed_off_is_eventually_observable();
  test_spillover_fan_stuck_on_is_not_directly_detectable();
  test_circulation_fan_failures_are_not_directly_detectable();
  test_compressor_failure_lockout_then_alarm();
  test_compressor_failure_with_missing_freezer_probe_loses_protection();
  test_compressor_failure_with_stuck_cold_freezer_probe();
  test_startup_alarm_grace_during_compressor_failure();

  if (failures != 0) {
    std::cerr << failures << " failure-mode simulation(s) failed\n";
    return 1;
  }
  std::cout << "All failure-mode simulations passed; "
            << expected_limitations << " expected hardware-observability limitations reproduced\n";
  return 0;
}
