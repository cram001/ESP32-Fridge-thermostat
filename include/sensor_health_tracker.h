#pragma once

#include <cstdint>

// Fixed-state helper for unattended sensor input validation. A small number of
// transient read failures are tolerated while a recent known-good sample is
// retained. The tracker never allocates heap memory and all elapsed-time checks
// are uint32_t subtraction, so they remain correct across millis() rollover.
class SensorHealthTracker {
 public:
  void reset() {
    has_good_sample_ = false;
    consecutive_failures_ = 0;
    last_good_ms_ = 0;
  }

  void note_good(uint32_t now) {
    has_good_sample_ = true;
    consecutive_failures_ = 0;
    last_good_ms_ = now;
  }

  bool note_failure(uint32_t now, uint8_t failure_limit,
                    uint32_t freshness_timeout_ms) {
    if (consecutive_failures_ < 255) ++consecutive_failures_;
    return !usable(now, failure_limit, freshness_timeout_ms);
  }

  bool usable(uint32_t now, uint8_t failure_limit,
              uint32_t freshness_timeout_ms) const {
    if (!has_good_sample_) return false;
    if (consecutive_failures_ >= failure_limit) return false;
    return now - last_good_ms_ < freshness_timeout_ms;
  }

  uint8_t consecutive_failures() const { return consecutive_failures_; }
  uint32_t last_good_ms() const { return last_good_ms_; }
  bool has_good_sample() const { return has_good_sample_; }

 private:
  bool has_good_sample_ = false;
  uint8_t consecutive_failures_ = 0;
  uint32_t last_good_ms_ = 0;
};
