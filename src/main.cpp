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

// Neutralize the legacy manual ArduinoOTA calls in main_implementation.inc.
// SensESP owns ArduinoOTA.begin() and its normal event-loop servicing; keeping
// these old call sites inactive avoids initializing or servicing OTA twice.
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

  ArduinoOTA.onStart([]() {
    ota_in_progress = true;

    // Cerbo MQTT is explicitly disconnected and held off. Signal K is
    // effectively suspended by the OTA-exclusive loop below: once this callback
    // returns, subsequent iterations do not tick the SensESP event loop at all.
    // Only ArduinoOTA.handle() is serviced until success or failure, so the SK
    // websocket cannot generate reconnect or publish traffic during the transfer.
    cerbo_mqtt_publisher().suspend();

    if (output_test_active) stop_output_test();
    buzzer.stop();

    ESP_LOGW("OTA", "OTA started: Signal K event loop and Cerbo MQTT suspended");
  });

  ArduinoOTA.onEnd([]() {
    // Successful firmware OTA reboots after this callback. Keep the controller
    // in OTA-exclusive mode until that reboot rather than briefly restarting
    // application services against firmware that is about to be replaced.
    ESP_LOGW("OTA", "OTA transfer complete; reboot pending");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    ESP_LOGE("OTA", "OTA failed: %s error (%u); resuming normal services",
             ota_error_name(error), static_cast<unsigned>(error));

    // The SK websocket may have been timed out by the server while its event
    // loop was paused. restart() gives it a clean reconnection path instead of
    // relying on stale transport state after a failed OTA attempt.
    if (sensesp_app && sensesp_app->get_ws_client()) {
      sensesp_app->get_ws_client()->restart();
    }
    cerbo_mqtt_publisher().resume(millis());
    ota_in_progress = false;
  });
}

void loop() {
  feed_task_watchdog();

  if (ota_in_progress) {
    // Do not tick SensESP here: its event loop also owns Signal K and other
    // services. Calling ArduinoOTA directly keeps firmware receive traffic
    // exclusive while still using SensESP for OTA configuration/initialization.
    ArduinoOTA.handle();
    feed_task_watchdog();
    delay(1);
    return;
  }

  fridge_controller_loop();
}
