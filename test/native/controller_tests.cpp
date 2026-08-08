#include <cmath>
#include <cstdint>
#include <iostream>

#include "fridge_controller.h"

uint32_t g_fake_millis = 0;

namespace {

int failures = 0;

void check(bool condition, const char* description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    failures++;
  }
}

uint32_t after(uint32_t start, uint32_t elapsed_ms) {
  return start + elapsed_ms;  // intentional uint32_t rollover
}

void test_missing_freezer_is_optional() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  FridgeController controller;

  g_fake_millis = 1000;
  check(!controller.update(8.0f, NAN, settings).spillover,
        "hot fridge begins qualification with missing freezer");
  g_fake_millis = 6000;
  check(controller.update(8.0f, NAN, settings).spillover,
        "missing freezer does not prevent normal fridge control");
}

void test_valid_warm_freezer_locks_out() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  FridgeController controller;

  g_fake_millis = 1000;
  controller.update(8.0f, -2.0f, settings);
  g_fake_millis = 7000;
  const ControllerOutput output =
      controller.update(8.0f, -2.0f, settings);
  check(output.freezer_lockout, "valid warm freezer reports lockout");
  check(!output.spillover, "valid warm freezer prevents spillover");
}

void test_fresh_samples_qualify_spillover_start() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  FridgeController controller;

  g_fake_millis = 1000;
  controller.update(8.0f, -10.0f, settings, true);
  g_fake_millis = 7000;
  check(!controller.update(8.0f, -10.0f, settings, false).spillover,
        "stored reading cannot complete the start delay");
  g_fake_millis = 8000;
  check(controller.update(8.0f, -10.0f, settings, true).spillover,
        "fresh qualifying reading completes the start delay");
}

void test_spillover_runs_to_min_temperature_and_minimum_time() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  settings.spillover_min_on_min = 1;
  settings.low_c = 3.0f;
  FridgeController controller;

  g_fake_millis = 1000;
  controller.update(5.0f, -10.0f, settings);
  g_fake_millis = 6000;
  ControllerOutput output = controller.update(5.0f, -10.0f, settings);
  check(output.spillover, "fridge max starts spillover");
  check(output.circulation, "circulation follows spillover immediately");

  g_fake_millis = 7000;
  output = controller.update(2.5f, -10.0f, settings);
  check(output.spillover,
        "fridge min cannot override spillover minimum runtime");
  g_fake_millis = 66000;
  output = controller.update(2.5f, -10.0f, settings);
  check(!output.spillover,
        "spillover stops at fridge min after minimum runtime");
  check(output.circulation,
        "circulation remains requested while below fridge min");
}

void test_freezer_lockout_overrides_spillover_minimum_time() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  settings.spillover_min_on_min = 5;
  FridgeController controller;

  g_fake_millis = 1000;
  controller.update(8.0f, -10.0f, settings);
  g_fake_millis = 6000;
  check(controller.update(8.0f, -10.0f, settings).spillover,
        "spillover starts before lockout test");
  g_fake_millis = 7000;
  const ControllerOutput output =
      controller.update(8.0f, settings.freezer_lockout_c, settings);
  check(output.freezer_lockout,
        "freezer locks out at the configured maximum");
  check(!output.spillover,
        "freezer lockout immediately overrides minimum runtime");
}

void test_cold_circulation_respects_delay_and_minimum_time() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  settings.circulation_min_on_min = 1;
  FridgeController controller;

  g_fake_millis = 1000;
  check(!controller.update(3.5f, -10.0f, settings).circulation,
        "cold fridge begins circulation qualification");
  g_fake_millis = 6000;
  check(controller.update(3.5f, -10.0f, settings).circulation,
        "persistent cold starts circulation");
  g_fake_millis = 7000;
  check(controller.update(4.5f, -10.0f, settings).circulation,
        "circulation minimum runtime overrides cleared request");
  g_fake_millis = 66000;
  check(!controller.update(4.5f, -10.0f, settings).circulation,
        "circulation stops after its minimum runtime");
}

void test_emergency_mode_defaults_off() {
  ControllerSettings settings;
  EmergencySpilloverController emergency;
  check(!emergency.update(0, true, false, NAN, settings),
        "get-me-home mode defaults to OFF");
}

