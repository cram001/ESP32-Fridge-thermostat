from pathlib import Path
import re

# hardware_config.h
p = Path('include/hardware_config.h')
s = p.read_text()
old = '''constexpr uint32_t kSensorRescanIntervalMs = 30UL * 1000UL;
constexpr uint8_t kSensorDiscoveryConfirmations = 2;
// Advisory fault when a valid probe reports no meaningful raw-temperature
// change for a prolonged period. 10-bit DS18B20 readings are quantized to
// 0.25 C, so 0.20 C cleanly recognizes a one-count change.
constexpr uint32_t kTemperatureStuckTimeoutMs = 30UL * 60UL * 1000UL;
constexpr float kTemperatureStuckChangeC = 0.20f;
'''
new = '''constexpr uint32_t kSensorRescanIntervalMs = 5UL * 1000UL;
constexpr uint8_t kSensorDiscoveryConfirmations = 2;
// Sensor input is considered healthy only while recent CRC-valid samples keep
// arriving. Two transient failures are tolerated; the third failed 5-second
// sample, or 15 seconds without a good sample, marks the role read-failed.
constexpr uint8_t kSensorReadFailureLimit = 3;
constexpr uint32_t kSensorFreshnessTimeoutMs = 15UL * 1000UL;
'''
if old not in s:
    raise SystemExit('hardware sensor constants block not found')
s = s.replace(old, new, 1)
if 'v0.12.8' not in s:
    raise SystemExit('expected v0.12.8 not found')
s = s.replace('v0.12.8', 'v0.12.9', 1)
p.write_text(s)

# fault manager
p = Path('include/fault_manager.h')
s = p.read_text()
old = '''  kFridgeStuck,
  kFreezerStuck,
  kAmbientStuck
'''
new = '''  kFridgeReadFailed,
  kFreezerReadFailed,
  kAmbientReadFailed
'''
if old not in s:
    raise SystemExit('fault enum stuck block not found')
p.write_text(s.replace(old, new, 1))

p = Path('src/fault_manager.cpp')
s = p.read_text()
old = '''    {FaultCode::kFridgeStuck, "Fridge temp not changing"},
    {FaultCode::kFreezerStuck, "Freezer temp not changing"},
    {FaultCode::kAmbientStuck, "Ambient temp not changing"},
'''
new = '''    {FaultCode::kFridgeReadFailed, "Fridge sensor read failed"},
    {FaultCode::kFreezerReadFailed, "Freezer sensor read failed"},
    {FaultCode::kAmbientReadFailed, "Ambient sensor read failed"},
'''
if old not in s:
    raise SystemExit('fault message stuck block not found')
p.write_text(s.replace(old, new, 1))

# TemperatureManager header
p = Path('include/temperature_manager.h')
s = p.read_text()
if '#include "sensor_health_tracker.h"' not in s:
    s = s.replace('#include "hardware_config.h"\n', '#include "hardware_config.h"\n#include "sensor_health_tracker.h"\n', 1)
s = s.replace('enum class SensorStatus : uint8_t { kOk, kMissing, kOutOfRange };',
              'enum class SensorStatus : uint8_t { kOk, kMissing, kReadFailed, kOutOfRange };', 1)
s = s.replace('''  void collect(const String assigned_rom[kRoleCount],
               const float calibration_c[kRoleCount]);''',
              '''  void collect(const String assigned_rom[kRoleCount],
               const float calibration_c[kRoleCount], uint32_t now);''', 1)
marker = '''  SensorStatus role_status_[kRoleCount] = {
      SensorStatus::kMissing, SensorStatus::kMissing, SensorStatus::kMissing};
'''
if marker not in s:
    raise SystemExit('temperature manager status marker not found')
s = s.replace(marker, marker + '  SensorHealthTracker role_health_[kRoleCount];\n', 1)
p.write_text(s)

# TemperatureManager implementation
p = Path('src/temperature_manager.cpp')
s = p.read_text()
old_apply = '''  if (changed) discovery_changed_ = true;
}
'''
new_apply = '''  if (changed) {
    // Newly connected DS18B20s power up at 12-bit resolution. Set each
    // committed address to 10-bit so the asynchronous 190 ms conversion wait
    // remains valid after hot-plug/reconnect as well as at cold boot.
    for (uint8_t i = 0; i < detected_count_; ++i) {
      bus_.setResolution(detected_roms_[i], 10, true);
    }
    discovery_changed_ = true;
  }
}
'''
if old_apply not in s:
    raise SystemExit('apply_discovery tail not found')
