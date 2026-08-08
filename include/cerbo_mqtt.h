#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

// Lightweight MQTT publisher for direct ESP32 -> Cerbo GX temperature data.
//
// This class deliberately owns no dynamic String state in its periodic service
// path. Connection settings are copied into fixed-size buffers when configure()
// is called. service() is safe to call from the main loop and uses rollover-safe
// unsigned millis() arithmetic.
class CerboMqttPublisher {
 public:
  static constexpr uint16_t kDefaultPort = 1883;

  CerboMqttPublisher();

  void configure(const char* host, uint16_t port, const char* username,
                 const char* password, uint32_t publish_interval_ms);

  // Call frequently from loop(). Temperatures are Celsius. NAN values are not
  // published. A successful reconnect causes an immediate publish attempt.
  void service(uint32_t now, float fridge_c, float freezer_c, float ambient_c);

  // Enabled reflects the user's reporting choice. A missing/invalid broker
  // host is therefore shown as enabled-but-disconnected rather than silently
  // hiding the MQTT status indicator.
  bool enabled() const { return publish_interval_ms_ != 0; }
  bool connected() { return enabled() && mqtt_.connected(); }

  // Force the next successful service cycle to publish immediately.
  void request_publish() { publish_due_ = true; }

 private:
  static constexpr size_t kHostSize = 64;
  static constexpr size_t kCredentialSize = 48;
  static constexpr size_t kClientIdSize = 40;
  static constexpr uint32_t kReconnectMinMs = 5000UL;
  static constexpr uint32_t kReconnectMaxMs = 60000UL;

  void reconnect(uint32_t now);
  bool publish_temperature(const char* topic, float value);
  void build_client_id();

  WiFiClient network_client_;
  PubSubClient mqtt_;

  char host_[kHostSize] = {};
  char username_[kCredentialSize] = {};
  char password_[kCredentialSize] = {};
  char client_id_[kClientIdSize] = {};
  uint16_t port_ = kDefaultPort;
  uint32_t publish_interval_ms_ = 0;
  uint32_t last_publish_ms_ = 0;
  uint32_t last_reconnect_attempt_ms_ = 0;
  uint32_t reconnect_delay_ms_ = kReconnectMinMs;
  bool publish_due_ = false;
};

// Single application-wide publisher instance. Keeping the publisher in one
// place lets the display report live MQTT state without duplicating connection
// state, while the main application can configure/service the same instance.
CerboMqttPublisher& cerbo_mqtt_publisher();
