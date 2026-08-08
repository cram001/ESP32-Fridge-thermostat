#pragma once

#include <cstdint>
#include <cstdlib>

// Tracks the SEN0502 count incrementally while allowing firmware to
// occasionally reposition the encoder count to place the LED ring at a mapped
// gauge position. Normal rotation is always decoded from consecutive raw
// encoder counts; gain affects the LED-ring response only and is not used as a
// divisor for user input.
class EncoderDeltaFilter {
 public:
  void reset(uint16_t baseline, uint8_t gain) {
    (void)gain;
    target_baseline_ = baseline;
    previous_position_ = baseline;
    last_position_ = baseline;
    rebaseline_pending_ = false;
    initialized_ = true;
  }

  void program_baseline(uint16_t baseline, uint8_t gain) {
    (void)gain;
    if (!initialized_) {
      reset(baseline, gain);
      return;
    }

    previous_position_ = last_position_;
    target_baseline_ = baseline;
    rebaseline_pending_ = previous_position_ != target_baseline_;
  }

  int32_t decode(int32_t position) {
    if (!initialized_) return 0;

    if (rebaseline_pending_) {
      if (position == static_cast<int32_t>(previous_position_)) {
        // The module can briefly return the count from before setEncoderValue.
        // Ignore that exact stale readback.
        return 0;
      }

      const int32_t from_target =
          position - static_cast<int32_t>(target_baseline_);
      const int32_t from_previous =
          position - static_cast<int32_t>(previous_position_);

      if (from_target == 0) {
        last_position_ = position;
        rebaseline_pending_ = false;
        return 0;
      }

      // If the user rotates before the programmed baseline is observed, use
      // whichever reference is clearly nearer. Equal-distance samples are
      // ambiguous and are discarded rather than risking a jump.
      const int32_t target_distance = std::abs(from_target);
      const int32_t previous_distance = std::abs(from_previous);
      if (target_distance == previous_distance) return 0;

      const int32_t delta =
          target_distance < previous_distance ? from_target : from_previous;
      last_position_ = position;
      rebaseline_pending_ = false;
      return delta;
    }

    const int32_t delta = position - last_position_;
    last_position_ = position;
    return delta;
  }

 private:
  uint16_t target_baseline_ = 0;
  uint16_t previous_position_ = 0;
  int32_t last_position_ = 0;
  bool rebaseline_pending_ = false;
  bool initialized_ = false;
};
