#include "settings_store.h"

#include <cmath>

#include "hardware_config.h"

namespace {

String JsonNumberArray(const uint8_t* values, uint8_t count) {
  String json = "[";
  for (uint8_t index = 0; index < count; ++index) {
    if (index != 0) json += ",";
    json += String(values[index]);
  }
  json += "]";
  return json;
}

bool LoadedControllerSettingsValid(const ControllerSettings& settings) {
  if (!std::isfinite(settings.high_c) || !std::isfinite(settings.low_c) ||
      !std::isfinite(settings.freezer_lockout_c) ||
      !std::isfinite(settings.fridge_alarm_c) ||
      !std::isfinite(settings.freezer_alarm_c)) {
    return false;
  }
  if (settings.high_c < hw::kFridgeControlMinC +
                            hw::kFridgeControlMinimumBandC ||
      settings.high_c > hw::kFridgeControlMaxC) {
    return false;
  }
  if (settings.low_c < hw::kFridgeControlMinC ||
      settings.low_c > hw::kFridgeControlMaxC -
                           hw::kFridgeControlMinimumBandC ||
      settings.high_c - settings.low_c < hw::kFridgeControlMinimumBandC) {
    return false;
  }
  if (settings.freezer_lockout_c < hw::kFreezerThresholdMinC ||
      settings.freezer_lockout_c > hw::kFreezerThresholdMaxC ||
      settings.fridge_alarm_c < hw::kFridgeAlarmMinC ||
      settings.fridge_alarm_c > hw::kFridgeAlarmMaxC ||
      settings.freezer_alarm_c < hw::kFreezerAlarmMinC ||
      settings.freezer_alarm_c > hw::kFreezerAlarmMaxC) {
    return false;
  }
  if (settings.fan_delay_s < hw::kFanDelayMinS ||
      settings.fan_delay_s > hw::kFanDelayMaxS ||
      settings.fan_delay_s % hw::kFanDelayStepS != 0 ||
      settings.spillover_min_on_min < hw::kFanMinimumOnMin ||
      settings.spillover_min_on_min > hw::kFanMinimumOnMax ||
      settings.circulation_min_on_min < hw::kFanMinimumOnMin ||
      settings.circulation_min_on_min > hw::kFanMinimumOnMax) {
    return false;
  }
  if (!IsAllowedSettingOption(settings.emergency_spillover_on_min,
                              hw::kEmergencySpilloverOptions,
                              hw::kEmergencySpilloverOptionCount) ||
      settings.buzzer_mode >= hw::kBuzzerModeCount ||
      !IsAllowedSettingOption(settings.oled_contrast_percent,
                              hw::kOledContrastOptions,
                              hw::kOledContrastOptionCount) ||
      !IsAllowedSettingOption(settings.display_timeout_min,
                              hw::kDisplayTimeoutOptions,
                              hw::kDisplayTimeoutOptionCount)) {
    return false;
  }
  return true;
}

bool LoadedCalibrationValid(const float calibration_c[3]) {
  for (uint8_t role = 0; role < 3; ++role) {
    if (!std::isfinite(calibration_c[role]) ||
        calibration_c[role] < -hw::kCalibrationLimitC ||
        calibration_c[role] > hw::kCalibrationLimitC) {
      return false;
    }
  }
  return true;
}

}  // namespace

SettingsStore::SettingsStore(ControllerSettings& settings, bool& fahrenheit,
                             float calibration_c[3], String assigned_rom[3],
                             String& vessel_name)
    : FileSystemSaveable("/fridge/settings"),
      settings_(settings),
      fahrenheit_(fahrenheit),
      calibration_c_(calibration_c),
      assigned_rom_(assigned_rom),
      vessel_name_(vessel_name) {}

