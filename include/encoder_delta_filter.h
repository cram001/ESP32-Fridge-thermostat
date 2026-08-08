#pragma once

#include <cstdint>
#include <cstdlib>

// Tracks the SEN0502 count incrementally while allowing firmware to
// occasionally reposition the encoder count to place the LED ring at a mapped
// gauge position. Normal rotation is decoded only from consecutive count
// changes; firmware must not reprogram the baseline after every detent.
class EncoderDeltaFilter {
 public:
  void reset(uint16_t baseline, uint8_t gain) {
    target_baseline_ = baseline;
    previous_position_ = baseline;
    last_position_ = baseline;
    gain_ = gain == 0 ? 1 : gain;
    residual_counts_ = 0;
    rebaseline_pending_ = false;
    initialized_ = true;
  }

  void program_baseline(uint16_t baseline, uint8_t gain) {
    const uint8_t next_gain = gain == 0 ? 1 : gain;
    if (!initialized_) {
      reset(baseline, next_gain);
      return;
    }

    previous_position_ = last_position_;
    target_baseline_ = baseline;
    gain_ = next_gain;
    residual_counts_ = 0;
    rebaseline_pending_ = previous_position_ != target_baseline_;
  }

  int32_t decode(int32_t position) {
    if (!initialized_) return 0;

    if (rebaseline_pending_) {
      if (position == static_cast<int32_t>(previous_position_)) {
        // The module can briefly return the count from before setEncoderValue.
        return 0;
      }

      const int32_t from_target =
          position - static_cast<int32_t>(target_baseline_);
      if (from_target == 0) {
        last_position_ = position;
        rebaseline_pending_ = false;
        return 0;
      }

      const int32_t from_previous =
          position - static_cast<int32_t>(previous_position_);

      // A real turn may occur before the programmed baseline is observed.
      // Accept it only when it is an exact gain-sized movement from one of the
      // two known references; otherwise keep waiting rather than inventing a
      // large delta from an ambiguous stale sample.
      if (is_complete_gain_step(from_target)) {
        last_position_ = position;
        rebaseline_pending_ = false;
        return from_target / static_cast<int32_t>(gain_);
      }
      if (is_complete_gain_step(from_previous)) {
        last_position_ = position;
        rebaseline_pending_ = false;
        return from_previous / static_cast<int32_t>(gain_);
      }
      return 0;
    }

    const int32_t raw_delta = position - last_position_;
    last_position_ = position;
    residual_counts_ += raw_delta;

    const int32_t steps = residual_counts_ / static_cast<int32_t>(gain_);
    residual_counts_ -= steps * static_cast<int32_t>(gain_);
    return steps;
  }

 private:
  bool is_complete_gain_step(int32_t delta) const {
    if (delta == 0) return false;
    const int32_t gain = static_cast<int32_t>(gain_);
    return std::abs(delta) >= gain && delta % gain == 0;
  }

  uint16_t target_baseline_ = 0;
  uint16_t previous_position_ = 0;
  int32_t last_position_ = 0;
  int32_t residual_counts_ = 0;
  uint8_t gain_ = 1;
  bool rebaseline_pending_ = false;
  bool initialized_ = false;
};
