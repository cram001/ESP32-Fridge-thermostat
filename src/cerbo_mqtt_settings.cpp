#include "cerbo_mqtt_settings.h"

namespace {
constexpr size_t kMaxHostLength = 63;
constexpr size_t kMaxCredentialLength = 47;
}

CerboMqttSettingsStore::CerboMqttSettingsStore(CerboMqttSettings& settings)
    : FileSystemSaveable("/cerbo-mqtt/settings"), settings_(settings) {}

bool CerboMqttSettingsStore::to_json(JsonObject& root) {
  root["host"] = settings_.host;
  root["port"] = settings_.port;
  root["username"] = settings_.username;
  root["password"] = settings_.password;
  root["report_interval_s"] = settings_.report_interval_s;
  return true;
}

bool CerboMqttSettingsStore::from_json(const JsonObject& root) {
  settings_.host = root["host"] | settings_.host;
  settings_.port = root["port"] | settings_.port;
  settings_.username = root["username"] | settings_.username;
  settings_.password = root["password"] | settings_.password;
  settings_.report_interval_s =
      root["report_interval_s"] | settings_.report_interval_s;

  settings_.host.trim();
  if (settings_.host.length() > kMaxHostLength) {
    settings_.host.remove(kMaxHostLength);
  }
  if (settings_.username.length() > kMaxCredentialLength) {
    settings_.username.remove(kMaxCredentialLength);
  }
  if (settings_.password.length() > kMaxCredentialLength) {
    settings_.password.remove(kMaxCredentialLength);
  }
  if (settings_.port == 0) {
    settings_.port = CerboMqttPublisher::kDefaultPort;
  }
  if (!cerbo_mqtt::IsAllowedReportInterval(settings_.report_interval_s)) {
    settings_.report_interval_s = 0;
  }
  return true;
}

const String ConfigSchema(const CerboMqttSettingsStore&) {
  return R"JSON({
    "type":"object",
    "properties":{
      "host":{"title":"Cerbo GX MQTT host / IP","description":"LAN hostname or IP address of the Cerbo GX. A DHCP reservation/static address is recommended.","type":"string","maxLength":63},
      "port":{"title":"MQTT port","description":"Default plain MQTT port is 1883.","type":"integer","minimum":1,"maximum":65535},
      "username":{"title":"MQTT username (optional)","type":"string","maxLength":47},
      "password":{"title":"MQTT password (optional)","type":"string","format":"password","maxLength":47},
      "report_interval_s":{"title":"Cerbo MQTT reporting interval","description":"OFF disables the MQTT connection and publishing.","type":"integer","oneOf":[{"const":0,"title":"OFF"},{"const":30,"title":"30 sec"},{"const":60,"title":"1 min"},{"const":120,"title":"2 min"},{"const":300,"title":"5 min"},{"const":600,"title":"10 min"}]}
    }
  })JSON";
}
