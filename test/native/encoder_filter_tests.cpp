#include <cstdint>
#include <iostream>

#include "encoder_delta_filter.h"

namespace {
int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void test_stale_previous_baseline_is_discarded() {
  EncoderDeltaFilter filter;
  filter.reset(501, 51);
  filter.program_baseline(510, 51);
  check(filter.decode(501) == 0,
        "old gauge baseline must not become edit movement");
  check(filter.decode(510) == 0,
        "new gauge baseline settles without an edit");
}

void test_incremental_raw_counts_in_gauge_mode() {
  EncoderDeltaFilter filter;
  filter.reset(510, 51);
  check(filter.decode(511) == 1,
        "one raw clockwise count at gauge gain is one raw step");
  check(filter.decode(512) == 1,
        "successive clockwise counts remain incremental");
  check(filter.decode(511) == -1,
        "one raw counterclockwise transition is preserved");
  check(filter.decode(510) == -1,
        "second counterclockwise transition is preserved");
}

void test_user_turn_during_rebaseline_is_preserved() {
  EncoderDeltaFilter filter;
  filter.reset(501, 51);
  filter.program_baseline(510, 51);
  check(filter.decode(511) == 1,
        "turn immediately after gauge reposition uses new baseline");
  check(filter.decode(512) == 1,
        "tracking continues incrementally after early turn");
}

void test_large_gauge_reposition_does_not_look_like_rotation() {
  EncoderDeltaFilter filter;
  filter.reset(280, 9);
  filter.program_baseline(510, 9);
  check(filter.decode(280) == 0,
        "large stale baseline after mapped reposition is ignored");
  check(filter.decode(510) == 0,
        "large gauge reposition settles cleanly");
  check(filter.decode(511) == 1,
        "first real count after large reposition is accepted");
}

void test_navigation_rebaseline() {
  EncoderDeltaFilter filter;
  filter.reset(510, 9);
  filter.program_baseline(32, 1);
  check(filter.decode(510) == 0,
        "stale gauge value is ignored when returning to navigation");
  check(filter.decode(32) == 0,
        "navigation baseline settles without movement");
  check(filter.decode(33) == 1,
        "navigation turn remains one step");
}
}  // namespace

int main() {
  test_stale_previous_baseline_is_discarded();
  test_incremental_raw_counts_in_gauge_mode();
  test_user_turn_during_rebaseline_is_preserved();
  test_large_gauge_reposition_does_not_look_like_rotation();
  test_navigation_rebaseline();

  if (failures != 0) {
    std::cerr << failures << " encoder filter test(s) failed\n";
    return 1;
  }
  std::cout << "All encoder filter tests passed\n";
  return 0;
}
