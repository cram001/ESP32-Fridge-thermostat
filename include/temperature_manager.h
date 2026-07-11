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
  void read(const String assigned_rom[kRoleCount],
            const float calibration_c[kRoleCount]);

  float role_temperature(uint8_t role) const { return role_temp_c_[role]; }
  SensorStatus role_status(uint8_t role) const { return role_status_[role]; }
  uint8_t detected_count() const { return detected_count_; }
  float detected_temperature(uint8_t sensor) const {
    return detected_temp_c_[sensor];
  }
  String detected_rom(uint8_t sensor) const;

 private:
  static String rom_to_string(const DeviceAddress rom);

  OneWire one_wire_;
  DallasTemperature bus_;
  DeviceAddress detected_roms_[kMaxSensors];
  float detected_temp_c_[kMaxSensors] = {NAN};
  float role_temp_c_[kRoleCount] = {NAN, NAN, NAN};
  SensorStatus role_status_[kRoleCount] = {
      SensorStatus::kMissing, SensorStatus::kMissing, SensorStatus::kMissing};
  uint8_t detected_count_ = 0;
};