s = s.replace(old_apply, new_apply, 1)

m = re.search(r'bool TemperatureManager::scan_sensors\(uint32_t now, bool force\) \{.*?\n\}\n\nvoid TemperatureManager::begin\(\)', s, re.S)
if not m:
    raise SystemExit('scan_sensors block not found')
scan = '''bool TemperatureManager::scan_sensors(uint32_t now, bool force) {
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

void TemperatureManager::begin()'''
s = s[:m.start()] + scan + s[m.end():]

if '  collect(assigned_rom, calibration_c);\n' not in s:
    raise SystemExit('collect call not found')
s = s.replace('  collect(assigned_rom, calibration_c);\n', '  collect(assigned_rom, calibration_c, now);\n', 1)

start = s.index('void TemperatureManager::collect(const String assigned_rom[kRoleCount],')
collect = '''void TemperatureManager::collect(const String assigned_rom[kRoleCount],
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
'''
s = s[:start] + collect
old_reset = '  role_temp_c_[role] = NAN;\n}\n'
new_reset = '  role_temp_c_[role] = NAN;\n  role_raw_temp_c_[role] = NAN;\n}\n'
if old_reset not in s:
    raise SystemExit('reset_filter tail not found')
s = s.replace(old_reset, new_reset, 1)
p.write_text(s)

# main.cpp
p = Path('src/main.cpp')
s = p.read_text()
s, n = re.subn(r'\n// Advisory stuck-value monitor\..*?bool temperature_stuck\[3\] = \{false, false, false\};\n', '\n', s, count=1, flags=re.S)
if n != 1:
    raise SystemExit('stuck monitor globals not found')
s, n = re.subn(r'\nvoid update_temperature_stuck_monitor\(uint32_t now\) \{.*?\n\}\n\nvoid publish_detected_probe_metadata', '\nvoid publish_detected_probe_metadata', s, count=1, flags=re.S)
if n != 1:
    raise SystemExit('stuck monitor function not found')
s = s.replace('  const uint32_t now = millis();\n  fridge_c = temperatures.role_temperature(0);', '  fridge_c = temperatures.role_temperature(0);', 1)
s = s.replace('  update_temperature_stuck_monitor(now);\n', '', 1)
old = '''  faults.set(FaultCode::kFridgeStuck,
             fridge_status == Status::kOk && temperature_stuck[0]);
  faults.set(FaultCode::kFreezerStuck,
             freezer_status == Status::kOk && temperature_stuck[1]);
  faults.set(FaultCode::kAmbientStuck,
             ambient_status == Status::kOk && temperature_stuck[2]);
'''
new = '''  faults.set(FaultCode::kFridgeReadFailed,
             fridge_status == Status::kReadFailed);
  faults.set(FaultCode::kFreezerReadFailed,
             freezer_status == Status::kReadFailed);
  faults.set(FaultCode::kAmbientReadFailed,
             ambient_status == Status::kReadFailed);
'''
if old not in s:
    raise SystemExit('main stuck fault block not found')
s = s.replace(old, new, 1)
p.write_text(s)

# Native runner
p = Path('tools/run-native-tests.sh')
s = p.read_text()
if 'sensor_health_tests.cpp' not in s:
    s += 'compile_and_run sensor_health_tests.cpp sensor-health-tests\n'
p.write_text(s)

# Failure-mode tests
p = Path('test/native/failure_mode_tests.cpp')
s = p.read_text()
s, n = re.subn(r'\nstruct StuckTemperatureMonitor \{.*?\n\};\n', '\n', s, count=1, flags=re.S)
if n != 1:
    raise SystemExit('StuckTemperatureMonitor not found')
s, n = re.subn(r'\nvoid test_stuck_temperature_advisory_timing\(\) \{.*?\n\}\n', '\n', s, count=1, flags=re.S)
if n != 1:
    raise SystemExit('stuck advisory test not found')
