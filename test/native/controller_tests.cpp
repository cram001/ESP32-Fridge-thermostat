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

void test_missing_freezer_is_optional() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  FridgeController controller;

  g_fake_millis = 1000;
  check(!controller.update(8.0f, NAN, settings, 0.5f).spillover,
        "hot fridge begins qualification with missing freezer");
  g_fake_millis = 6000;
  check(controller.update(8.0f, NAN, settings, 0.5f).spillover,
        "missing freezer does not prevent normal fridge control");
}

void test_valid_warm_freezer_locks_out() {
  ControllerSettings settings;
  settings.fan_delay_s = 5;
  FridgeController controller;

  g_fake_millis = 1000;
  controller.update(8.0f, -2.0f, settings, 0.5f);
  g_fake_millis = 7000;
  const ControllerOutput output =
      controller.update(8.0f, -2.0f, settings, 0.5f);
  check(output.freezer_lockout, "valid warm freezer reports lockout");
  check(!output.spillover, "valid warm freezer prevents spillover");
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

  check(settings.high_c == hw::kFridgeControlMinC,
        "spillover threshold is clamped independently");
  check(settings.low_c == hw::kFridgeControlMaxC,
        "circulation threshold is clamped independently");
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
  check(settings.oled_contrast_percent == hw::kOledContrastMinPercent,
        "OLED contrast is clamped");
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

}  // namespace

int main() {
  test_missing_freezer_is_optional();
  test_valid_warm_freezer_locks_out();
  test_emergency_mode_defaults_off();
  test_emergency_hourly_duty_cycle();
  test_emergency_respects_only_a_valid_lockout();
  test_changing_emergency_setting_restarts_on_phase();
  test_settings_normalization_enforces_numeric_bounds();
  test_settings_normalization_enforces_enums_and_finite_values();

  if (failures != 0) {
    std::cerr << failures << " controller test(s) failed\n";
    return 1;
  }
  std::cout << "All controller tests passed\n";
  return 0;
}
