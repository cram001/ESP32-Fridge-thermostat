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

void test_complete_gauge_steps_only() {
  EncoderDeltaFilter filter;
  filter.reset(510, 51);
  check(filter.decode(519) == 0,
        "sub-gain gauge residual is ignored");
  check(filter.decode(561) == 1,
        "one clockwise gain-sized movement is one step");
  check(filter.decode(459) == -1,
        "one counterclockwise gain-sized movement is one raw step");
  check(filter.decode(612) == 2,
        "two rapid clockwise movements remain two steps");
}

void test_user_turn_during_rebaseline_is_preserved() {
  EncoderDeltaFilter filter;
  filter.reset(501, 51);
  filter.program_baseline(510, 51);
  check(filter.decode(552) == 1,
        "turn from old baseline during rebaseline is preserved");

  filter.program_baseline(519, 51);
  check(filter.decode(570) == 1,
        "turn from new baseline during rebaseline is preserved");
}

void test_large_gauge_reposition_does_not_look_like_rotation() {
  EncoderDeltaFilter filter;
  filter.reset(280, 51);
  filter.program_baseline(510, 51);
  check(filter.decode(280) == 0,
        "large stale baseline after mapped gauge reposition is ignored");
  check(filter.decode(510) == 0,
        "large gauge reposition settles cleanly");
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
  test_complete_gauge_steps_only();
  test_user_turn_during_rebaseline_is_preserved();
  test_large_gauge_reposition_does_not_look_like_rotation();
  test_navigation_rebaseline_chooses_nearest_reference();

  if (failures != 0) {
    std::cerr << failures << " encoder filter test(s) failed\n";
    return 1;
  }
  std::cout << "All encoder filter tests passed\n";
  return 0;
}
