#pragma once

#include <Arduino.h>
#include <cmath>

#include "hardware_config.h"

struct ControllerSettings {
  // All internal temperatures are Celsius, regardless of the selected display
  // unit. Signal K conversion to Kelvin happens at the publishing boundary.
  float high_c = 5.0f;
  float low_c = 4.0f;
  float freezer_lockout_c = -5.0f;
  float fridge_alarm_c = 10.0f;
  float freezer_alarm_c = -2.0f;
  uint16_t fan_delay_s = 15;
  uint8_t spillover_min_on_min = 2;
  uint8_t circulation_min_on_min = 2;
  // Explicit get-me-home mode used only when the fridge probe has failed.
  // Zero keeps the spillover fan off; non-zero values are minutes ON per hour.
  uint8_t emergency_spillover_on_min = 0;
  // 0 OFF, 1 STEADY, 2 DOUBLE, 3 HI-LO, 4 TRIPLE.
  uint8_t buzzer_mode = hw::kDefaultBuzzerMode;
  uint8_t oled_contrast_percent = 50;
  uint8_t display_timeout_min = 0;
  // Default preserves the original layout: freezer left, fridge right.
  bool fridge_on_left = false;
};

inline bool IsAllowedSettingOption(uint8_t value, const uint8_t* options,
                                   uint8_t option_count) {
  for (uint8_t index = 0; index < option_count; ++index) {
    if (value == options[index]) return true;
  }
  return false;
}

// Apply the authoritative runtime limits to settings received from any source:
// saved configuration, the SensESP web UI, or the physical rotary UI.
inline void NormalizeControllerSettings(ControllerSettings& settings) {
  const ControllerSettings defaults;
  if (!std::isfinite(settings.high_c)) settings.high_c = defaults.high_c;
  if (!std::isfinite(settings.low_c)) settings.low_c = defaults.low_c;
  if (!std::isfinite(settings.freezer_lockout_c)) {
    settings.freezer_lockout_c = defaults.freezer_lockout_c;
  }
  if (!std::isfinite(settings.fridge_alarm_c)) {
    settings.fridge_alarm_c = defaults.fridge_alarm_c;
  }
  if (!std::isfinite(settings.freezer_alarm_c)) {
    settings.freezer_alarm_c = defaults.freezer_alarm_c;
  }

  settings.high_c = constrain(
      settings.high_c,
      hw::kFridgeControlMinC + hw::kFridgeControlMinimumBandC,
      hw::kFridgeControlMaxC);
  settings.low_c =
      constrain(settings.low_c, hw::kFridgeControlMinC,
                settings.high_c - hw::kFridgeControlMinimumBandC);
  settings.freezer_lockout_c =
      constrain(settings.freezer_lockout_c, hw::kFreezerThresholdMinC,
                hw::kFreezerThresholdMaxC);
  settings.fridge_alarm_c =
      constrain(settings.fridge_alarm_c, hw::kFridgeAlarmMinC,
                hw::kFridgeAlarmMaxC);
  settings.freezer_alarm_c =
      constrain(settings.freezer_alarm_c, hw::kFreezerAlarmMinC,
                hw::kFreezerAlarmMaxC);
  settings.fan_delay_s =
      constrain(settings.fan_delay_s, hw::kFanDelayMinS, hw::kFanDelayMaxS);
  settings.spillover_min_on_min =
      constrain(settings.spillover_min_on_min, hw::kFanMinimumOnMin,
                hw::kFanMinimumOnMax);
  settings.circulation_min_on_min =
      constrain(settings.circulation_min_on_min, hw::kFanMinimumOnMin,
                hw::kFanMinimumOnMax);

  if (!IsAllowedSettingOption(
          settings.emergency_spillover_on_min,
          hw::kEmergencySpilloverOptions,
          hw::kEmergencySpilloverOptionCount)) {
    settings.emergency_spillover_on_min =
        defaults.emergency_spillover_on_min;
  }
  if (settings.buzzer_mode >= hw::kBuzzerModeCount) {
    settings.buzzer_mode = defaults.buzzer_mode;
  }
  if (!IsAllowedSettingOption(settings.oled_contrast_percent,
                              hw::kOledContrastOptions,
                              hw::kOledContrastOptionCount)) {
    settings.oled_contrast_percent = defaults.oled_contrast_percent;
  }
  if (!IsAllowedSettingOption(settings.display_timeout_min,
                              hw::kDisplayTimeoutOptions,
                              hw::kDisplayTimeoutOptionCount)) {
    settings.display_timeout_min = defaults.display_timeout_min;
  }
}

struct ControllerOutput {
  bool spillover = false;
  bool circulation = false;
  bool sensor_fault = true;
  bool freezer_lockout = true;
};

