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
                float calibration_c[3], String assigned_rom[3],
                String& vessel_name);

  bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& root) override;

  // Startup loading is stricter than later web edits: if persisted control or
  // calibration values are invalid, defaults are restored as one coherent set.
  bool startup_defaults_restored() const { return startup_defaults_restored_; }
  void finish_startup_validation() { startup_validation_pending_ = false; }

 private:
  ControllerSettings& settings_;
  bool& fahrenheit_;
  float* calibration_c_;
  String* assigned_rom_;
  String& vessel_name_;
  bool startup_validation_pending_ = true;
  bool startup_defaults_restored_ = false;
};

const String ConfigSchema(const SettingsStore& store);
