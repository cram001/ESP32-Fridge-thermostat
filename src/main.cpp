#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DFRobot_VisualRotaryEncoder.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#include "buzzer_controller.h"
#include "cerbo_mqtt.h"
#include "cerbo_mqtt_interval.h"
#include "cerbo_mqtt_settings.h"
#include "fridge_display.h"
#include "fridge_controller.h"
#include "fault_manager.h"
#include "hardware_config.h"
#include "settings_store.h"
#include "temperature_manager.h"
#include "sensesp.h"
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/ui/config_item.h"

namespace {

// main_implementation.inc predates the OTA architecture change and constructs
// its SensESP builder internally. This derived builder keeps that implementation
// intact while making OTA a SensESP-owned service. enable_ota() only configures
// the SensESPApp; the hostname is still applied immediately afterward by the
// existing builder.set_hostname("fridge-controller") call.
class FridgeSensESPAppBuilder : public sensesp::SensESPAppBuilder {
 public:
  FridgeSensESPAppBuilder() { enable_ota("esp32!"); }
};

// Neutralize the three legacy manual ArduinoOTA calls inside
// main_implementation.inc. SensESP now owns ArduinoOTA.begin()/handle(); keeping
// those old call sites inactive avoids running two OTA service mechanisms while
// allowing the controller implementation to remain a single shared unit for
// firmware and tests.
class LegacyArduinoOtaNoop {
 public:
  void setHostname(const char*) {}
  void begin() {}
  void handle() {}
};

LegacyArduinoOtaNoop legacy_arduino_ota_noop;
volatile bool ota_in_progress = false;

const char* ota_error_name(ota_error_t error) {
  switch (error) {
    case OTA_AUTH_ERROR:
      return "auth";
    case OTA_BEGIN_ERROR:
      return "begin";
    case OTA_CONNECT_ERROR:
      return "connect";
    case OTA_RECEIVE_ERROR:
      return "receive";
    case OTA_END_ERROR:
      return "end";
    default:
      return "unknown";
  }
}

}  // namespace

// Keep the controller implementation unchanged, but rename its Arduino entry
// points so this file can layer OTA service isolation around the normal loop.
// Headers are included before these macros so dependency declarations are not
// rewritten by the preprocessor.
#define SensESPAppBuilder FridgeSensESPAppBuilder
#define ArduinoOTA legacy_arduino_ota_noop
#define setup fridge_controller_setup
#define loop fridge_controller_loop
#include "main_implementation.inc"
#undef loop
#undef setup
#undef ArduinoOTA
#undef SensESPAppBuilder

void setup() {
  fridge_controller_setup();

  auto ws_client = sensesp_app ? sensesp_app->get_ws_client() : nullptr;

  ArduinoOTA.onStart([ws_client]() {
    ota_in_progress = true;

    // Stop nonessential network traffic. The controller loop will stop after
    // this iteration; physical fan outputs then remain at their last commanded
    // state for the transfer. The persistent all-fans-off interlock therefore
    // remains physically OFF during OTA as well.
    if (ws_client) ws_client->suspend();
    cerbo_mqtt_publisher().suspend();

    if (output_test_active) stop_output_test();
    buzzer.stop();

    ESP_LOGW("OTA", "OTA started: Signal K and Cerbo MQTT suspended");
  });

  ArduinoOTA.onEnd([]() {
    // Successful firmware OTA reboots after this callback. Keep the controller
    // in OTA-exclusive mode until that reboot rather than briefly restarting
    // application services against firmware that is about to be replaced.
    ESP_LOGW("OTA", "OTA transfer complete; reboot pending");
  });

  ArduinoOTA.onError([ws_client](ota_error_t error) {
    ESP_LOGE("OTA", "OTA failed: %s error (%u); resuming normal services",
             ota_error_name(error), static_cast<unsigned>(error));

    if (ws_client) ws_client->resume();
    cerbo_mqtt_publisher().resume(millis());
    ota_in_progress = false;
  });
}

void loop() {
  feed_task_watchdog();

  if (ota_in_progress) {
    // SensESP owns ArduinoOTA.handle() through its event loop. Keep only that
    // event loop and the watchdog alive during the transfer; sensors, display,
    // control logic, Signal K traffic, and MQTT servicing remain quiescent.
    event_loop()->tick();
    feed_task_watchdog();
    delay(1);
    return;
  }

  fridge_controller_loop();
}