class FridgeController {
 public:
  // Called periodically. Start qualification advances only on fresh sensor
  // samples, preventing one stored value from satisfying fan_delay_s.
  ControllerOutput update(float fridge_c, float freezer_c,
                          const ControllerSettings& settings,
                          bool fresh_sample = true) {
    const bool fridge_valid = valid(fridge_c);
    const bool freezer_valid = valid(freezer_c);
    // A missing freezer probe disables only freezer lockout; fridge-based
    // control remains available. A missing fridge probe is handled by the
    // duty-cycle fail-safe in main.cpp.
    output_.sensor_fault = !fridge_valid;
    output_.freezer_lockout = freezer_valid &&
                              freezer_c >= settings.freezer_lockout_c;

    const uint32_t now = millis();
    if (output_.sensor_fault || output_.freezer_lockout) {
      // A fridge fault disables normal thermostat control. A valid freezer
      // reading at its lockout threshold immediately overrides minimum runtime.
      output_.spillover = false;
      spillover_pending_ = false;
    } else if (!output_.spillover && fridge_c >= settings.high_c) {
      // Start, or continue, the continuous-condition qualification timer.
      if (fresh_sample && !spillover_pending_) {
        spillover_pending_ = true;
        spillover_pending_since_ = now;
      } else if (fresh_sample && now - spillover_pending_since_ >=
                 static_cast<uint32_t>(settings.fan_delay_s) * 1000UL) {
        output_.spillover = true;
        spillover_started_at_ = now;
        spillover_pending_ = false;
      }
    } else if (output_.spillover &&
               fridge_c <= settings.low_c &&
               now - spillover_started_at_ >=
                   static_cast<uint32_t>(settings.spillover_min_on_min) *
                       60UL * 1000UL) {
      output_.spillover = false;
    } else if (!output_.spillover && fresh_sample) {
      spillover_pending_ = false;
    }

    if (!fridge_valid) {
      output_.circulation = false;
      circulation_pending_ = false;
    } else {
      const bool cold_mix_requested = fridge_c <= settings.low_c;
      const bool circulation_requested =
          output_.spillover || cold_mix_requested;
      if (!output_.circulation && output_.spillover) {
        // Circulation follows spillover immediately so incoming freezer air is
        // mixed throughout the fridge compartment.
        output_.circulation = true;
        circulation_started_at_ = now;
        circulation_pending_ = false;
      } else if (!output_.circulation && cold_mix_requested) {
        if (fresh_sample && !circulation_pending_) {
          circulation_pending_ = true;
          circulation_pending_since_ = now;
        } else if (fresh_sample &&
                   now - circulation_pending_since_ >=
                       static_cast<uint32_t>(settings.fan_delay_s) * 1000UL) {
          output_.circulation = true;
          circulation_started_at_ = now;
          circulation_pending_ = false;
        }
      } else if (output_.circulation && !circulation_requested &&
                 now - circulation_started_at_ >=
                     static_cast<uint32_t>(
                         settings.circulation_min_on_min) *
                         60UL * 1000UL) {
        output_.circulation = false;
      } else if (!output_.circulation && fresh_sample) {
        circulation_pending_ = false;
      }
    }
    return output_;
  }

 private:
  static bool valid(float value) {
    return std::isfinite(value) && value >= -55.0f && value <= 85.0f;
  }
  ControllerOutput output_;
  bool spillover_pending_ = false;
  bool circulation_pending_ = false;
  uint32_t spillover_pending_since_ = 0;
  uint32_t circulation_pending_since_ = 0;
  uint32_t spillover_started_at_ = 0;
  uint32_t circulation_started_at_ = 0;
};

// Pure timing helper for the explicit fridge-probe-failure mode. The selected
// duty cycle starts immediately when it is enabled or changed, then repeats on
// a fixed one-hour period. A missing freezer probe is intentionally permissive;
// a valid warm-freezer reading still enforces the normal lockout.
class EmergencySpilloverController {
 public:
  bool update(uint32_t now, bool fridge_probe_failed, bool freezer_valid,
              float freezer_c, const ControllerSettings& settings) {
    if (!fridge_probe_failed) {
      active_ = false;
      return false;
    }
    if (!active_ ||
        applied_on_min_ != settings.emergency_spillover_on_min) {
      active_ = true;
      cycle_started_ms_ = now;
      applied_on_min_ = settings.emergency_spillover_on_min;
    }
    const bool freezer_allows_spillover =
        !freezer_valid || freezer_c < settings.freezer_lockout_c;
    const uint32_t on_ms = static_cast<uint32_t>(applied_on_min_) *
                           60UL * 1000UL;
    constexpr uint32_t kHourMs = 60UL * 60UL * 1000UL;
    return freezer_allows_spillover && on_ms != 0 &&
           (now - cycle_started_ms_) % kHourMs < on_ms;
  }

 private:
  bool active_ = false;
  uint8_t applied_on_min_ = 0;
  uint32_t cycle_started_ms_ = 0;
};
