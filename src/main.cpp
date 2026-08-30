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

// PR #25 moved the controller into main_implementation.inc so an OTA-exclusive
// wrapper could take over Arduino setup()/loop(). Keep that split for now, but
// make the embedded controller use SensESP's OTA configuration rather than a
// second independently-started ArduinoOTA instance.
class FridgeSensESPAppBuilder : public sensesp::SensESPAppBuilder {
 public:
  FridgeSensESPAppBuilder() { enable_ota("esp32!"); }
};

// main_implementation.inc still contains the pre-PR25 manual ArduinoOTA calls.
// Neutralize only those legacy calls while the implementation is included.
// The real ArduinoOTA instance is owned and serviced by SensESP.
class LegacyArduinoOtaNoop {
 public:
  void setHostname(const char*) {}
  void begin() {}
  void handle() {}
};

LegacyArduinoOtaNoop legacy_arduino_ota_noop;

}  // namespace

// Rename the controller entry points and substitute the SensESP OTA-enabled
// builder. This deliberately avoids running two ArduinoOTA begin/handle paths.
#define setup fridge_controller_setup
#define loop fridge_controller_loop
#define SensESPAppBuilder FridgeSensESPAppBuilder
#define ArduinoOTA legacy_arduino_ota_noop
#include "main_implementation.inc"
#undef ArduinoOTA
#undef SensESPAppBuilder
#undef loop
#undef setup

void setup() {
  fridge_controller_setup();

  // Match the known-working Yanmar architecture: after SensESP has created its
  // network objects, suspend only competing network clients during OTA. The
  // controller, temperature monitoring, watchdog, display, alarms, and fan
  // safety logic continue to run normally while the inactive OTA slot is being
  // written.
  if (sensesp_app != nullptr) {
    auto* ws_client = sensesp_app->get_ws_client().get();
    sensesp_app->get_event_loop()->onDelay(0, [ws_client]() {
      ArduinoOTA.onStart([ws_client]() {
        ESP_LOGI("OTA", "OTA starting: suspending Signal K and Cerbo MQTT");
        if (ws_client != nullptr) ws_client->suspend();
        cerbo_mqtt_publisher().suspend();
      });

      ArduinoOTA.onEnd([ws_client]() {
        ESP_LOGI("OTA", "OTA transfer complete");
        // Successful firmware OTA normally reboots immediately. Resume here as
        // a defensive fallback in case the framework does not reboot.
        cerbo_mqtt_publisher().resume();
        if (ws_client != nullptr) ws_client->resume();
      });

      ArduinoOTA.onError([ws_client](ota_error_t error) {
        ESP_LOGE("OTA", "OTA failed with error %u; resuming network services",
                 static_cast<unsigned>(error));
        cerbo_mqtt_publisher().resume();
        if (ws_client != nullptr) ws_client->resume();
      });
    });
  }
}

void loop() {
  // SensESP's event loop services the single ArduinoOTA instance. The fridge
  // controller continues executing during OTA so physical fan state and safety
  // logic remain deterministic rather than freezing at the last command.
  fridge_controller_loop();
}
