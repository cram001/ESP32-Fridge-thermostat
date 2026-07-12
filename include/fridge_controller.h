#pragma once

#include <Arduino.h>
#include <cmath>

struct ControllerSettings {
  // All internal temperatures are Celsius, regardless of the selected display
  // unit. Signal K conversion to Kelvin happens at the publishing boundary.
  float high_c = 5.0f;
  float low_c = 1.0f;
  float freezer_lockout_c = -5.0f;
  float fridge_alarm_c = 10.0f;
  float freezer_alarm_c = -2.0f;
  uint16_t fan_delay_s = 30;
  uint8_t spillover_min_on_min = 2;
  uint8_t circulation_min_on_min = 2;
  uint8_t failsafe_off_min = 15;
  bool buzzer_enabled = true;
  uint8_t oled_contrast_percent = 50;
  uint8_t display_timeout_min = 0;
};

struct ControllerOutput {
  bool spillover = false;
  bool circulation = false;
  bool sensor_fault = true;
  bool freezer_lockout = true;
};

class FridgeController {
 public:
  // Called periodically. A start condition must remain true for fan_delay_s;
  // stop thresholds and safety lockouts take effect without that delay.
  ControllerOutput update(float fridge_c, float freezer_c,
                          const ControllerSettings& settings,
                          float hysteresis_c) {
    const bool fridge_valid = valid(fridge_c);
    const bool freezer_valid = valid(freezer_c);
    // A missing freezer probe disables only freezer lockout; fridge-based
    // control remains available. A missing fridge probe is handled by the
    // duty-cycle fail-safe in main.cpp.
    output_.sensor_fault = !fridge_valid;
    output_.freezer_lockout = freezer_valid &&
                              freezer_c > settings.freezer_lockout_c;

    const uint32_t now = millis();
    if (output_.sensor_fault || output_.freezer_lockout) {
      // Fail safe: never pull warmer freezer air into the fridge when either
      // required reading is unavailable or the freezer is above its lockout.
      output_.spillover = false;
      spillover_pending_ = false;
    } else if (!output_.spillover && fridge_c >= settings.high_c) {
      // Start, or continue, the continuous-condition qualification timer.
      if (!spillover_pending_) {
        spillover_pending_ = true;
        spillover_pending_since_ = now;
      } else if (now - spillover_pending_since_ >=
                 static_cast<uint32_t>(settings.fan_delay_s) * 1000UL) {
        output_.spillover = true;
        spillover_started_at_ = now;
        spillover_pending_ = false;
      }
    } else if (output_.spillover &&
               fridge_c <= settings.high_c - hysteresis_c &&
               now - spillover_started_at_ >=
                   static_cast<uint32_t>(settings.spillover_min_on_min) *
                       60UL * 1000UL) {
      output_.spillover = false;
    } else if (!output_.spillover) {
      spillover_pending_ = false;
    }

    if (!fridge_valid) {
      output_.circulation = false;
      circulation_pending_ = false;
    } else if (!output_.circulation && fridge_c <= settings.low_c) {
      // The circulation fan has an independent qualification timer.
      if (!circulation_pending_) {
        circulation_pending_ = true;
        circulation_pending_since_ = now;
      } else if (now - circulation_pending_since_ >=
                 static_cast<uint32_t>(settings.fan_delay_s) * 1000UL) {
        output_.circulation = true;
        circulation_started_at_ = now;
        circulation_pending_ = false;
      }
    } else if (output_.circulation &&
               fridge_c >= settings.low_c + hysteresis_c &&
               now - circulation_started_at_ >=
                   static_cast<uint32_t>(settings.circulation_min_on_min) *
                       60UL * 1000UL) {
      output_.circulation = false;
    } else if (!output_.circulation) {
      circulation_pending_ = false;
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