void test_emergency_hourly_duty_cycle() {
  ControllerSettings settings;
  settings.emergency_spillover_on_min = 10;
  EmergencySpilloverController emergency;

  check(emergency.update(1000, true, false, NAN, settings),
        "10-minute mode starts immediately");
  check(emergency.update(1000 + 10UL * 60UL * 1000UL - 1, true, false, NAN,
                         settings),
        "10-minute mode stays on for the full selected interval");
  check(!emergency.update(1000 + 10UL * 60UL * 1000UL, true, false, NAN,
                          settings),
        "10-minute mode switches off at ten minutes");
  check(emergency.update(1000 + 60UL * 60UL * 1000UL, true, false, NAN,
                         settings),
        "10-minute mode restarts at the next hour");
}

void test_emergency_respects_only_a_valid_lockout() {
  ControllerSettings settings;
  settings.emergency_spillover_on_min = 20;
  EmergencySpilloverController emergency;

  check(emergency.update(5000, true, false, NAN, settings),
        "missing freezer is ignored in get-me-home mode");
  check(!emergency.update(6000, true, true, -2.0f, settings),
        "valid warm freezer blocks get-me-home spillover");
  check(!emergency.update(6500, true, true, settings.freezer_lockout_c,
                          settings),
        "freezer lockout engages exactly at its configured maximum");
  check(emergency.update(7000, true, true, -10.0f, settings),
        "valid cold freezer permits get-me-home spillover");
}

void test_changing_emergency_setting_restarts_on_phase() {
  ControllerSettings settings;
  settings.emergency_spillover_on_min = 5;
  EmergencySpilloverController emergency;
  check(emergency.update(0, true, false, NAN, settings),
        "five-minute mode starts immediately even at millis zero");
  check(!emergency.update(20UL * 60UL * 1000UL, true, false, NAN, settings),
        "five-minute mode is off later in the hour");

  settings.emergency_spillover_on_min = 10;
  check(emergency.update(20UL * 60UL * 1000UL, true, false, NAN, settings),
        "changing the selected duty cycle starts a fresh on phase");
}

void test_fan_delay_survives_millis_rollover() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  FridgeController controller;
  const uint32_t start = 0xFFFFFF00U;

  g_fake_millis = start;
  check(!controller.update(8.0f, -10.0f, settings, true).spillover,
        "rollover test starts fan qualification");
  g_fake_millis = after(start, 4999U);
  check(!controller.update(8.0f, -10.0f, settings, true).spillover,
        "fan delay remains active across millis rollover");
  g_fake_millis = after(start, 5000U);
  check(controller.update(8.0f, -10.0f, settings, true).spillover,
        "fan delay expires correctly across millis rollover");
}

void test_minimum_runtime_survives_millis_rollover() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  settings.spillover_min_on_min = 1;
  FridgeController controller;
  const uint32_t start = 0xFFFFF000U;

  g_fake_millis = start;
  controller.update(8.0f, -10.0f, settings, true);
  g_fake_millis = after(start, 5000U);
  check(controller.update(8.0f, -10.0f, settings, true).spillover,
        "spillover starts across millis rollover");
  g_fake_millis = after(start, 5000U + 59999U);
  check(controller.update(2.0f, -10.0f, settings, true).spillover,
        "minimum runtime remains active across rollover");
  g_fake_millis = after(start, 5000U + 60000U);
  check(!controller.update(2.0f, -10.0f, settings, true).spillover,
        "minimum runtime expires correctly across rollover");
}

void test_emergency_cycle_survives_millis_rollover() {
  ControllerSettings settings;
  settings.emergency_spillover_on_min = 5;
  EmergencySpilloverController emergency;
  const uint32_t start = 0xFFFF0000U;

  check(emergency.update(start, true, false, NAN, settings),
        "get-me-home starts before rollover");
  check(emergency.update(after(start, 5U * 60U * 1000U - 1U), true, false,
                         NAN, settings),
        "get-me-home ON interval survives rollover");
  check(!emergency.update(after(start, 5U * 60U * 1000U), true, false, NAN,
                          settings),
        "get-me-home turns off correctly after rollover");
  check(emergency.update(after(start, 60U * 60U * 1000U), true, false, NAN,
                         settings),
        "get-me-home next-hour phase survives rollover");
}

