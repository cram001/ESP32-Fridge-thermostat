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

}  // namespace

int main() {
  test_missing_freezer_is_optional();
  test_valid_warm_freezer_locks_out();
  test_emergency_mode_defaults_off();
  test_emergency_hourly_duty_cycle();
  test_emergency_respects_only_a_valid_lockout();
  test_changing_emergency_setting_restarts_on_phase();

  if (failures != 0) {
    std::cerr << failures << " controller test(s) failed\n";
    return 1;
  }
  std::cout << "All controller tests passed\n";
  return 0;
}
