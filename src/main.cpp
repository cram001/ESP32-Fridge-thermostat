#include <Arduino.h>
#include <DFRobot_VisualRotaryEncoder.h>
#include <Wire.h>

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

using namespace sensesp;

namespace {

// main.cpp coordinates the hardware-facing modules. Sensor discovery,
// persistence, display rendering, and thermostat decisions live in their own
// classes so the loop below remains a readable schedule of system activity.
DFRobot_VisualRotaryEncoder_I2C encoder(hw::kEncoderAddress, &Wire);
ControllerSettings settings;
float calibration_c[3] = {0.0f, 0.0f, 0.0f};
bool display_fahrenheit = false;
String assigned_rom[3];
String vessel_name = "FRIDGE CTRL";
auto settings_store = std::make_shared<SettingsStore>(
    settings, display_fahrenheit, calibration_c, assigned_rom, vessel_name);
TemperatureManager temperatures(hw::kOneWirePin);
FridgeDisplay display(hw::kOledCsPin, hw::kOledDcPin, hw::kOledResetPin,
                      hw::kPixelShiftPeriodMs);

FridgeController controller;
ControllerOutput control_output;
FaultManager faults;
float fridge_c = NAN;
float freezer_c = NAN;
float ambient_c = NAN;
bool temperature_sample_ready = false;
bool assignment_mode = false;
uint8_t assignment_role = 0;
uint8_t assignment_sensor = 0;
bool encoder_available = false;
int32_t last_encoder_position = 0;
uint8_t selected_setting = 0;
bool alarm_active = false;
bool alarm_condition_present = false;
bool critical_probe_alarm = false;
bool temperature_alarm_armed = false;
uint32_t alarm_snooze_until_ms = 0;
uint32_t fridge_failure_started_ms = 0;
uint32_t spillover_started_ms = 0;
uint8_t selected_error = 0;
uint32_t last_control_ms = 0;
uint32_t last_display_ms = 0;
uint32_t last_display_activity_ms = 0;
bool display_awake = true;
uint8_t applied_oled_contrast_percent = 0;
uint32_t last_menu_activity_ms = 0;
constexpr uint32_t kMenuActivityTimeoutMs = 10UL * 1000UL;
uint32_t splash_started_ms = 0;
bool splash_active = true;
bool settings_dirty = false;
uint32_t settings_changed_ms = 0;
bool encoder_input_locked = false;
uint32_t encoder_activity_started_ms = 0;
uint32_t encoder_quiet_started_ms = 0;
uint32_t last_button_event_ms = 0;
bool encoder_button_ready = true;

std::shared_ptr<SKOutput<float>> sk_fridge;
std::shared_ptr<SKOutput<float>> sk_freezer;
std::shared_ptr<SKOutput<float>> sk_ambient;
std::shared_ptr<SKOutput<bool>> sk_spillover;
std::shared_ptr<SKOutput<bool>> sk_circulation;
std::shared_ptr<SKOutput<bool>> sk_lockout;
std::shared_ptr<SKOutput<bool>> sk_fault;
std::shared_ptr<SKOutput<bool>> sk_alarm;

void write_fan(uint8_t pin, bool on) {
  digitalWrite(pin, on == hw::kFanActiveHigh ? HIGH : LOW);
}

void load_settings() {
  settings_store->load();
}

void save_settings() {
  settings_store->save();
  settings_dirty = false;
}

void mark_settings_dirty() {
  settings_dirty = true;
  settings_changed_ms = millis();
}

void save_settings_when_idle() {
  if (settings_dirty &&
      millis() - settings_changed_ms >= hw::kSettingsSaveDelayMs) {
    save_settings();
  }
}

void read_temperatures() {
  // poll() is non-blocking and paces its own conversion requests against
  // hw::kTemperaturePeriodMs, so this can safely run every loop() tick.
  if (!temperatures.poll(assigned_rom, calibration_c, hw::kTemperaturePeriodMs)) {
    return;
  }
  fridge_c = temperatures.role_temperature(0);
  freezer_c = temperatures.role_temperature(1);
  ambient_c = temperatures.role_temperature(2);
  temperature_sample_ready = true;

  // Signal K temperatures use kelvin.
  sk_fridge->set(std::isfinite(fridge_c) ? fridge_c + 273.15f : NAN);
  sk_freezer->set(std::isfinite(freezer_c) ? freezer_c + 273.15f : NAN);
  sk_ambient->set(std::isfinite(ambient_c) ? ambient_c + 273.15f : NAN);
}

void update_controller() {
  // Do not interpret the initial NAN placeholders as a failed fridge probe.
  // Outputs were latched OFF in setup and stay there until the first bus poll.
  if (!temperature_sample_ready) return;
  const uint32_t now = millis();
  using Status = TemperatureManager::SensorStatus;
  const Status fridge_status = temperatures.role_status(0);
  const Status freezer_status = temperatures.role_status(1);
  const Status ambient_status = temperatures.role_status(2);
  faults.set(FaultCode::kFridgeMissing, fridge_status == Status::kMissing);
  faults.set(FaultCode::kFridgeRange, fridge_status == Status::kOutOfRange);
  faults.set(FaultCode::kFreezerMissing, freezer_status == Status::kMissing);
  faults.set(FaultCode::kFreezerRange, freezer_status == Status::kOutOfRange);
  faults.set(FaultCode::kAmbientMissing, ambient_status == Status::kMissing);
  faults.set(FaultCode::kAmbientRange, ambient_status == Status::kOutOfRange);
  faults.set(FaultCode::kEncoderOffline, !encoder_available);
  faults.set(FaultCode::kEncoderErratic, encoder_input_locked);
  const bool sk_offline = now >= hw::kSignalKFaultGraceMs &&
      (!sensesp_app || !sensesp_app->get_ws_client()->is_connected());
  faults.set(FaultCode::kSignalKOffline, sk_offline);

  // The normal controller owns hysteresis, lockout, and start-delay state.
  control_output = controller.update(fridge_c, freezer_c, settings,
                                     hw::kHysteresisC);

  // Without the controlling fridge probe, use a bounded duty cycle instead of
  // leaving food warm indefinitely or running the spillover fan continuously.
  if (fridge_status != Status::kOk) {
    if (fridge_failure_started_ms == 0) fridge_failure_started_ms = now;
    const uint32_t on_ms = 5UL * 60UL * 1000UL;
    const uint32_t cycle_ms =
        (5UL + settings.failsafe_off_min) * 60UL * 1000UL;
    control_output.spillover =
        (now - fridge_failure_started_ms) % cycle_ms < on_ms;
    control_output.circulation = false;
  } else {
    fridge_failure_started_ms = 0;
  }

  if (control_output.spillover) {
    if (spillover_started_ms == 0) spillover_started_ms = now;
  } else {
    spillover_started_ms = 0;
  }
  faults.set(FaultCode::kSpilloverLongRun,
             spillover_started_ms != 0 &&
             now - spillover_started_ms >= hw::kLongFanRunMs);

  write_fan(hw::kSpilloverFanPin, control_output.spillover);
  write_fan(hw::kCirculationFanPin, control_output.circulation);
  sk_spillover->set(control_output.spillover);
  sk_circulation->set(control_output.circulation);
  sk_lockout->set(control_output.freezer_lockout);
  sk_fault->set(faults.count() > 0);

  const bool reached_safe_temperature =
      fridge_status == Status::kOk && fridge_c < settings.fridge_alarm_c &&
      (freezer_status != Status::kOk || freezer_c < settings.freezer_alarm_c);
  if (reached_safe_temperature || now >= hw::kStartupAlarmGraceMs) {
    temperature_alarm_armed = true;
  }
  const bool temperature_alarm_condition = temperature_alarm_armed &&
      ((std::isfinite(fridge_c) && fridge_c >= settings.fridge_alarm_c) ||
       (std::isfinite(freezer_c) && freezer_c >= settings.freezer_alarm_c));
  critical_probe_alarm = fridge_status != Status::kOk ||
                         freezer_status != Status::kOk;
  alarm_condition_present = temperature_alarm_condition ||
                            critical_probe_alarm;
  if (!alarm_condition_present) {
    // Clearing the fault also clears any remaining snooze time. A later,
    // genuinely new high-temperature event must alarm immediately.
    alarm_snooze_until_ms = 0;
    alarm_active = false;
  } else {
    // Signed subtraction makes this comparison safe when millis() wraps.
    const bool snoozed = alarm_snooze_until_ms != 0 &&
        static_cast<int32_t>(millis() - alarm_snooze_until_ms) < 0;
    alarm_active = !snoozed;
  }
  if (alarm_condition_present) {
    display_awake = true;
    last_display_activity_ms = now;
    display.set_enabled(true);
  }
  if (alarm_active && settings.buzzer_enabled) {
    tone(hw::kBuzzerPin, hw::kBuzzerFrequencyHz);
  }
  else noTone(hw::kBuzzerPin);
  sk_alarm->set(alarm_active);
}

void update_encoder() {
  if (!encoder_available) return;
  const int32_t position = encoder.getEncoderValue();
  int32_t delta = position - last_encoder_position;
  // SEN0502 count wraps at 1024.
  if (delta > 512) delta -= 1024;
  if (delta < -512) delta += 1024;
  last_encoder_position = position;
  const bool raw_button_down = encoder.detectButtonDown();
  const uint32_t now = millis();
  const bool raw_activity = delta != 0 || raw_button_down;

  // Contain corrupted I2C readings or a module that continuously generates
  // input. Normal operation resumes after two quiet seconds.
  if (encoder_input_locked) {
    if (raw_activity) {
      encoder_quiet_started_ms = 0;
    } else if (encoder_quiet_started_ms == 0) {
      encoder_quiet_started_ms = now;
    } else if (now - encoder_quiet_started_ms >=
               hw::kEncoderRecoveryQuietMs) {
      encoder_input_locked = false;
      encoder_activity_started_ms = 0;
      encoder_quiet_started_ms = 0;
    }
    return;
  }
  if (abs(delta) > hw::kEncoderMaxDeltaPerPoll) {
    encoder_input_locked = true;
    encoder_quiet_started_ms = 0;
    return;
  }
  if (raw_activity) {
    if (encoder_activity_started_ms == 0) encoder_activity_started_ms = now;
    if (now - encoder_activity_started_ms >=
        hw::kEncoderContinuousLimitMs) {
      encoder_input_locked = true;
      encoder_quiet_started_ms = 0;
      return;
    }
  } else {
    encoder_activity_started_ms = 0;
  }

  if (!raw_button_down) encoder_button_ready = true;
  const bool button_down = raw_button_down && encoder_button_ready &&
      (last_button_event_ms == 0 ||
       now - last_button_event_ms >= hw::kEncoderButtonGuardMs);
  if (button_down) {
    last_button_event_ms = now;
    encoder_button_ready = false;
  }
  if ((delta != 0 || button_down) && !display_awake) {
    display_awake = true;
    last_display_activity_ms = millis();
    last_menu_activity_ms = 0;
    display.set_enabled(true);
    return;
  }
  if (delta != 0 || button_down) {
    last_display_activity_ms = millis();
    if (!alarm_active && !assignment_mode) last_menu_activity_ms = millis();
  }
  if (delta != 0) {
    // Rotation selects a physical probe during assignment; otherwise it edits
    // whichever normal settings item is currently shown on the OLED.
    if (assignment_mode && temperatures.detected_count() > 0) {
      assignment_sensor =
          (assignment_sensor + delta + temperatures.detected_count() * 8) %
          temperatures.detected_count();
    } else if (selected_setting <= 4) {
      const float step_c = display_fahrenheit ? (0.5f / 1.8f) : 0.5f;
      if (selected_setting == 0) {
        settings.high_c = constrain(settings.high_c + delta * step_c,
                                    settings.low_c + hw::kHysteresisC, 30.0f);
      } else if (selected_setting == 1) {
        settings.low_c = constrain(settings.low_c + delta * step_c, -40.0f,
                                   settings.high_c - hw::kHysteresisC);
      } else if (selected_setting == 2) {
        settings.freezer_lockout_c = constrain(
            settings.freezer_lockout_c + delta * step_c, -40.0f, 20.0f);
      } else if (selected_setting == 3) {
        settings.fridge_alarm_c = constrain(
            settings.fridge_alarm_c + delta * step_c, -20.0f, 40.0f);
      } else {
        settings.freezer_alarm_c = constrain(
            settings.freezer_alarm_c + delta * step_c, -40.0f, 20.0f);
      }
    } else if (selected_setting == 5) {
      display_fahrenheit = !display_fahrenheit;
    } else if (selected_setting <= 8) {
      // Calibration is stored in Celsius but edited in the displayed unit.
      const uint8_t role = selected_setting - 6;
      float shown_offset = display_fahrenheit
                               ? calibration_c[role] * 1.8f
                               : calibration_c[role];
      const float shown_limit = display_fahrenheit ? 9.0f : 5.0f;
      shown_offset = constrain(shown_offset + delta * 0.1f,
                               -shown_limit, shown_limit);
      calibration_c[role] = display_fahrenheit ? shown_offset / 1.8f
                                                : shown_offset;
    } else if (selected_setting == 9) {
      settings.fan_delay_s = constrain(
          static_cast<int>(settings.fan_delay_s) + delta * 5, 5, 180);
    } else if (selected_setting == 10) {
      settings.spillover_min_on_min = constrain(
          static_cast<int>(settings.spillover_min_on_min) + delta, 1, 5);
    } else if (selected_setting == 11) {
      settings.circulation_min_on_min = constrain(
          static_cast<int>(settings.circulation_min_on_min) + delta, 1, 5);
    } else if (selected_setting == 12) {
      const int options[] = {5, 10, 15, 20};
      uint8_t option = settings.failsafe_off_min == 5 ? 0
                       : settings.failsafe_off_min == 10 ? 1
                       : settings.failsafe_off_min == 15 ? 2 : 3;
      option = (option + delta + 32) % 4;
      settings.failsafe_off_min = options[option];
    } else if (selected_setting == 13) {
      settings.buzzer_enabled = !settings.buzzer_enabled;
    } else if (selected_setting == 14 && faults.count() > 0) {
      selected_error = (selected_error + delta + faults.count() * 8) %
                       faults.count();
    } else if (selected_setting == 15) {
      settings.oled_contrast_percent = constrain(
          static_cast<int>(settings.oled_contrast_percent) + delta * 10,
          10, 100);
      display.set_contrast(settings.oled_contrast_percent);
    } else if (selected_setting == 16) {
      const uint8_t options[] = {0, 1, 5, 20, 30, 60};
      uint8_t option = 0;
      while (option < 5 &&
             options[option] != settings.display_timeout_min) option++;
      int32_t next_option = (static_cast<int32_t>(option) + delta) % 6;
      if (next_option < 0) next_option += 6;
      settings.display_timeout_min = options[next_option];
    }
    mark_settings_dirty();
  }
  if (button_down) {
    // Alarm acknowledgement has priority over all menu button actions.
    if (alarm_active) {
      alarm_snooze_until_ms = millis() + hw::kAlarmSnoozeMs;
      alarm_active = false;
      noTone(hw::kBuzzerPin);
    } else if (assignment_mode && temperatures.detected_count() == 0) {
      assignment_mode = false;
    } else if (assignment_mode && temperatures.detected_count() > 0) {
      const String selected_rom = temperatures.detected_rom(assignment_sensor);
      // A physical sensor may belong to only one logical location.
      for (uint8_t role = 0; role < 3; ++role) {
        if (role != assignment_role && assigned_rom[role].equalsIgnoreCase(selected_rom)) {
          assigned_rom[role] = "";
        }
      }
      assigned_rom[assignment_role] = selected_rom;
      save_settings();
      assignment_role++;
      if (assignment_role >= 3) {
        assignment_mode = false;
        assignment_role = 0;
        selected_setting = 0;
      }
    } else if (selected_setting == 17) {
      assignment_mode = true;
      assignment_role = 0;
      assignment_sensor = 0;
    } else {
      selected_setting = (selected_setting + 1) % 18;
    }
  }
}

void update_display() {
  if (applied_oled_contrast_percent != settings.oled_contrast_percent) {
    display.set_contrast(settings.oled_contrast_percent);
    applied_oled_contrast_percent = settings.oled_contrast_percent;
  }
  if (display_awake && settings.display_timeout_min != 0 &&
      !alarm_condition_present &&
      millis() - last_display_activity_ms >=
          static_cast<uint32_t>(settings.display_timeout_min) * 60UL * 1000UL) {
    display_awake = false;
    display.set_enabled(false);
  }
  if (!display_awake) return;
  // Build a read-only snapshot so the display module does not depend on or
  // mutate controller globals.
  const float role_temps[] = {fridge_c, freezer_c, ambient_c};
  const uint8_t count = temperatures.detected_count();
  const float assignment_temp =
      count > 0 ? temperatures.detected_temperature(assignment_sensor) : NAN;
  const String assignment_rom =
      count > 0 ? temperatures.detected_rom(assignment_sensor) : String();
  const uint8_t fault_count = faults.count();
  if (fault_count == 0 || selected_error >= fault_count) selected_error = 0;
  const FaultEntry fault = faults.entry(selected_error);
  const bool signalk_connected =
      sensesp_app && sensesp_app->get_ws_client()->is_connected();
  const bool menu_active = last_menu_activity_ms != 0 &&
      millis() - last_menu_activity_ms < kMenuActivityTimeoutMs;
  DisplayModel model{role_temps,
                     calibration_c,
                     &settings,
                     &control_output,
                     display_fahrenheit,
                     alarm_active,
                     critical_probe_alarm,
                     assignment_mode,
                     menu_active,
                     selected_setting,
                     assignment_role,
                     assignment_sensor,
                     count,
                     assignment_temp,
                     assignment_rom,
                     fault_count,
                     static_cast<uint8_t>(fault.code),
                     fault.message,
                     signalk_connected};
  display.draw(model);
}

void setup_signalk() {
  sk_fridge = std::make_shared<SKOutput<float>>(
      "environment.inside.refrigerator.temperature", "/Fridge/Temperature");
  sk_freezer = std::make_shared<SKOutput<float>>(
      "environment.inside.freezer.temperature", "/Freezer/Temperature");
  sk_ambient = std::make_shared<SKOutput<float>>(
      "environment.inside.cabin.temperature", "/Cabin/Temperature");
  sk_spillover = std::make_shared<SKOutput<bool>>(
      "electrical.switches.fridgeSpillover.state", "/Fridge/SpilloverFan");
  sk_circulation = std::make_shared<SKOutput<bool>>(
      "electrical.switches.fridgeCirculation.state", "/Fridge/CirculationFan");
  sk_lockout = std::make_shared<SKOutput<bool>>(
      "notifications.fridge.freezerLockout", "/Fridge/FreezerLockout");
  sk_fault = std::make_shared<SKOutput<bool>>(
      "notifications.fridge.sensorFault", "/Fridge/SensorFault");
  sk_alarm = std::make_shared<SKOutput<bool>>(
      "notifications.fridge.temperatureAlarm", "/Fridge/TemperatureAlarm");
}

}  // namespace

