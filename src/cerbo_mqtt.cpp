#include "cerbo_mqtt.h"

#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr char kFridgeTopic[] = "marinefridge/fridge/temperature";
constexpr char kFreezerTopic[] = "marinefridge/freezer/temperature";
constexpr char kAmbientTopic[] = "marinefridge/ambient/temperature";

CerboMqttPublisher g_cerbo_mqtt_publisher;
}

CerboMqttPublisher& cerbo_mqtt_publisher() {
  return g_cerbo_mqtt_publisher;
}

CerboMqttPublisher::CerboMqttPublisher() : mqtt_(network_client_) {
  build_client_id();
}

void CerboMqttPublisher::configure(const char* host, uint16_t port,
                                   const char* username, const char* password,
                                   uint32_t publish_interval_ms) {
  snprintf(host_, sizeof(host_), "%s", host ? host : "");
  snprintf(username_, sizeof(username_), "%s", username ? username : "");
  snprintf(password_, sizeof(password_), "%s", password ? password : "");
  port_ = port == 0 ? kDefaultPort : port;
  publish_interval_ms_ = publish_interval_ms;

  mqtt_.disconnect();
  mqtt_.setServer(host_, port_);
  reconnect_delay_ms_ = kReconnectMinMs;
  last_reconnect_attempt_ms_ = 0;
  last_publish_ms_ = 0;
  publish_due_ = enabled();
}

void CerboMqttPublisher::build_client_id() {
  const uint64_t chip_id = ESP.getEfuseMac();
  snprintf(client_id_, sizeof(client_id_), "fridge-%04X%08X",
           static_cast<unsigned>((chip_id >> 32U) & 0xFFFFU),
           static_cast<unsigned>(chip_id & 0xFFFFFFFFU));
}

void CerboMqttPublisher::reconnect(uint32_t now) {
  if (!enabled() || WiFi.status() != WL_CONNECTED) return;
  if (mqtt_.connected()) return;

  if (last_reconnect_attempt_ms_ != 0 &&
      now - last_reconnect_attempt_ms_ < reconnect_delay_ms_) {
    return;
  }
  last_reconnect_attempt_ms_ = now;

  bool connected = false;
  if (username_[0] != '\0') {
    connected = mqtt_.connect(client_id_, username_, password_);
  } else {
    connected = mqtt_.connect(client_id_);
  }

  if (connected) {
    reconnect_delay_ms_ = kReconnectMinMs;
    publish_due_ = true;
    return;
  }

  if (reconnect_delay_ms_ < kReconnectMaxMs) {
    reconnect_delay_ms_ *= 2;
    if (reconnect_delay_ms_ > kReconnectMaxMs) {
      reconnect_delay_ms_ = kReconnectMaxMs;
    }
  }
}

bool CerboMqttPublisher::publish_temperature(const char* topic, float value) {
  if (!std::isfinite(value)) return true;

  char payload[16];
  const int length = snprintf(payload, sizeof(payload), "%.2f", value);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(payload)) return false;

  // Retain the latest value so Node-RED receives the current reading after a
  // restart/redeploy without waiting for the next configured reporting period.
  return mqtt_.publish(topic, payload, true);
}

void CerboMqttPublisher::service(uint32_t now, float fridge_c, float freezer_c,
                                 float ambient_c) {
  if (!enabled()) {
    if (mqtt_.connected()) mqtt_.disconnect();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (mqtt_.connected()) mqtt_.disconnect();
    return;
  }

  reconnect(now);
  if (!mqtt_.connected()) return;

  mqtt_.loop();

  if (!publish_due_ && now - last_publish_ms_ < publish_interval_ms_) return;

  const bool fridge_ok = publish_temperature(kFridgeTopic, fridge_c);
  const bool freezer_ok = publish_temperature(kFreezerTopic, freezer_c);
  const bool ambient_ok = publish_temperature(kAmbientTopic, ambient_c);

  if (fridge_ok && freezer_ok && ambient_ok) {
    last_publish_ms_ = now;
    publish_due_ = false;
  } else {
    // Do not spin in a tight retry loop on a transient broker/network failure.
    // Drop the connection and let the bounded reconnect backoff recover it.
    mqtt_.disconnect();
    last_reconnect_attempt_ms_ = now;
  }
}
