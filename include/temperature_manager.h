#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#include "hardware_config.h"

class TemperatureManager {
 public:
  enum class SensorStatus : uint8_t { kOk, kMissing, kOutOfRange };
  static constexpr uint8_t kRoleCount = 3;
  static constexpr uint8_t kMaxSensors = 8;

  explicit TemperatureManager(uint8_t pin);
  void begin();

  // Non-blocking: call every loop() iteration. Internally paces conversions and
  // periodically re-scans the OneWire bus so removed/reconnected/replacement
  // probes recover without rebooting.
  bool poll(const String assigned_rom[kRoleCount],
            const float calibration_c[kRoleCount], uint32_t sample_period_ms);

  float role_temperature(uint8_t role) const { return role_temp_c_[role]; }
  float role_raw_temperature(uint8_t role) const {
    return role_raw_temp_c_[role];
  }
  SensorStatus role_status(uint8_t role) const { return role_status_[role]; }
  uint8_t detected_count() const { return detected_count_; }
  float detected_temperature(uint8_t sensor) const {
    return sensor < detected_count_ ? detected_temp_c_[sensor] : NAN;
  }

  // Caller-owned fixed buffer; safe for periodic display/metadata paths.
  void detected_rom_chars(uint8_t sensor, char out[17]) const;

  // Returns true once for each committed discovery-list change.
  bool take_discovery_changed();

 private:
  // 10-bit resolution converts in ~187.5ms per the DS18B20 datasheet;
  // rounded up for margin.
  static constexpr uint32_t kConversionDelayMs = 190;
  enum class ConversionState : uint8_t { kIdle, kWaiting };

  static void rom_to_chars(const DeviceAddress rom, char out[17]);
  static int compare_rom(const DeviceAddress a, const DeviceAddress b);
  static bool same_rom(const DeviceAddress a, const DeviceAddress b);
  static void copy_rom(DeviceAddress dest, const DeviceAddress src);

  bool scan_sensors(uint32_t now, bool force);
  bool discovery_matches(const DeviceAddress list[kMaxSensors],
                         uint8_t count) const;
  bool candidate_matches(const DeviceAddress list[kMaxSensors],
                          uint8_t count) const;
  void apply_discovery(const DeviceAddress list[kMaxSensors], uint8_t count);

  void reset_filter(uint8_t role);
  void add_filter_sample(uint8_t role, float value);
  void collect(const String assigned_rom[kRoleCount],
               const float calibration_c[kRoleCount]);

  OneWire one_wire_;
  DallasTemperature bus_;
  DeviceAddress detected_roms_[kMaxSensors] = {};
  float detected_temp_c_[kMaxSensors] = {NAN};
  float role_raw_temp_c_[kRoleCount] = {NAN, NAN, NAN};
  float role_temp_c_[kRoleCount] = {NAN, NAN, NAN};
  float filter_history_c_[kRoleCount][hw::kTemperatureFilterSamples] = {};
  float filter_sum_c_[kRoleCount] = {};
  uint8_t filter_index_[kRoleCount] = {};
  uint8_t filter_count_[kRoleCount] = {};
  char filter_rom_[kRoleCount][17] = {};
  float filter_calibration_c_[kRoleCount] = {NAN, NAN, NAN};
  SensorStatus role_status_[kRoleCount] = {
      SensorStatus::kMissing, SensorStatus::kMissing, SensorStatus::kMissing};

  uint8_t detected_count_ = 0;
  DeviceAddress discovery_candidate_[kMaxSensors] = {};
  uint8_t discovery_candidate_count_ = 0;
  uint8_t discovery_candidate_confirmations_ = 0;
  uint32_t last_discovery_scan_ms_ = 0;
  bool discovery_changed_ = false;

  ConversionState conversion_state_ = ConversionState::kIdle;
  uint32_t conversion_started_ms_ = 0;
  uint32_t last_request_ms_ = 0;
};