void setup() {
  SetupLogging(ESP_LOG_INFO);
  // Load the inactive level into each output latch before enabling the pin.
  digitalWrite(hw::kSpilloverFanPin, hw::kFanActiveHigh ? LOW : HIGH);
  digitalWrite(hw::kCirculationFanPin, hw::kFanActiveHigh ? LOW : HIGH);
  pinMode(hw::kSpilloverFanPin, OUTPUT);
  pinMode(hw::kCirculationFanPin, OUTPUT);
  pinMode(hw::kBuzzerPin, OUTPUT);
  noTone(hw::kBuzzerPin);
  write_fan(hw::kSpilloverFanPin, false);
  write_fan(hw::kCirculationFanPin, false);

  // Creating the SensESP app mounts the filesystem used by FileSystemSaveable.
  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("fridge-controller")->get_app();
  ConfigItem(settings_store)
      ->set_title("Fridge Controller")
      ->set_description("Thermostat, alarms, fan timing, calibration, and sensor assignments")
      ->set_sort_order(500);
  load_settings();
  Wire.begin(hw::kI2cSdaPin, hw::kI2cSclPin);
  encoder_available = encoder.begin() == NO_ERR;
  if (encoder_available) {
    encoder.setGainCoefficient(1);
    last_encoder_position = encoder.getEncoderValue();
  }
  display.begin();
  display.set_contrast(settings.oled_contrast_percent);
  applied_oled_contrast_percent = settings.oled_contrast_percent;
  last_display_activity_ms = millis();
  if (temperatures.begin(assigned_rom)) save_settings();

  setup_signalk();
  sensesp_app->start();
  splash_started_ms = millis();
  display.draw_splash(vessel_name.c_str(), hw::kFirmwareVersion,
                      temperatures.detected_count(), 30);
}

