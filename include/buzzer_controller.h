#pragma once

#include <Arduino.h>

#include "hardware_config.h"

class BuzzerController {
 public:
  explicit BuzzerController(uint8_t pin) : pin_(pin) {}

  void begin() {
    pinMode(pin_, OUTPUT);
    noTone(pin_);
  }

  void stop() {
    preview_active_ = false;
    preview_repeat_ = false;
    preview_duration_override_ms_ = 0;
    alarm_running_ = false;
    active_frequency_hz_ = 0;
    noTone(pin_);
  }

  void preview(uint8_t mode, uint32_t now) {
    start_preview(mode, now, 0, false);
  }

  // Service/output-test playback. Unlike a menu preview, the selected pattern
  // repeats for the requested duration so the physical buzzer can be verified.
  void test(uint8_t mode, uint32_t now, uint32_t duration_ms) {
    if (mode == hw::kBuzzerModeOff || mode >= hw::kBuzzerModeCount) {
      mode = hw::kDefaultBuzzerMode;
    }
    start_preview(mode, now, duration_ms, true);
  }

  void update(uint32_t now, bool alarm_should_sound, uint8_t alarm_mode) {
    if (preview_active_) {
      const uint32_t duration = preview_duration_override_ms_ != 0
                                    ? preview_duration_override_ms_
                                    : preview_duration_ms(preview_mode_);
      if (now - preview_started_ms_ < duration) {
        update_output(now - preview_started_ms_, preview_mode_,
                      !preview_repeat_);
        return;
      }
      preview_active_ = false;
      preview_repeat_ = false;
      preview_duration_override_ms_ = 0;
      active_frequency_hz_ = 0xFFFF;
    }

    if (!alarm_should_sound || alarm_mode == hw::kBuzzerModeOff ||
        alarm_mode >= hw::kBuzzerModeCount) {
      alarm_running_ = false;
      apply_frequency(0);
      return;
    }

    if (!alarm_running_ || alarm_mode_ != alarm_mode) {
      alarm_running_ = true;
      alarm_mode_ = alarm_mode;
      alarm_started_ms_ = now;
    }
    update_output(now - alarm_started_ms_, alarm_mode_, false);
  }

 private:
  void start_preview(uint8_t mode, uint32_t now, uint32_t duration_override_ms,
                     bool repeat) {
    if (mode == hw::kBuzzerModeOff || mode >= hw::kBuzzerModeCount) {
      preview_active_ = false;
      preview_repeat_ = false;
      preview_duration_override_ms_ = 0;
      noTone(pin_);
      active_frequency_hz_ = 0;
      return;
    }
    preview_active_ = true;
    preview_repeat_ = repeat;
    preview_duration_override_ms_ = duration_override_ms;
    preview_mode_ = mode;
    preview_started_ms_ = now;
    // Force the first update to apply the new tone immediately, starting at
    // phase zero regardless of system uptime.
    active_frequency_hz_ = 0xFFFF;
    update_output(0, mode, !repeat);
  }

  uint32_t preview_duration_ms(uint8_t mode) const {
    switch (mode) {
      case hw::kBuzzerModeSteady:
        return 500;
      case hw::kBuzzerModeDouble:
        return 550;
      case hw::kBuzzerModeHiLo:
        return 1200;
      case hw::kBuzzerModeTriple:
        return 690;
      default:
        return 0;
    }
  }

  uint16_t frequency_for(uint8_t mode, uint32_t elapsed_ms,
                         bool single_preview) const {
    switch (mode) {
      case hw::kBuzzerModeSteady:
        if (single_preview && elapsed_ms >= 500) return 0;
        return hw::kBuzzerSteadyFrequencyHz;

      case hw::kBuzzerModeDouble: {
        const uint32_t cycle = single_preview ? elapsed_ms
                                              : elapsed_ms % 2000UL;
        if (cycle < 200) return hw::kBuzzerSteadyFrequencyHz;
        if (cycle < 350) return 0;
        if (cycle < 550) return hw::kBuzzerSteadyFrequencyHz;
        return 0;
      }

      case hw::kBuzzerModeHiLo: {
        const uint32_t cycle = elapsed_ms % 600UL;
        return cycle < 300 ? hw::kBuzzerHighFrequencyHz
                           : hw::kBuzzerLowFrequencyHz;
      }

      case hw::kBuzzerModeTriple: {
        const uint32_t cycle = single_preview ? elapsed_ms
                                              : elapsed_ms % 2000UL;
        if (cycle < 150) return hw::kBuzzerHighFrequencyHz;
        if (cycle < 270) return 0;
        if (cycle < 420) return hw::kBuzzerHighFrequencyHz;
        if (cycle < 540) return 0;
        if (cycle < 690) return hw::kBuzzerHighFrequencyHz;
        return 0;
      }

      default:
        return 0;
    }
  }

  void update_output(uint32_t elapsed_ms, uint8_t mode, bool single_preview) {
    apply_frequency(frequency_for(mode, elapsed_ms, single_preview));
  }

  void apply_frequency(uint16_t frequency_hz) {
    if (frequency_hz == active_frequency_hz_) return;
    active_frequency_hz_ = frequency_hz;
    if (frequency_hz == 0) {
      noTone(pin_);
    } else {
      tone(pin_, frequency_hz);
    }
  }

  uint8_t pin_;
  bool preview_active_ = false;
  bool preview_repeat_ = false;
  uint32_t preview_duration_override_ms_ = 0;
  uint8_t preview_mode_ = hw::kBuzzerModeOff;
  uint32_t preview_started_ms_ = 0;
  bool alarm_running_ = false;
  uint8_t alarm_mode_ = hw::kBuzzerModeOff;
  uint32_t alarm_started_ms_ = 0;
  uint16_t active_frequency_hz_ = 0;
};
