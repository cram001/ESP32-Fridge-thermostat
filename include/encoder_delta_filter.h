#pragma once

#include <cstdint>
#include <cstdlib>

// Filters SEN0502 encoder reads when firmware reprograms the encoder value to
// reposition the LED gauge. The gain coefficient changes the visual LED
// response; it does not mean getEncoderValue() moves by `gain` counts for one
// physical detent. Rotary input therefore remains raw small count changes.
//
// After setEncoderValue(), the module may briefly return the previous value.
// During that one rebaseline transition, decode relative to whichever of the
// old or new programmed baselines is nearest. This rejects our own gauge jump
// while preserving a real turn that occurs before the new baseline is read.
class EncoderDeltaFilter {
 public:
  void reset(uint16_t baseline, uint8_t gain) {
    baseline_ = baseline;
    previous_baseline_ = baseline;
    gain_ = gain == 0 ? 1 : gain;
    rebaseline_pending_ = false;
    initialized_ = true;
  }

  void program_baseline(uint16_t baseline, uint8_t gain) {
    const uint8_t next_gain = gain == 0 ? 1 : gain;
    if (!initialized_) {
      reset(baseline, next_gain);
      return;
    }

    previous_baseline_ = baseline_;
    baseline_ = baseline;
    gain_ = next_gain;
    rebaseline_pending_ = previous_baseline_ != baseline_;
  }

  int32_t decode(int32_t position) {
    if (!initialized_) return 0;

    if (rebaseline_pending_) {
      if (position == static_cast<int32_t>(previous_baseline_)) {
        // Exact stale readback from before the firmware reposition. Keep the
        // transition pending until the new baseline (or a real nearby turn)
        // is observed.
        return 0;
      }
      if (position == static_cast<int32_t>(baseline_)) {
        rebaseline_pending_ = false;
        return 0;
      }

      const int32_t from_previous =
          position - static_cast<int32_t>(previous_baseline_);
      const int32_t from_current =
          position - static_cast<int32_t>(baseline_);
      const int32_t previous_distance = std::abs(from_previous);
      const int32_t current_distance = std::abs(from_current);

      rebaseline_pending_ = false;

      // A genuine detent occurring during the rebaseline will normally be a
      // one- or two-count change from one baseline and much farther from the
      // other. In the unlikely exact-tie case, discard the ambiguous sample
      // rather than risk a multi-step edit.
      if (previous_distance < current_distance) return from_previous;
      if (current_distance < previous_distance) return from_current;
      return 0;
    }

    return position - static_cast<int32_t>(baseline_);
  }

 private:
  uint16_t baseline_ = 0;
  uint16_t previous_baseline_ = 0;
  uint8_t gain_ = 1;
  bool rebaseline_pending_ = false;
  bool initialized_ = false;
};
