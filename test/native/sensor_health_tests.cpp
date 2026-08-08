#include <cstdint>
#include <iostream>

#include "sensor_health_tracker.h"

namespace {
int failures = 0;

void check(bool condition, const char* description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void test_requires_a_good_sample_before_use() {
  SensorHealthTracker tracker;
  check(!tracker.usable(1000, 3, 15000),
        "sensor is not usable before its first good sample");
}

void test_transient_failures_are_tolerated() {
  SensorHealthTracker tracker;
  tracker.note_good(1000);
  check(tracker.usable(1000, 3, 15000), "good sample is immediately usable");
  check(!tracker.note_failure(6000, 3, 15000),
        "first failed read retains recent good sample");
  check(!tracker.note_failure(11000, 3, 15000),
        "second failed read retains recent good sample");
  check(tracker.note_failure(16000, 3, 15000),
        "third failed read expires the sensor");
}

void test_good_sample_recovers_immediately() {
  SensorHealthTracker tracker;
  tracker.note_good(1000);
  tracker.note_failure(6000, 3, 15000);
  tracker.note_failure(11000, 3, 15000);
  tracker.note_failure(16000, 3, 15000);
  tracker.note_good(17000);
  check(tracker.usable(17000, 3, 15000),
        "one new good sample clears prior read failures");
  check(tracker.consecutive_failures() == 0,
        "recovery clears the failure counter");
}

void test_freshness_timeout_is_independent_of_value_change() {
  SensorHealthTracker tracker;
  tracker.note_good(1000);
  tracker.note_good(6000);
  tracker.note_good(11000);
  check(tracker.usable(20000, 3, 15000),
        "repeated identical good samples remain valid");
  check(!tracker.usable(26000, 3, 15000),
        "sensor expires when no good sample arrives before freshness timeout");
}

void test_millis_rollover() {
  SensorHealthTracker tracker;
  const uint32_t start = 0xFFFFFF00u;
  tracker.note_good(start);
  const uint32_t after_rollover = start + 10000u;
  check(tracker.usable(after_rollover, 3, 15000),
        "freshness remains valid across millis rollover");
  const uint32_t expired = start + 16000u;
  check(!tracker.usable(expired, 3, 15000),
        "freshness timeout remains correct across millis rollover");
}
}  // namespace

int main() {
  test_requires_a_good_sample_before_use();
  test_transient_failures_are_tolerated();
  test_good_sample_recovers_immediately();
  test_freshness_timeout_is_independent_of_value_change();
  test_millis_rollover();

  if (failures != 0) {
    std::cerr << failures << " sensor-health test(s) failed\n";
    return 1;
  }
  std::cout << "All sensor-health tests passed\n";
  return 0;
}
