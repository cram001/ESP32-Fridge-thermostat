#include "temperature_manager.h"

#include <strings.h>  // strcasecmp

TemperatureManager::TemperatureManager(uint8_t pin)
    : one_wire_(pin), bus_(&one_wire_) {}

void TemperatureManager::rom_to_chars(const DeviceAddress rom, char out[17]) {
  for (uint8_t i = 0; i < 8; ++i) snprintf(out + i * 2, 3, "%02X", rom[i]);
  out[16] = 0;
}

int TemperatureManager::compare_rom(const DeviceAddress a,
                                    const DeviceAddress b) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}

bool TemperatureManager::same_rom(const DeviceAddress a,
                                  const DeviceAddress b) {
  return compare_rom(a, b) == 0;
}

void TemperatureManager::copy_rom(DeviceAddress dest,
                                  const DeviceAddress src) {
  for (uint8_t i = 0; i < 8; ++i) dest[i] = src[i];
}

void TemperatureManager::detected_rom_chars(uint8_t sensor,
                                            char out[17]) const {
  if (sensor >= detected_count_) {
    out[0] = 0;
    return;
  }
  rom_to_chars(detected_roms_[sensor], out);
}

bool TemperatureManager::take_discovery_changed() {
  const bool changed = discovery_changed_;
  discovery_changed_ = false;
  return changed;
}

void TemperatureManager::reset_filter(uint8_t role) {
  filter_sum_c_[role] = 0.0f;
  filter_index_[role] = 0;
  filter_count_[role] = 0;
  role_temp_c_[role] = NAN;
  role_raw_temp_c_[role] = NAN;
}

void TemperatureManager::add_filter_sample(uint8_t role, float value) {
  const uint8_t index = filter_index_[role];
  if (filter_count_[role] == hw::kTemperatureFilterSamples) {
    filter_sum_c_[role] -= filter_history_c_[role][index];
  } else {
    filter_count_[role]++;
  }
  filter_history_c_[role][index] = value;
  filter_sum_c_[role] += value;
  filter_index_[role] =
      (index + 1) % hw::kTemperatureFilterSamples;
  role_temp_c_[role] = filter_sum_c_[role] / filter_count_[role];
}

bool TemperatureManager::discovery_matches(
    const DeviceAddress list[kMaxSensors], uint8_t count) const {
  if (count != detected_count_) return false;
  for (uint8_t i = 0; i < count; ++i) {
    if (!same_rom(list[i], detected_roms_[i])) return false;
  }
  return true;
}

bool TemperatureManager::candidate_matches(
    const DeviceAddress list[kMaxSensors], uint8_t count) const {
  if (count != discovery_candidate_count_) return false;
  for (uint8_t i = 0; i < count; ++i) {
    if (!same_rom(list[i], discovery_candidate_[i])) return false;
  }
  return true;
}

void TemperatureManager::apply_discovery(
    const DeviceAddress list[kMaxSensors], uint8_t count) {
  const bool changed = !discovery_matches(list, count);
  detected_count_ = count;
  for (uint8_t i = 0; i < kMaxSensors; ++i) {
    detected_temp_c_[i] = NAN;
    for (uint8_t b = 0; b < 8; ++b) {
      detected_roms_[i][b] = i < count ? list[i][b] : 0;
    }
  }
  if (changed) {
    // Newly connected DS18B20s power up at 12-bit resolution. Set each
    // committed address to 10-bit so the asynchronous 190 ms conversion wait
    // remains valid after hot-plug/reconnect as well as at cold boot.
    for (uint8_t i = 0; i < detected_count_; ++i) {
      bus_.setResolution(detected_roms_[i], 10, true);
    }
    discovery_changed_ = true;
  }
}

