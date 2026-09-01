#include <cassert>
#include <cstdint>
#include <iostream>

#include "fridge_controller.h"

uint32_t g_fake_millis = 0;

namespace {
constexpr uint32_t kMinuteMs = 60UL * 1000UL;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::abort();
  }
}

ControllerSettings idle_settings() {
  ControllerSettings settings;
  settings.high_c = 5.0f;
  settings.low_c = 4.0f;
  settings.circulation_mix_interval_min = 6;
  return settings;
}

void test_periodic_mix_starts_after_idle_interval_and_runs_three_minutes() {
  FridgeController controller;
  ControllerSettings settings = idle_settings();

  g_fake_millis = 1000;
  auto output = controller.update(4.5f, -10.0f, settings, true);
  expect(!output.circulation, "circulation should initially be idle");
  expect(!output.spillover, "periodic mixing must not start spillover");

  g_fake_millis = 1000 + 6 * kMinuteMs - 1;
  output = controller.update(4.5f, -10.0f, settings, true);
  expect(!output.circulation, "mix must wait for the complete idle interval");

  g_fake_millis = 1000 + 6 * kMinuteMs;
  output = controller.update(4.5f, -10.0f, settings, true);
  expect(output.circulation, "mix should start at the selected interval");
  expect(!output.spillover, "mix must run only the circulation fan");

  g_fake_millis = 1000 + 6 * kMinuteMs + 3 * kMinuteMs - 1;
  output = controller.update(4.5f, -10.0f, settings, true);
  expect(output.circulation, "mix should remain on for the full three minutes");

  g_fake_millis = 1000 + 6 * kMinuteMs + 3 * kMinuteMs;
  output = controller.update(4.5f, -10.0f, settings, true);
  expect(!output.circulation, "mix should stop after three minutes");
}

void test_next_interval_starts_after_previous_mix_stops() {
  FridgeController controller;
  ControllerSettings settings = idle_settings();

  g_fake_millis = 5000;
  controller.update(4.5f, -10.0f, settings, true);
  g_fake_millis += 6 * kMinuteMs;
  expect(controller.update(4.5f, -10.0f, settings, true).circulation,
         "first periodic mix should start");
  g_fake_millis += 3 * kMinuteMs;
  expect(!controller.update(4.5f, -10.0f, settings, true).circulation,
         "first periodic mix should stop");

  g_fake_millis += 6 * kMinuteMs - 1;
  expect(!controller.update(4.5f, -10.0f, settings, true).circulation,
         "next interval should be measured from mix stop");
  g_fake_millis += 1;
  expect(controller.update(4.5f, -10.0f, settings, true).circulation,
         "second periodic mix should start after a fresh idle interval");
}

void test_off_override_and_probe_fault_inhibit_periodic_mix() {
  {
    FridgeController controller;
    ControllerSettings settings = idle_settings();
    settings.circulation_mix_interval_min = 0;
    g_fake_millis = 1000;
    controller.update(4.5f, -10.0f, settings, true);
    g_fake_millis += 2UL * 60UL * kMinuteMs;
    expect(!controller.update(4.5f, -10.0f, settings, true).circulation,
           "OFF must disable periodic mixing");
  }

  {
    FridgeController controller;
    ControllerSettings settings = idle_settings();
    settings.fan_override_all_off = true;
    g_fake_millis = 1000;
    controller.update(4.5f, -10.0f, settings, true);
    g_fake_millis += 6 * kMinuteMs;
    expect(!controller.update(4.5f, -10.0f, settings, true).circulation,
           "fan override must inhibit periodic mixing");
  }

  {
    FridgeController controller;
    ControllerSettings settings = idle_settings();
    g_fake_millis = 1000;
    controller.update(NAN, -10.0f, settings, true);
    g_fake_millis += 6 * kMinuteMs;
    expect(!controller.update(NAN, -10.0f, settings, true).circulation,
           "fridge probe fault must inhibit periodic mixing");
  }
}

void test_option_normalization() {
  ControllerSettings settings;
  settings.circulation_mix_interval_min = 45;
  NormalizeControllerSettings(settings);
  expect(settings.circulation_mix_interval_min == 45,
         "allowed mix interval should survive normalization");

  settings.circulation_mix_interval_min = 7;
  NormalizeControllerSettings(settings);
  expect(settings.circulation_mix_interval_min == 0,
         "unsupported mix interval should normalize to OFF");
}
}  // namespace

int main() {
  test_periodic_mix_starts_after_idle_interval_and_runs_three_minutes();
  test_next_interval_starts_after_previous_mix_stops();
  test_off_override_and_probe_fault_inhibit_periodic_mix();
  test_option_normalization();
  std::cout << "circulation mix tests passed\n";
  return 0;
}
