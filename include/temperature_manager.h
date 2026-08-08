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

  // Non-blocking: call every loop() iteration. Internally paces itself to
  // start at most one conversion per sample_period_ms and returns true on
  // the tick a conversion finishes and role temperatures are refreshed --
  // DS18B20 conversion time (~190ms at 10-bit) is spent doing other work
  // instead of blocking the shared loop.
  bool poll(const String assigned_rom[kRoleCount],
           const float calibration_c[kRoleCount], uint32_t sample_period_ms);

  float role_temperature(uint8_t role) const { return role_temp_c_[role]; }
  float role_raw_temperature(uint8_t role) const {
    return role_raw_temp_c_[role];
  }
  SensorStatus role_status(uint8_t role) const { return role_status_[role]; }
  uint8_t detected_count() const { return detected_count_; }
  float detected_temperature(uint8_t sensor) const {
    return detected_temp_c_[sensor];
  }
  String detected_rom(uint8_t sensor) const;

 private:
  // 10-bit resolution converts in ~187.5ms per the DS18B20 datasheet;
  // rounded up for margin.
  static constexpr uint32_t kConversionDelayMs = 190;
  enum class ConversionState : uint8_t { kIdle, kWaiting };

  static String rom_to_string(const DeviceAddress rom);
  // Fills a caller-owned 17-byte buffer with the hex ROM string. Used on the
  // ~5s poll hot path so matching a role to a detected sensor never
  // allocates on the heap -- important on a long-running embedded device
  // where repeated small String allocations can fragment the heap over
  // weeks/months of uptime.
  static void rom_to_chars(const DeviceAddress rom, char out[17]);
  void reset_filter(uint8_t role);
  void add_filter_sample(uint8_t role, float value);
  void collect(const String assigned_rom[kRoleCount],
              const float calibration_c[kRoleCount]);

  OneWire one_wire_;
  DallasTemperature bus_;
  DeviceAddress detected_roms_[kMaxSensors];
  float detected_temp_c_[kMaxSensors] = {NAN};
  float role_raw_temp_c_[kRoleCount] = {NAN, NAN, NAN};
  float role_temp_c_[kRoleCount] = {NAN, NAN, NAN};
  float filter_history_c_[kRoleCount][hw::kTemperatureFilterSamples] = {};
  float filter_sum_c_[kRoleCount] = {};
  uint8_t filter_index_[kRoleCount] = {};
  uint8_t filter_count_[kRoleCount] = {};
  String filter_rom_[kRoleCount];
  float filter_calibration_c_[kRoleCount] = {NAN, NAN, NAN};
  SensorStatus role_status_[kRoleCount] = {
      SensorStatus::kMissing, SensorStatus::kMissing, SensorStatus::kMissing};
  uint8_t detected_count_ = 0;
  ConversionState conversion_state_ = ConversionState::kIdle;
  uint32_t conversion_started_ms_ = 0;
  uint32_t last_request_ms_ = 0;
};
