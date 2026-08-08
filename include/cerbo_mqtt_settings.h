#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "cerbo_mqtt.h"
#include "cerbo_mqtt_interval.h"
#include "sensesp/system/saveable.h"

struct CerboMqttSettings {
  String host;
  uint16_t port = CerboMqttPublisher::kDefaultPort;
  String username;
  String password;
  uint16_t report_interval_s = 0;
};

class CerboMqttSettingsStore : public sensesp::FileSystemSaveable {
 public:
  explicit CerboMqttSettingsStore(CerboMqttSettings& settings);

  bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& root) override;

 private:
  CerboMqttSettings& settings_;
};

const String ConfigSchema(const CerboMqttSettingsStore& store);
