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

void TemperatureManager::read(const String assigned_rom[kRoleCount],
                              const float calibration_c[kRoleCount]) {
  bus_.requestTemperatures();
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
