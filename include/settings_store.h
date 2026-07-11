#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "fridge_controller.h"
#include "sensesp/system/saveable.h"

// Single source of truth for settings changed by either the rotary UI or the
// SensESP Web UI. FileSystemSaveable stores it under /config/fridge/settings.
class SettingsStore : public sensesp::FileSystemSaveable {
 public:
  SettingsStore(ControllerSettings& settings, bool& fahrenheit,
                float calibration_c[3], String assigned_rom[3]);

  bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& root) override;

 private:
  ControllerSettings& settings_;
  bool& fahrenheit_;
  float* calibration_c_;
  String* assigned_rom_;
};

const String ConfigSchema(const SettingsStore& store);

