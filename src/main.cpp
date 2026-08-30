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

  // SensESP 3.4.0 owns and services the single ArduinoOTA instance. Its current
  // SKWSClient has no supported suspend/resume API (unlike the older SensESP
  // release used by the Yanmar project), so do not reach into its protected
  // websocket internals. We can still eliminate the other application-owned
  // competing network client: Cerbo MQTT is disconnected for the transfer and
  // resumes cleanly after a failed/non-rebooting OTA attempt.
  if (sensesp_app != nullptr) {
    sensesp_app->get_event_loop()->onDelay(0, []() {
      ArduinoOTA.onStart([]() {
        ESP_LOGI("OTA", "OTA starting: suspending Cerbo MQTT");
        cerbo_mqtt_publisher().suspend();
      });

      ArduinoOTA.onEnd([]() {
        ESP_LOGI("OTA", "OTA transfer complete");
        // Successful firmware OTA normally reboots immediately. Resume here as
        // a defensive fallback in case the framework does not reboot.
        cerbo_mqtt_publisher().resume();
      });

      ArduinoOTA.onError([](ota_error_t error) {
        ESP_LOGE("OTA", "OTA failed with error %u; resuming Cerbo MQTT",
                 static_cast<unsigned>(error));
        cerbo_mqtt_publisher().resume();
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