bool SettingsStore::to_json(JsonObject& root) {
  root["high_c"] = settings_.high_c;
  root["low_c"] = settings_.low_c;
  root["freezer_lockout_c"] = settings_.freezer_lockout_c;
  root["fridge_alarm_c"] = settings_.fridge_alarm_c;
  root["freezer_alarm_c"] = settings_.freezer_alarm_c;
  root["fan_delay_s"] = settings_.fan_delay_s;
  root["spillover_min_on_min"] = settings_.spillover_min_on_min;
  root["circulation_min_on_min"] = settings_.circulation_min_on_min;
  root["emergency_spillover_on_min"] =
      settings_.emergency_spillover_on_min;
  root["buzzer_mode"] = settings_.buzzer_mode;
  root["oled_contrast_percent"] = settings_.oled_contrast_percent;
  root["display_timeout_min"] = settings_.display_timeout_min;
  root["fridge_on_left"] = settings_.fridge_on_left;
  root["fahrenheit"] = fahrenheit_;
  root["fridge_calibration_c"] = calibration_c_[0];
  root["freezer_calibration_c"] = calibration_c_[1];
  root["ambient_calibration_c"] = calibration_c_[2];
  root["fridge_rom"] = assigned_rom_[0];
  root["freezer_rom"] = assigned_rom_[1];
  root["ambient_rom"] = assigned_rom_[2];
  root["vessel_name"] = vessel_name_;
  return true;
}

bool SettingsStore::from_json(const JsonObject& root) {
  settings_.high_c = root["high_c"] | settings_.high_c;
  settings_.low_c = root["low_c"] | settings_.low_c;
  settings_.freezer_lockout_c =
      root["freezer_lockout_c"] | settings_.freezer_lockout_c;
  settings_.fridge_alarm_c =
      root["fridge_alarm_c"] | settings_.fridge_alarm_c;
  settings_.freezer_alarm_c =
      root["freezer_alarm_c"] | settings_.freezer_alarm_c;
  settings_.fan_delay_s = root["fan_delay_s"] | settings_.fan_delay_s;
  settings_.spillover_min_on_min =
      root["spillover_min_on_min"] | settings_.spillover_min_on_min;
  settings_.circulation_min_on_min =
      root["circulation_min_on_min"] | settings_.circulation_min_on_min;
  settings_.emergency_spillover_on_min =
      root["emergency_spillover_on_min"] |
      settings_.emergency_spillover_on_min;
  if (root.containsKey("buzzer_mode")) {
    settings_.buzzer_mode = root["buzzer_mode"] | settings_.buzzer_mode;
  } else if (root.containsKey("buzzer_enabled")) {
    // One-time migration from the old ON/OFF setting. Existing enabled systems
    // move to the new default HI-LO sound; disabled systems stay silent.
    const bool legacy_enabled = root["buzzer_enabled"] | true;
    settings_.buzzer_mode = legacy_enabled ? hw::kDefaultBuzzerMode
                                            : hw::kBuzzerModeOff;
  }
  settings_.oled_contrast_percent =
      root["oled_contrast_percent"] | settings_.oled_contrast_percent;
  settings_.display_timeout_min =
      root["display_timeout_min"] | settings_.display_timeout_min;
  settings_.fridge_on_left = root["fridge_on_left"] | settings_.fridge_on_left;
  fahrenheit_ = root["fahrenheit"] | fahrenheit_;

  // Read raw calibration values first. During startup, invalid persisted values
  // trigger a coherent fallback to defaults rather than silently clamping a
  // partly-corrupt configuration into something that looks intentional.
  calibration_c_[0] = root["fridge_calibration_c"] | calibration_c_[0];
  calibration_c_[1] = root["freezer_calibration_c"] | calibration_c_[1];
  calibration_c_[2] = root["ambient_calibration_c"] | calibration_c_[2];

  if (startup_validation_pending_ &&
      (!LoadedControllerSettingsValid(settings_) ||
       !LoadedCalibrationValid(calibration_c_))) {
    settings_ = ControllerSettings{};
    fahrenheit_ = false;
    calibration_c_[0] = 0.0f;
    calibration_c_[1] = 0.0f;
    calibration_c_[2] = 0.0f;
    startup_defaults_restored_ = true;
  } else {
    NormalizeControllerSettings(settings_);
    for (uint8_t role = 0; role < 3; ++role) {
      if (!std::isfinite(calibration_c_[role])) calibration_c_[role] = 0.0f;
      calibration_c_[role] = constrain(calibration_c_[role],
                                       -hw::kCalibrationLimitC,
                                       hw::kCalibrationLimitC);
    }
  }

  assigned_rom_[0] = root["fridge_rom"] | assigned_rom_[0];
  assigned_rom_[1] = root["freezer_rom"] | assigned_rom_[1];
  assigned_rom_[2] = root["ambient_rom"] | assigned_rom_[2];
  // The rotary workflow already prevents duplicates. Apply the same invariant
  // to web edits and recovered configuration files so one physical probe can
  // never silently stand in for two locations.
  for (uint8_t role = 0; role < 3; ++role) {
    assigned_rom_[role].trim();
    assigned_rom_[role].toUpperCase();
    if (!assigned_rom_[role].isEmpty() && assigned_rom_[role].length() != 16) {
      assigned_rom_[role] = "";
      continue;
    }
    for (uint8_t earlier = 0; earlier < role; ++earlier) {
      if (!assigned_rom_[role].isEmpty() &&
          assigned_rom_[role].equalsIgnoreCase(assigned_rom_[earlier])) {
        assigned_rom_[role] = "";
        break;
      }
    }
  }
  vessel_name_ = root["vessel_name"] | vessel_name_;
  vessel_name_.trim();
  if (vessel_name_.length() > 24) vessel_name_.remove(24);

  return true;
}

