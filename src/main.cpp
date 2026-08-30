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

// Keep the controller implementation unchanged, but rename its Arduino entry
// points so this file can put a very small OTA-priority wrapper around them.
// All headers are included above before these macros are defined so setup/loop
// identifiers inside dependencies cannot be rewritten by the preprocessor.
#define setup fridge_controller_setup
#define loop fridge_controller_loop
#include "main_implementation.inc"
#undef loop
#undef setup

namespace {
volatile bool ota_in_progress = false;
}

void setup() {
  fridge_controller_setup();

  // The existing implementation has already called ArduinoOTA.begin().
  // Registering the callbacks here is valid and avoids disturbing the proven
  // controller startup sequence. Once OTA starts, loop() below stops servicing
  // SensESP, Signal K, Cerbo MQTT, display, sensors, and control logic until the
  // transfer finishes or errors. Physical outputs therefore remain at their
  // last commanded state for the short update window.
  ArduinoOTA.onStart([]() {
    ota_in_progress = true;
    ESP_LOGW("OTA", "OTA exclusive mode started");
  });

  ArduinoOTA.onEnd([]() {
    ESP_LOGW("OTA", "OTA transfer complete");
    ota_in_progress = false;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    ESP_LOGE("OTA", "OTA failed with error %u; resuming normal services",
             static_cast<unsigned>(error));
    ota_in_progress = false;
  });
}

void loop() {
  // Give OTA first chance to consume incoming packets. If this call starts an
  // update, onStart() flips ota_in_progress before any SensESP work is run.
  ArduinoOTA.handle();
  feed_task_watchdog();

  if (ota_in_progress) {
    // Yield to the Wi-Fi/TCP stack while deliberately avoiding event_loop()->
    // tick() and all other application network clients during flash writes.
    delay(1);
    return;
  }

  fridge_controller_loop();
}