void test_settings_normalization_enforces_numeric_bounds() {
  ControllerSettings settings;
  settings.high_c = -100.0f;
  settings.low_c = 100.0f;
  settings.freezer_lockout_c = -100.0f;
  settings.fridge_alarm_c = 100.0f;
  settings.freezer_alarm_c = 100.0f;
  settings.fan_delay_s = 1;
  settings.spillover_min_on_min = 0;
  settings.circulation_min_on_min = 99;
  settings.oled_contrast_percent = 0;

  NormalizeControllerSettings(settings);

  check(settings.high_c ==
            hw::kFridgeControlMinC + hw::kFridgeControlMinimumBandC,
        "fridge max preserves the minimum control band");
  check(settings.low_c == hw::kFridgeControlMinC,
        "fridge min remains below fridge max");
  check(settings.freezer_lockout_c == hw::kFreezerThresholdMinC,
        "freezer lockout is clamped");
  check(settings.fridge_alarm_c == hw::kFridgeAlarmMaxC,
        "fridge alarm is clamped");
  check(settings.freezer_alarm_c == hw::kFreezerAlarmMaxC,
        "freezer alarm is clamped");
  check(settings.fan_delay_s == hw::kFanDelayMinS,
        "fan delay is clamped");
  check(settings.spillover_min_on_min == hw::kFanMinimumOnMin,
        "spillover minimum runtime is clamped");
  check(settings.circulation_min_on_min == hw::kFanMinimumOnMax,
        "circulation minimum runtime is clamped");
  check(settings.oled_contrast_percent ==
            ControllerSettings().oled_contrast_percent,
        "unsupported OLED contrast returns to default");
}

void test_settings_normalization_enforces_enums_and_finite_values() {
  ControllerSettings settings;
  settings.high_c = NAN;
  settings.low_c = INFINITY;
  settings.emergency_spillover_on_min = 7;
  settings.display_timeout_min = 2;

  NormalizeControllerSettings(settings);

  const ControllerSettings defaults;
  check(settings.high_c == defaults.high_c,
        "non-finite high threshold returns to default");
  check(settings.low_c == defaults.low_c,
        "non-finite low threshold returns to default");
  check(settings.emergency_spillover_on_min ==
            defaults.emergency_spillover_on_min,
        "unsupported emergency duty cycle returns to default");
  check(settings.display_timeout_min == defaults.display_timeout_min,
        "unsupported display timeout returns to default");
}

void test_settings_normalization_enforces_control_band() {
  ControllerSettings settings;
  settings.high_c = 5.0f;
  settings.low_c = 8.0f;

  NormalizeControllerSettings(settings);

  check(settings.high_c == 5.0f,
        "valid fridge max remains unchanged");
  check(settings.low_c ==
            settings.high_c - hw::kFridgeControlMinimumBandC,
        "fridge min remains below fridge max by the minimum band");
}

}  // namespace

int main() {
  test_missing_freezer_is_optional();
  test_valid_warm_freezer_locks_out();
  test_fresh_samples_qualify_spillover_start();
  test_spillover_runs_to_min_temperature_and_minimum_time();
  test_freezer_lockout_overrides_spillover_minimum_time();
  test_cold_circulation_respects_delay_and_minimum_time();
  test_emergency_mode_defaults_off();
  test_emergency_hourly_duty_cycle();
  test_emergency_respects_only_a_valid_lockout();
  test_changing_emergency_setting_restarts_on_phase();
  test_fan_delay_survives_millis_rollover();
  test_minimum_runtime_survives_millis_rollover();
  test_emergency_cycle_survives_millis_rollover();
  test_settings_normalization_enforces_numeric_bounds();
  test_settings_normalization_enforces_enums_and_finite_values();
  test_settings_normalization_enforces_control_band();

  if (failures != 0) {
    std::cerr << failures << " controller test(s) failed\n";
    return 1;
  }
  std::cout << "All controller tests passed\n";
  return 0;
}