const String ConfigSchema(const SettingsStore&) {
  String schema = R"JSON({
    "type":"object",
    "properties":{
      "high_c":{"title":"Fridge max T (C)","description":"Spillover turns ON after this temperature persists for the fan delay.","type":"number","minimum":%HIGH_MIN_C%,"maximum":%FRIDGE_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "low_c":{"title":"Fridge min T (C)","description":"Spillover may turn OFF after reaching this temperature and completing its minimum runtime. Circulation runs below this temperature.","type":"number","minimum":%FRIDGE_MIN_C%,"maximum":%LOW_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "freezer_lockout_c":{"title":"Freez T lockout (C)","type":"number","minimum":%FREEZER_MIN_C%,"maximum":%FREEZER_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "fridge_alarm_c":{"title":"Fridge alarm temperature (C)","type":"number","minimum":%FRIDGE_ALARM_MIN_C%,"maximum":%FRIDGE_ALARM_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "freezer_alarm_c":{"title":"Freezer alarm temperature (C)","type":"number","minimum":%FREEZER_ALARM_MIN_C%,"maximum":%FREEZER_ALARM_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "fan_delay_s":{"title":"Fan trigger delay (seconds)","type":"integer","minimum":%FAN_DELAY_MIN_S%,"maximum":%FAN_DELAY_MAX_S%,"multipleOf":%FAN_DELAY_STEP_S%},
      "spillover_min_on_min":{"title":"Spillover minimum ON (minutes)","type":"integer","minimum":%FAN_ON_MIN%,"maximum":%FAN_ON_MAX%},
      "circulation_min_on_min":{"title":"Circulation minimum ON (minutes)","type":"integer","minimum":%FAN_ON_MIN%,"maximum":%FAN_ON_MAX%},
      "emergency_spillover_on_min":{"title":"Get-me-home spillover fan (minutes ON per hour; 0 = OFF)","type":"integer","enum":%EMERGENCY_OPTIONS%},
      "buzzer_mode":{"title":"Buzzer","description":"Alarm sound; OFF disables audible alarms.","type":"integer","oneOf":[{"const":0,"title":"OFF"},{"const":1,"title":"STEADY"},{"const":2,"title":"DOUBLE"},{"const":3,"title":"HI-LO"},{"const":4,"title":"TRIPLE"}]},
      "oled_contrast_percent":{"title":"OLED contrast (%)","type":"integer","enum":%CONTRAST_OPTIONS%},
      "display_timeout_min":{"title":"Display auto-off (minutes, 0 = disabled)","type":"integer","enum":%DISPLAY_TIMEOUT_OPTIONS%},
      "fridge_on_left":{"title":"Display fridge on left side","type":"boolean"},
      "fahrenheit":{"title":"Display temperatures in Fahrenheit","type":"boolean"},
      "fridge_calibration_c":{"title":"Fridge calibration offset (C)","type":"number","minimum":%CALIBRATION_MIN_C%,"maximum":%CALIBRATION_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "freezer_calibration_c":{"title":"Freezer calibration offset (C)","type":"number","minimum":%CALIBRATION_MIN_C%,"maximum":%CALIBRATION_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "ambient_calibration_c":{"title":"Ambient calibration offset (C)","type":"number","minimum":%CALIBRATION_MIN_C%,"maximum":%CALIBRATION_MAX_C%,"multipleOf":%TEMP_STEP_C%},
      "fridge_rom":{"title":"Fridge sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "freezer_rom":{"title":"Freezer sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "ambient_rom":{"title":"Ambient sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "vessel_name":{"title":"Vessel name (optional)","type":"string","maxLength":24}
    }
  })JSON";
  schema.replace("%HIGH_MIN_C%",
                 String(hw::kFridgeControlMinC +
                            hw::kFridgeControlMinimumBandC,
                        1));
  schema.replace("%TEMP_STEP_C%", String(hw::kTemperatureEditStepC, 1));
  schema.replace("%FRIDGE_MIN_C%", String(hw::kFridgeControlMinC, 1));
  schema.replace("%FRIDGE_MAX_C%", String(hw::kFridgeControlMaxC, 1));
  schema.replace("%LOW_MAX_C%",
                 String(hw::kFridgeControlMaxC -
                            hw::kFridgeControlMinimumBandC,
                        1));
  schema.replace("%FREEZER_MIN_C%", String(hw::kFreezerThresholdMinC, 1));
  schema.replace("%FREEZER_MAX_C%", String(hw::kFreezerThresholdMaxC, 1));
  schema.replace("%FRIDGE_ALARM_MIN_C%", String(hw::kFridgeAlarmMinC, 1));
  schema.replace("%FRIDGE_ALARM_MAX_C%", String(hw::kFridgeAlarmMaxC, 1));
  schema.replace("%FREEZER_ALARM_MIN_C%", String(hw::kFreezerAlarmMinC, 1));
  schema.replace("%FREEZER_ALARM_MAX_C%", String(hw::kFreezerAlarmMaxC, 1));
  schema.replace("%FAN_DELAY_MIN_S%", String(hw::kFanDelayMinS));
  schema.replace("%FAN_DELAY_MAX_S%", String(hw::kFanDelayMaxS));
  schema.replace("%FAN_DELAY_STEP_S%", String(hw::kFanDelayStepS));
  schema.replace("%FAN_ON_MIN%", String(hw::kFanMinimumOnMin));
  schema.replace("%FAN_ON_MAX%", String(hw::kFanMinimumOnMax));
  schema.replace("%CALIBRATION_MIN_C%",
                 String(-hw::kCalibrationLimitC, 1));
  schema.replace("%CALIBRATION_MAX_C%", String(hw::kCalibrationLimitC, 1));
  schema.replace(
      "%EMERGENCY_OPTIONS%",
      JsonNumberArray(hw::kEmergencySpilloverOptions,
                      hw::kEmergencySpilloverOptionCount));
  schema.replace(
      "%DISPLAY_TIMEOUT_OPTIONS%",
      JsonNumberArray(hw::kDisplayTimeoutOptions,
                      hw::kDisplayTimeoutOptionCount));
  schema.replace(
      "%CONTRAST_OPTIONS%",
      JsonNumberArray(hw::kOledContrastOptions,
                      hw::kOledContrastOptionCount));
  return schema;
}
