#pragma once

#include <cstdint>
#include <cstdlib>

// Filters SEN0502 encoder reads when firmware reprograms the encoder value to
// reposition the LED gauge. The module may briefly return the previous value
// after setEncoderValue(); those stale readbacks must never be interpreted as
// user rotation.
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
        // Exact stale readback from before the firmware reposition.
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

      if (gain_ <= 1) {
        // During a navigation-mode rebaseline, choose the nearer reference.
        // This preserves a genuine turn that occurs before the new baseline is
        // observed while rejecting a large stale jump from the other mode.
        rebaseline_pending_ = false;
        return std::abs(from_previous) <= std::abs(from_current)
                   ? from_previous
                   : from_current;
      }

      // A user may rotate before the reprogrammed gauge baseline has been
      // observed. Accept only complete gain-sized movements from either the
      // old or new baseline; ambiguous residuals are discarded.
      if (is_complete_step(from_previous)) {
        rebaseline_pending_ = false;
        return from_previous / static_cast<int32_t>(gain_);
      }
      if (is_complete_step(from_current)) {
        rebaseline_pending_ = false;
        return from_current / static_cast<int32_t>(gain_);
      }
      return 0;
    }

    const int32_t delta = position - static_cast<int32_t>(baseline_);
    if (gain_ <= 1) return delta;
    if (!is_complete_step(delta)) return 0;
    return delta / static_cast<int32_t>(gain_);
  }

 private:
  bool is_complete_step(int32_t delta) const {
    if (delta == 0) return false;
    const int32_t gain = static_cast<int32_t>(gain_);
    return std::abs(delta) >= gain && delta % gain == 0;
  }

  uint16_t baseline_ = 0;
  uint16_t previous_baseline_ = 0;
  uint8_t gain_ = 1;
  bool rebaseline_pending_ = false;
  bool initialized_ = false;
};
