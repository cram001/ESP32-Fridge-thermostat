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
        "old gauge baseline must not become nine edit steps");
  check(filter.decode(510) == 0,
        "new gauge baseline settles without an edit");
}

void test_raw_counts_remain_active_in_gauge_mode() {
  EncoderDeltaFilter filter;
  filter.reset(510, 51);
  check(filter.decode(511) == 1,
        "one clockwise raw count remains one step at gain 51");
  check(filter.decode(509) == -1,
        "one counterclockwise raw count remains visible at gain 51");
  check(filter.decode(512) == 2,
        "two rapid raw counts remain two steps at gain 51");
}

void test_user_turn_during_rebaseline_is_preserved() {
  EncoderDeltaFilter filter;
  filter.reset(501, 51);
  filter.program_baseline(510, 51);
  check(filter.decode(502) == 1,
        "one-count turn from old baseline during rebaseline is preserved");

  filter.program_baseline(519, 51);
  check(filter.decode(520) == 1,
        "one-count turn from new baseline during rebaseline is preserved");
}

void test_large_gauge_reposition_does_not_look_like_rotation() {
  EncoderDeltaFilter filter;
  filter.reset(280, 51);
  filter.program_baseline(510, 51);
  check(filter.decode(280) == 0,
        "large stale baseline after mapped gauge reposition is ignored");
  check(filter.decode(510) == 0,
        "large gauge reposition settles cleanly");

  filter.program_baseline(740, 51);
  check(filter.decode(741) == 1,
        "real raw turn immediately after a large reposition is preserved");
}

void test_direction_changes_around_rebaseline() {
  EncoderDeltaFilter filter;
  filter.reset(500, 51);
  filter.program_baseline(509, 51);
  check(filter.decode(508) == -1,
        "CCW turn near new gauge baseline is preserved");

  filter.program_baseline(500, 51);
  check(filter.decode(501) == 1,
        "CW turn after reversing gauge direction is preserved");
}

void test_navigation_rebaseline_chooses_nearest_reference() {
  EncoderDeltaFilter filter;
  filter.reset(510, 51);
  filter.program_baseline(32, 1);
  check(filter.decode(510) == 0,
        "stale gauge value is ignored when returning to navigation");

  filter.program_baseline(510, 51);
  filter.program_baseline(32, 1);
  check(filter.decode(33) == 1,
        "navigation turn after mode change remains one step");
}
}  // namespace

int main() {
  test_stale_previous_baseline_is_discarded();
  test_raw_counts_remain_active_in_gauge_mode();
  test_user_turn_during_rebaseline_is_preserved();
  test_large_gauge_reposition_does_not_look_like_rotation();
  test_direction_changes_around_rebaseline();
  test_navigation_rebaseline_chooses_nearest_reference();

  if (failures != 0) {
    std::cerr << failures << " encoder filter test(s) failed\n";
    return 1;
  }
  std::cout << "All encoder filter tests passed\n";
  return 0;
}