bool TemperatureManager::scan_sensors(uint32_t now, bool force) {
  if (!force &&
      now - last_discovery_scan_ms_ < hw::kSensorRescanIntervalMs) {
    return false;
  }
  last_discovery_scan_ms_ = now;

  DeviceAddress found[kMaxSensors] = {};
  uint8_t found_count = 0;
  DeviceAddress address;

  // Perform a real OneWire ROM search every time. DallasTemperature caches its
  // device count during begin(), so its indexed discovery helpers cannot be
  // relied upon to find probes connected later. Validate the ROM CRC and only
  // accept the DS18B20 family used by this controller.
  one_wire_.reset_search();
  while (found_count < kMaxSensors && one_wire_.search(address)) {
    if (OneWire::crc8(address, 7) != address[7]) continue;
    if (address[0] != 0x28) continue;
    copy_rom(found[found_count], address);
    ++found_count;
  }
  one_wire_.reset_search();

  for (uint8_t i = 1; i < found_count; ++i) {
    DeviceAddress key;
    copy_rom(key, found[i]);
    int j = static_cast<int>(i) - 1;
    while (j >= 0 && compare_rom(found[j], key) > 0) {
      copy_rom(found[j + 1], found[j]);
      --j;
    }
    copy_rom(found[j + 1], key);
  }

  if (force) {
    apply_discovery(found, found_count);
    discovery_candidate_count_ = 0;
    discovery_candidate_confirmations_ = 0;
    return true;
  }

  if (discovery_matches(found, found_count)) {
    discovery_candidate_count_ = 0;
    discovery_candidate_confirmations_ = 0;
    return false;
  }

  if (candidate_matches(found, found_count)) {
    if (discovery_candidate_confirmations_ < 255) {
      ++discovery_candidate_confirmations_;
    }
  } else {
    discovery_candidate_count_ = found_count;
    for (uint8_t i = 0; i < found_count; ++i) {
      copy_rom(discovery_candidate_[i], found[i]);
    }
    discovery_candidate_confirmations_ = 1;
  }

  if (discovery_candidate_confirmations_ <
      hw::kSensorDiscoveryConfirmations) {
    return false;
  }

  apply_discovery(discovery_candidate_, discovery_candidate_count_);
  discovery_candidate_count_ = 0;
  discovery_candidate_confirmations_ = 0;
  return true;
}

void TemperatureManager::begin() {
  bus_.begin();
  bus_.setResolution(10);
  // Conversions are collected on our own schedule in poll(), so the bus must
  // not block the caller waiting for them to finish.
  bus_.setWaitForConversion(false);
  scan_sensors(millis(), true);
}

bool TemperatureManager::poll(const String assigned_rom[kRoleCount],
                              const float calibration_c[kRoleCount],
                              uint32_t sample_period_ms) {
  const uint32_t now = millis();
  if (conversion_state_ == ConversionState::kIdle) {
    scan_sensors(now, false);

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
  if (now - conversion_started_ms_ < kConversionDelayMs) return false;
  collect(assigned_rom, calibration_c, now);
  last_request_ms_ = now;
  conversion_state_ = ConversionState::kIdle;
  return true;
}

void TemperatureManager::collect(const String assigned_rom[kRoleCount],
                                 const float calibration_c[kRoleCount],
                                 uint32_t now) {
  for (uint8_t i = 0; i < detected_count_; ++i) {
    const float value = bus_.getTempC(detected_roms_[i]);
    if (value == DEVICE_DISCONNECTED_C || !std::isfinite(value) ||
        fabsf(value - hw::kDs18b20PowerOnResetC) < 0.01f) {
      detected_temp_c_[i] = NAN;
    } else {
      detected_temp_c_[i] = value;
    }
  }

  for (uint8_t role = 0; role < kRoleCount; ++role) {
    const char* assigned = assigned_rom[role].c_str();
    if (strcasecmp(filter_rom_[role], assigned) != 0 ||
        filter_calibration_c_[role] != calibration_c[role]) {
      reset_filter(role);
      role_health_[role].reset();
      snprintf(filter_rom_[role], sizeof(filter_rom_[role]), "%s", assigned);
      filter_calibration_c_[role] = calibration_c[role];
    }

    if (assigned[0] == 0) {
      role_status_[role] = SensorStatus::kMissing;
      role_health_[role].reset();
      reset_filter(role);
      continue;
    }

    int8_t matched_sensor = -1;
    for (uint8_t sensor = 0; sensor < detected_count_; ++sensor) {
      char sensor_rom[17];
      rom_to_chars(detected_roms_[sensor], sensor_rom);
      if (strcasecmp(assigned, sensor_rom) == 0) {
        matched_sensor = static_cast<int8_t>(sensor);
        break;
      }
    }

    if (matched_sensor < 0) {
      role_status_[role] = SensorStatus::kMissing;
      role_health_[role].reset();
      reset_filter(role);
      continue;
    }

    const float raw = detected_temp_c_[matched_sensor];
    if (!std::isfinite(raw)) {
      const bool expired = role_health_[role].note_failure(
          now, hw::kSensorReadFailureLimit, hw::kSensorFreshnessTimeoutMs);
      if (expired) {
        role_status_[role] = SensorStatus::kReadFailed;
        reset_filter(role);
      } else {
        // A brief CRC/read glitch must not destabilize control. Retain the most
        // recent known-good sample only while the explicit freshness window is
        // still valid.
        role_status_[role] = SensorStatus::kOk;
      }
      continue;
    }

    const float calibrated = raw + calibration_c[role];
    if (calibrated < -55.0f || calibrated > 85.0f) {
      role_status_[role] = SensorStatus::kOutOfRange;
      role_health_[role].reset();
      reset_filter(role);
      continue;
    }

    role_health_[role].note_good(now);
    role_raw_temp_c_[role] = calibrated;
    role_status_[role] = SensorStatus::kOk;
    add_filter_sample(role, calibrated);
  }
}