void loop() {
  // SensESP networking and these three periodic jobs share a cooperative loop;
  // none of the schedules below intentionally block or use delay().
  event_loop()->tick();
  const uint32_t now = millis();
  if (splash_active) {
    const uint32_t elapsed = now - splash_started_ms;
    if (elapsed < hw::kSplashDurationMs) {
      if (now - last_display_ms >= hw::kDisplayPeriodMs) {
        last_display_ms = now;
        const uint8_t remaining = static_cast<uint8_t>(
            (hw::kSplashDurationMs - elapsed + 999UL) / 1000UL);
        display.draw_splash(vessel_name.c_str(), hw::kFirmwareVersion,
                            temperatures.detected_count(), remaining);
      }
      return;
    }
    splash_active = false;
    if (encoder_available) {
      last_encoder_position = encoder.getEncoderValue();
      encoder.detectButtonDown();  // discard presses made during the splash
    }
    last_control_ms = now;
    last_display_ms = now;
    last_display_activity_ms = now;
    read_temperatures();
    update_controller();
    update_display();
    return;
  }
  // Non-blocking: paces its own conversion requests against
  // hw::kTemperaturePeriodMs internally, so it's safe to call every tick.
  read_temperatures();
  if (now - last_control_ms >= hw::kControlPeriodMs) {
    last_control_ms = now;
    update_encoder();
    update_controller();
    save_settings_when_idle();
  }
  if (now - last_display_ms >= hw::kDisplayPeriodMs) {
    last_display_ms = now;
    update_display();
  }
}
