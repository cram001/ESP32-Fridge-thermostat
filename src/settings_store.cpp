#include "settings_store.h"

SettingsStore::SettingsStore(ControllerSettings& settings, bool& fahrenheit,
                             float calibration_c[3], String assigned_rom[3])
    : FileSystemSaveable("/fridge/settings"),
      settings_(settings),
      fahrenheit_(fahrenheit),
      calibration_c_(calibration_c),
      assigned_rom_(assigned_rom) {}

bool SettingsStore::to_json(JsonObject& root) {
  root["high_c"] = settings_.high_c;
  root["low_c"] = settings_.low_c;
  root["freezer_lockout_c"] = settings_.freezer_lockout_c;
  root["fridge_alarm_c"] = settings_.fridge_alarm_c;
  root["freezer_alarm_c"] = settings_.freezer_alarm_c;
  root["fan_delay_s"] = settings_.fan_delay_s;
  root["spillover_min_on_min"] = settings_.spillover_min_on_min;
  root["circulation_min_on_min"] = settings_.circulation_min_on_min;
  root["failsafe_off_min"] = settings_.failsafe_off_min;
  root["buzzer_enabled"] = settings_.buzzer_enabled;
  root["fahrenheit"] = fahrenheit_;
  root["fridge_calibration_c"] = calibration_c_[0];
  root["freezer_calibration_c"] = calibration_c_[1];
  root["ambient_calibration_c"] = calibration_c_[2];
  root["fridge_rom"] = assigned_rom_[0];
  root["freezer_rom"] = assigned_rom_[1];
  root["ambient_rom"] = assigned_rom_[2];
  return true;
}

bool SettingsStore::from_json(const JsonObject& root) {
  settings_.high_c = constrain(root["high_c"] | settings_.high_c, -40.0f, 30.0f);
  settings_.low_c = constrain(root["low_c"] | settings_.low_c, -40.0f, 30.0f);
  settings_.freezer_lockout_c = constrain(
      root["freezer_lockout_c"] | settings_.freezer_lockout_c, -40.0f, 20.0f);
  settings_.fridge_alarm_c = constrain(
      root["fridge_alarm_c"] | settings_.fridge_alarm_c, -20.0f, 40.0f);
  settings_.freezer_alarm_c = constrain(
      root["freezer_alarm_c"] | settings_.freezer_alarm_c, -40.0f, 20.0f);
  settings_.fan_delay_s = constrain(
      root["fan_delay_s"] | settings_.fan_delay_s, 5, 180);
  settings_.spillover_min_on_min = constrain(
      root["spillover_min_on_min"] | settings_.spillover_min_on_min, 1, 5);
  settings_.circulation_min_on_min = constrain(
      root["circulation_min_on_min"] | settings_.circulation_min_on_min, 1, 5);

  const uint8_t fail_off = root["failsafe_off_min"] | settings_.failsafe_off_min;
  settings_.failsafe_off_min =
      fail_off == 5 || fail_off == 10 || fail_off == 15 || fail_off == 20
          ? fail_off : 15;
  settings_.buzzer_enabled = root["buzzer_enabled"] | settings_.buzzer_enabled;
  fahrenheit_ = root["fahrenheit"] | fahrenheit_;
  calibration_c_[0] = constrain(
      root["fridge_calibration_c"] | calibration_c_[0], -5.0f, 5.0f);
  calibration_c_[1] = constrain(
      root["freezer_calibration_c"] | calibration_c_[1], -5.0f, 5.0f);
  calibration_c_[2] = constrain(
      root["ambient_calibration_c"] | calibration_c_[2], -5.0f, 5.0f);
  assigned_rom_[0] = root["fridge_rom"] | assigned_rom_[0];
  assigned_rom_[1] = root["freezer_rom"] | assigned_rom_[1];
  assigned_rom_[2] = root["ambient_rom"] | assigned_rom_[2];

  // Never allow the low threshold to overlap the high threshold.
  settings_.low_c = min(settings_.low_c, settings_.high_c - 0.5f);
  return true;
}

const String ConfigSchema(const SettingsStore&) {
  return R"JSON({
    "type":"object",
    "properties":{
      "high_c":{"title":"Spillover ON temperature (C)","type":"number","minimum":-40,"maximum":30},
      "low_c":{"title":"Circulation ON temperature (C)","type":"number","minimum":-40,"maximum":30},
      "freezer_lockout_c":{"title":"Freezer lockout temperature (C)","type":"number","minimum":-40,"maximum":20},
      "fridge_alarm_c":{"title":"Fridge alarm temperature (C)","type":"number","minimum":-20,"maximum":40},
      "freezer_alarm_c":{"title":"Freezer alarm temperature (C)","type":"number","minimum":-40,"maximum":20},
      "fan_delay_s":{"title":"Fan trigger delay (seconds)","type":"integer","minimum":5,"maximum":180},
      "spillover_min_on_min":{"title":"Spillover minimum ON (minutes)","type":"integer","minimum":1,"maximum":5},
      "circulation_min_on_min":{"title":"Circulation minimum ON (minutes)","type":"integer","minimum":1,"maximum":5},
      "failsafe_off_min":{"title":"Fridge sensor fail-safe OFF time (minutes)","type":"integer","enum":[5,10,15,20]},
      "buzzer_enabled":{"title":"Buzzer enabled","type":"boolean"},
      "fahrenheit":{"title":"Display temperatures in Fahrenheit","type":"boolean"},
      "fridge_calibration_c":{"title":"Fridge calibration offset (C)","type":"number","minimum":-5,"maximum":5,"multipleOf":0.1},
      "freezer_calibration_c":{"title":"Freezer calibration offset (C)","type":"number","minimum":-5,"maximum":5,"multipleOf":0.1},
      "ambient_calibration_c":{"title":"Ambient calibration offset (C)","type":"number","minimum":-5,"maximum":5,"multipleOf":0.1},
      "fridge_rom":{"title":"Fridge sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "freezer_rom":{"title":"Freezer sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"},
      "ambient_rom":{"title":"Ambient sensor ROM (16 hex characters)","type":"string","pattern":"^(|[0-9A-Fa-f]{16})$"}
    }
  })JSON";
}
