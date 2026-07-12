#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

class TemperatureManager {
 public:
  enum class SensorStatus : uint8_t { kOk, kMissing, kOutOfRange };
  static constexpr uint8_t kRoleCount = 3;
  static constexpr uint8_t kMaxSensors = 8;

  explicit TemperatureManager(uint8_t pin);
  bool begin(String assigned_rom[kRoleCount]);

  // Non-blocking: call every loop() iteration. Internally paces itself to
  // start at most one conversion per sample_period_ms and returns true on
  // the tick a conversion finishes and role temperatures are refreshed --
  // DS18B20 conversion time (~190ms at 10-bit) is spent doing other work
  // instead of blocking the shared loop.
  bool poll(const String assigned_rom[kRoleCount],
           const float calibration_c[kRoleCount], uint32_t sample_period_ms);

  float role_temperature(uint8_t role) const { return role_temp_c_[role]; }
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
  void collect(const String assigned_rom[kRoleCount],
              const float calibration_c[kRoleCount]);

  OneWire one_wire_;
  DallasTemperature bus_;
  DeviceAddress detected_roms_[kMaxSensors];
  float detected_temp_c_[kMaxSensors] = {NAN};
  float role_temp_c_[kRoleCount] = {NAN, NAN, NAN};
  SensorStatus role_status_[kRoleCount] = {
      SensorStatus::kMissing, SensorStatus::kMissing, SensorStatus::kMissing};
  uint8_t detected_count_ = 0;
  ConversionState conversion_state_ = ConversionState::kIdle;
  uint32_t conversion_started_ms_ = 0;
  uint32_t last_request_ms_ = 0;
};
