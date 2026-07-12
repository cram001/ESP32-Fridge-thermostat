// Implements asynchronous DS18B20 discovery, conversion, and role mapping.
// The cooperative poll state machine keeps OneWire conversion latency out of
// the main loop while retaining a bounded recovery path for bus failures.
#include "temperature_manager.h"

TemperatureManager::TemperatureManager(uint8_t pin)
    : one_wire_(pin), bus_(&one_wire_) {}

String TemperatureManager::rom_to_string(const DeviceAddress rom) {
  char text[17];
  for (uint8_t i = 0; i < 8; ++i) snprintf(text + i * 2, 3, "%02X", rom[i]);
  text[16] = 0;
  return String(text);
}

bool TemperatureManager::begin(String assigned_rom[kRoleCount]) {
  bus_.begin();
  bus_.setResolution(10);
  // Conversions are collected on our own schedule in poll(), so the bus
  // must not block the caller waiting for them to finish.
  bus_.setWaitForConversion(false);
  detected_count_ = min<uint8_t>(bus_.getDeviceCount(), kMaxSensors);
  bool assignments_changed = false;
  // Provide a usable first-boot mapping. The on-device assignment workflow
  // replaces these provisional index-based roles with confirmed ROM mappings.
  for (uint8_t i = 0; i < detected_count_; ++i) {
    bus_.getAddress(detected_roms_[i], i);
    if (i < kRoleCount && assigned_rom[i].isEmpty()) {
      assigned_rom[i] = rom_to_string(detected_roms_[i]);
      assignments_changed = true;
    }
  }
  return assignments_changed;
}

bool TemperatureManager::poll(const String assigned_rom[kRoleCount],
                              const float calibration_c[kRoleCount],
                              uint32_t sample_period_ms) {
  const uint32_t now = millis();
  if (conversion_state_ == ConversionState::kIdle) {
    // last_request_ms_ == 0 only before the very first sample; take it
    // immediately rather than waiting out a full period on cold boot.
    if (last_request_ms_ != 0 && now - last_request_ms_ < sample_period_ms) {
      return false;
    }
    bus_.requestTemperatures();
    conversion_started_ms_ = now;
    conversion_state_ = ConversionState::kWaiting;
    return false;
  }
  // Read as soon as the sensors report completion. The timeout keeps a
  // damaged or stuck OneWire bus from permanently blocking future polls.
  if (!bus_.isConversionComplete() &&
      now - conversion_started_ms_ < kConversionTimeoutMs) {
    return false;
  }
  collect(assigned_rom, calibration_c);
  last_request_ms_ = now;
  conversion_state_ = ConversionState::kIdle;
  return true;
}

void TemperatureManager::collect(const String assigned_rom[kRoleCount],
                                 const float calibration_c[kRoleCount]) {
  // First cache every physical probe result, then derive logical role values.
  // This also supplies live readings to the sensor-assignment screen.
  for (uint8_t i = 0; i < detected_count_; ++i) {
    const float value = bus_.getTempC(detected_roms_[i]);
    detected_temp_c_[i] = value == DEVICE_DISCONNECTED_C ? NAN : value;
  }
  for (uint8_t role = 0; role < kRoleCount; ++role) {
    // Roles follow the immutable 64-bit ROM code, so OneWire discovery order
    // cannot swap the fridge and freezer after a reboot or wiring change.
    role_temp_c_[role] = NAN;
    role_status_[role] = SensorStatus::kMissing;
    for (uint8_t sensor = 0; sensor < detected_count_; ++sensor) {
      if (assigned_rom[role].equalsIgnoreCase(
              rom_to_string(detected_roms_[sensor]))) {
        if (std::isfinite(detected_temp_c_[sensor])) {
          const float calibrated =
              detected_temp_c_[sensor] + calibration_c[role];
          if (calibrated < -55.0f || calibrated > 85.0f) {
            role_status_[role] = SensorStatus::kOutOfRange;
          } else {
            role_temp_c_[role] = calibrated;
            role_status_[role] = SensorStatus::kOk;
          }
        }
        break;
      }
    }
  }
}

String TemperatureManager::detected_rom(uint8_t sensor) const {
  return rom_to_string(detected_roms_[sensor]);
}
