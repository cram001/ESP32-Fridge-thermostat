#include "settings_store.h"

#include "hardware_config.h"

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
  root["buzzer_enabled"] = settings_.buzzer_enabled;
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
  settings_.high_c = constrain(root["high_c"] | settings_.high_c,
                               hw::kFridgeControlMinC,
                               hw::kFridgeControlMaxC);
  settings_.low_c = constrain(root["low_c"] | settings_.low_c,
                              hw::kFridgeControlMinC,
                              hw::kFridgeControlMaxC);
  settings_.freezer_lockout_c = constrain(
      root["freezer_lockout_c"] | settings_.freezer_lockout_c,
      hw::kFreezerThresholdMinC, hw::kFreezerThresholdMaxC);
  settings_.fridge_alarm_c = constrain(
      root["fridge_alarm_c"] | settings_.fridge_alarm_c,
      hw::kFridgeAlarmMinC, hw::kFridgeAlarmMaxC);
  settings_.freezer_alarm_c = constrain(
      root["freezer_alarm_c"] | settings_.freezer_alarm_c,
      hw::kFreezerAlarmMinC, hw::kFreezerAlarmMaxC);
  settings_.fan_delay_s = constrain(
      root["fan_delay_s"] | settings_.fan_delay_s, 5, 180);
  settings_.spillover_min_on_min = constrain(
      root["spillover_min_on_min"] | settings_.spillover_min_on_min, 1, 5);
  settings_.circulation_min_on_min = constrain(
      root["circulation_min_on_min"] | settings_.circulation_min_on_min, 1, 5);

  const uint8_t emergency_on = root["emergency_spillover_on_min"] |
                               settings_.emergency_spillover_on_min;
  settings_.emergency_spillover_on_min =
      emergency_on == 0 || emergency_on == 5 || emergency_on == 10 ||
              emergency_on == 20 || emergency_on == 30 || emergency_on == 40
          ? emergency_on
          : 0;
  settings_.buzzer_enabled = root["buzzer_enabled"] | settings_.buzzer_enabled;
  settings_.oled_contrast_percent = constrain(
      root["oled_contrast_percent"] | settings_.oled_contrast_percent, 10, 100);
  const uint8_t display_timeout =
      root["display_timeout_min"] | settings_.display_timeout_min;
  settings_.display_timeout_min =
      display_timeout == 0 || display_timeout == 1 || display_timeout == 5 ||
              display_timeout == 10 || display_timeout == 15 ||
              display_timeout == 20 || display_timeout == 30 ||
              display_timeout == 60
          ? display_timeout
          : 0;
  settings_.fridge_on_left = root["fridge_on_left"] | settings_.fridge_on_left;
  fahrenheit_ = root["fahrenheit"] | fahrenheit_;
  calibration_c_[0] = constrain(
      root["fridge_calibration_c"] | calibration_c_[0],
      -hw::kCalibrationLimitC, hw::kCalibrationLimitC);
  calibration_c_[1] = constrain(
      root["freezer_calibration_c"] | calibration_c_[1],
      -hw::kCalibrationLimitC, hw::kCalibrationLimitC);
  calibration_c_[2] = constrain(
      root["ambient_calibration_c"] | calibration_c_[2],
      -hw::kCalibrationLimitC, hw::kCalibrationLimitC);
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

  // Never allow the low threshold to overlap the high threshold.
  settings_.low_c = constrain(settings_.low_c, hw::kFridgeControlMinC,
                              settings_.high_c - hw::kHysteresisC);
  return true;
}

const String ConfigSchema(const SettingsStore&) {
  return R"JSON({
    "type":"object",
    "properties":{
      "high_c":{"title":"Spillover ON temperature (C)","type":"number","minimum":-5,"maximum":15,"multipleOf":0.1},
      "low_c":{"title":"Circulation ON temperature (C)","type":"number","minimum":-5,"maximum":15,"multipleOf":0.1},
      "freezer_lockout_c":{"title":"Freezer lockout temperature (C)","type":"number","minimum":-30,"maximum":10,"multipleOf":0.1},
      "fridge_alarm_c":{"title":"Fridge alarm temperature (C)","type":"number","minimum":0,"maximum":30,"multipleOf":0.1},
      "freezer_alarm_c":{"title":"Freezer alarm temperature (C)","type":"number","minimum":-30,"maximum":10,"multipleOf":0.1},
      "fan_delay_s":{"title":"Fan trigger delay (seconds)","type":"integer","minimum":5,"maximum":180},
      "spillover_min_on_min":{"title":"Spillover minimum ON (minutes)","type":"integer","minimum":1,"maximum":5},
      "circulation_min_on_min":{"title":"Circulation minimum ON (minutes)","type":"integer","minimum":1,"maximum":5},
      "emergency_spillover_on_min":{"title":"Get-me-home spillover fan (minutes ON per hour; 0 = OFF)","type":"integer","enum":[0,5,10,20,30,40]},
      "buzzer_enabled":{"title":"Buzzer enabled","type":"boolean"},
      "oled_contrast_percent":{"title":"OLED contrast (%)","type":"integer","minimum":10,"maximum":100,"multipleOf":10},
      "display_timeout_min":{"title":"Display auto-off (minutes, 0 = disabled)","type":"integer","enum":[0,1,5,10,15,20,30,60]},
      "fridge_on_left":{"title":"Display fridge on left side","type":"boolean"},
      "fahrenheit":{"title":"Display temperatures in Fahrenheit","type":"boolean"},
      "fridge_calibration_c":{"title":"Fridge calibration offset (C)","type":"number","minimum":-5,"maximum":5,"multipleOf":0.1},
      "freezer_calibration_c":{"title":"Freezer calibration offset (C)","type":"number","minimum":-5,"maximum":5,"multipleOf":0.1},
      "ambient_calibration_c":{"title":"Ambient calibration offset (C)","type":"number","minimum":-5,"maximum":5,"multipleOf":0.1},
      "fridge_rom":{"title":"Fridge sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "freezer_rom":{"title":"Freezer sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "ambient_rom":{"title":"Ambient sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "vessel_name":{"title":"Vessel name (optional)","type":"string","maxLength":24}
    }
  })JSON";
}