s, n = re.subn(r'void test_compressor_failure_with_stuck_cold_freezer_probe\(\) \{.*?\n\}\n', '''void test_compressor_failure_with_stuck_cold_freezer_probe() {
  FirmwareSim sim;
  const float reported_freezer_c = -10.0f;
  for (uint32_t minute = 0; minute <= 35; ++minute) {
    const uint32_t now = 1000 + minute * 60UL * 1000UL;
    sim.step(now, 8.0f, reported_freezer_c, true, true);
  }

  limitation(!sim.output.freezer_lockout && !sim.temperature_alarm,
             "a CRC-valid, plausible but physically wrong freezer reading cannot be distinguished from a genuinely stable freezer without independent feedback");
}
''', s, count=1, flags=re.S)
if n != 1:
    raise SystemExit('stuck-cold compressor test not found')
s = s.replace('  test_stuck_temperature_advisory_timing();\n', '', 1)
p.write_text(s)

# Failure-mode docs
p = Path('docs/FAILURE_MODE_ANALYSIS.md')
s = p.read_text()
s = s.replace('| Valid probe unchanged ~30 min | `temp not changing` advisory | User is warned | Reading remains usable for control because it may still be physically valid |\n', '')
s = s.replace('| Compressor failure + freezer probe stuck plausibly cold | 30-minute unchanged-temperature advisory | User is warned about suspicious probe | **Cold false reading can mask freezer warming until independent observation** |\n', '| Compressor failure + freezer probe stuck plausibly cold | Reading remains apparently valid if communication/CRC are good | None from temperature alone | **Cold false reading can mask freezer warming until independent observation** |\n')
old = '''Ambient temperature is informational and does not participate in refrigeration
control. Missing/range/stuck conditions should therefore remain advisory faults
rather than changing fan behavior.

### Plausible but frozen sensor value

A disconnected DS18B20 is normally reported as invalid, but a sensor or data
path can theoretically remain at one plausible value. The firmware therefore
raises an advisory if a valid raw reading fails to move by one 10-bit DS18B20
measurement step for approximately 30 minutes.

The advisory does not automatically reject the reading. A compartment can
legitimately remain extremely stable, so treating this as a hard sensor failure
would create false fail-safe transitions. For the fridge or freezer probe, the
warning triangle and Active Errors entry tell the user to investigate.
'''
new = '''Ambient temperature is informational and does not participate in refrigeration
control. Missing, read-failed, and range conditions remain advisory faults rather
than changing fan behavior. A stable cabin temperature is not a fault: repeated
CRC-valid samples remain healthy even if the numeric value is unchanged for hours.

### Sensor input validity and freshness

Each assigned role is healthy only while its ROM is present and recent valid
samples continue to arrive. DallasTemperature validates the scratchpad CRC; the
firmware also rejects the DS18B20 +85 C power-on value and implausible range. Two
transient failed 5-second reads are tolerated using the most recent good sample.
A third failed read, or 15 seconds without a good sample, changes the role to
`read failed`. One subsequent good sample recovers automatically.

Numeric movement is deliberately not a validity requirement. A refrigerator,
freezer, or cabin can legitimately remain on the same 0.25 C 10-bit reading for
hours, so an unchanged-value alarm produces false positives and is not used.
'''
if old not in s:
    raise SystemExit('failure doc ambient/stuck section not found')
s = s.replace(old, new, 1)
old = '''A plausible cold reading can prevent lockout and freezer high-temperature alarm
even while the real freezer warms. The 30-minute unchanged-temperature advisory
is the only available software indication until another measured quantity
changes. Independent compressor/fan/current/temperature feedback would be
required to make this condition fully deterministic.
'''
new = '''A plausible cold reading can prevent lockout and freezer high-temperature alarm
even while the real freezer warms. If the sensor continues returning CRC-valid
data, software cannot reliably distinguish a physically stuck or misplaced probe
from a genuinely stable freezer. Independent compressor/fan/current/temperature
feedback would be required to make this condition fully deterministic.
'''
if old not in s:
    raise SystemExit('failure doc stuck-cold section not found')
s = s.replace(old, new, 1)
p.write_text(s)

print('Sensor reliability patch applied successfully')
