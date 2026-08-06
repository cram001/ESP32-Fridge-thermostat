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
EmergencySpilloverController emergency_spillover_controller;
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
bool menu_editing = false;
bool alarm_active = false;
bool alarm_acknowledged = false;
bool alarm_visual_active = false;
bool alarm_condition_present = false;
bool critical_probe_alarm = false;
bool temperature_alarm_armed = false;
uint32_t spillover_started_ms = 0;
uint8_t selected_error = 0;
uint32_t last_control_ms = 0;
uint32_t last_display_ms = 0;
uint32_t last_display_activity_ms = 0;
bool display_awake = true;
uint8_t applied_oled_contrast_percent = 0;
uint32_t last_menu_activity_ms = 0;
constexpr uint32_t kMenuActivityTimeoutMs = 10UL * 1000UL;
constexpr uint8_t kSettingCount = 21;
constexpr uint8_t kLayoutSetting = 17;
constexpr uint8_t kFirstAssignmentSetting = 18;
constexpr uint8_t kLastAssignmentSetting = 20;
uint32_t splash_started_ms = 0;
bool splash_active = true;
bool settings_dirty = false;
uint32_t settings_changed_ms = 0;
bool encoder_input_locked = false;
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
std::shared_ptr<SKOutput<float>>
    sk_detected_temp[TemperatureManager::kMaxSensors];
std::shared_ptr<SKOutput<String>>
    sk_detected_rom[TemperatureManager::kMaxSensors];

void write_fan(uint8_t pin, bool on) {
  digitalWrite(pin, on == hw::kFanActiveHigh ? HIGH : LOW);
}

void load_settings() { settings_store->load(); }

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
  if (!temperatures.poll(assigned_rom, calibration_c,
                         hw::kTemperaturePeriodMs)) {
    return;
  }
  fridge_c = temperatures.role_temperature(0);
  freezer_c = temperatures.role_temperature(1);
  ambient_c = temperatures.role_temperature(2);
  temperature_sample_ready = true;

  sk_fridge->set(std::isfinite(fridge_c) ? fridge_c + 273.15f : NAN);
  sk_freezer->set(std::isfinite(freezer_c) ? freezer_c + 273.15f : NAN);
  sk_ambient->set(std::isfinite(ambient_c) ? ambient_c + 273.15f : NAN);
  for (uint8_t sensor = 0; sensor < temperatures.detected_count(); ++sensor) {
    const float detected_c = temperatures.detected_temperature(sensor);
    sk_detected_temp[sensor]->set(
        std::isfinite(detected_c) ? detected_c + 273.15f : NAN);
    sk_detected_rom[sensor]->set(temperatures.detected_rom(sensor));
  }
}

void update_controller() {
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

  control_output = controller.update(fridge_c, freezer_c, settings,
                                     hw::kHysteresisC);

  if (fridge_status != Status::kOk) {
    control_output.spillover = emergency_spillover_controller.update(
        now, true, freezer_status == Status::kOk, freezer_c, settings);
    control_output.circulation = false;
  } else {
    emergency_spillover_controller.update(now, false, false, NAN, settings);
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
      (freezer_status != Status::kOk ||
       freezer_c < settings.freezer_alarm_c);
  if (reached_safe_temperature || now >= hw::kStartupAlarmGraceMs) {
    temperature_alarm_armed = true;
  }
  const bool temperature_alarm_condition = temperature_alarm_armed &&
      ((std::isfinite(fridge_c) && fridge_c >= settings.fridge_alarm_c) ||
       (std::isfinite(freezer_c) &&
        freezer_c >= settings.freezer_alarm_c));
  critical_probe_alarm = fridge_status != Status::kOk;
  alarm_condition_present = temperature_alarm_condition || critical_probe_alarm;
  if (!alarm_condition_present) {
    alarm_active = false;
    alarm_acknowledged = false;
  } else {
    alarm_active = true;
  }
  alarm_visual_active = alarm_active && !alarm_acknowledged;
  if (alarm_visual_active) {
    display_awake = true;
    last_display_activity_ms = now;
    display.set_enabled(true);
  }
  if (alarm_visual_active && settings.buzzer_enabled) {
    tone(hw::kBuzzerPin, hw::kBuzzerFrequencyHz);
  } else {
    noTone(hw::kBuzzerPin);
  }
  sk_alarm->set(alarm_active);
}

int32_t normalized_encoder_delta(int32_t current_position) {
  int32_t raw_delta = current_position - last_encoder_position;
  if (raw_delta > 512) raw_delta -= 1024;
  if (raw_delta < -512) raw_delta += 1024;
  last_encoder_position = current_position;
  return raw_delta;
}

uint8_t wrapped_index(uint8_t current, int32_t delta, uint8_t count) {
  if (count == 0) return 0;
  int32_t next = (static_cast<int32_t>(current) + delta) % count;
  if (next < 0) next += count;
  return static_cast<uint8_t>(next);
}

void update_encoder() {
  if (!encoder_available) return;
  const int32_t position = encoder.getEncoderValue();
  const int32_t delta = normalized_encoder_delta(position);
  const bool raw_button_down = encoder.detectButtonDown();
  const uint32_t now = millis();
  const bool raw_activity = delta != 0 || raw_button_down;

  if (encoder_input_locked) {
    if (raw_activity) {
      encoder_quiet_started_ms = 0;
    } else if (encoder_quiet_started_ms == 0) {
      encoder_quiet_started_ms = now;
    } else if (now - encoder_quiet_started_ms >=
               hw::kEncoderRecoveryQuietMs) {
      encoder_input_locked = false;
      encoder_quiet_started_ms = 0;
    }
    return;
  }
  if (abs(delta) > hw::kEncoderMaxDeltaPerPoll) {
    encoder_input_locked = true;
    encoder_quiet_started_ms = 0;
    return;
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
    last_display_activity_ms = now;
    last_menu_activity_ms = 0;
    display.set_enabled(true);
    return;
  }
  if (delta != 0 || button_down) last_display_activity_ms = now;

  if (alarm_visual_active) {
    if (button_down) {
      alarm_acknowledged = true;
      alarm_visual_active = false;
      noTone(hw::kBuzzerPin);
    }
    return;
  }

  if (assignment_mode) {
    const uint8_t detected_count = temperatures.detected_count();
    if (delta != 0 && detected_count > 0) {
      assignment_sensor =
          wrapped_index(assignment_sensor, delta, detected_count);
    }
    if (delta != 0 || button_down) last_menu_activity_ms = now;
    if (!button_down) return;

    if (detected_count > 0) {
      const String selected_rom = temperatures.detected_rom(assignment_sensor);
      for (uint8_t role = 0; role < 3; ++role) {
        if (role != assignment_role &&
            assigned_rom[role].equalsIgnoreCase(selected_rom)) {
          assigned_rom[role] = "";
        }
      }
      assigned_rom[assignment_role] = selected_rom;
      save_settings();
    }
    assignment_mode = false;
    assignment_sensor = 0;
    return;
  }

  const bool menu_active =
      last_menu_activity_ms != 0 &&
      now - last_menu_activity_ms < kMenuActivityTimeoutMs;
  if (!menu_active) {
    last_menu_activity_ms = 0;
    menu_editing = false;
    // Rotation on the home screen only counts as display activity. A button
    // press is the sole way to enter settings, and every entry starts at item 1.
    if (button_down) {
      selected_setting = 0;
      last_menu_activity_ms = now;
    }
    return;
  }

  if (delta != 0 || button_down) last_menu_activity_ms = now;

  if (!menu_editing) {
    if (delta != 0) {
      selected_setting =
          wrapped_index(selected_setting, delta, kSettingCount);
    }
    if (button_down) {
      if (selected_setting >= kFirstAssignmentSetting &&
          selected_setting <= kLastAssignmentSetting) {
        assignment_mode = true;
        assignment_role = selected_setting - kFirstAssignmentSetting;
        assignment_sensor = 0;
      } else {
        menu_editing = true;
      }
    }
    return;
  }

  bool setting_changed = false;
  if (delta != 0) {
    if (selected_setting <= 4) {
      const float step_c = display_fahrenheit
                               ? hw::kTemperatureEditStepC / 1.8f
                               : hw::kTemperatureEditStepC;
      if (selected_setting == 0) {
        settings.high_c = constrain(
            settings.high_c + delta * step_c,
            settings.low_c + hw::kHysteresisC,
            hw::kFridgeControlMaxC);
      } else if (selected_setting == 1) {
        settings.low_c = constrain(
            settings.low_c + delta * step_c,
            hw::kFridgeControlMinC,
            settings.high_c - hw::kHysteresisC);
      } else if (selected_setting == 2) {
        settings.freezer_lockout_c = constrain(
            settings.freezer_lockout_c + delta * step_c,
            hw::kFreezerThresholdMinC,
            hw::kFreezerThresholdMaxC);
      } else if (selected_setting == 3) {
        settings.fridge_alarm_c = constrain(
            settings.fridge_alarm_c + delta * step_c,
            hw::kFridgeAlarmMinC,
            hw::kFridgeAlarmMaxC);
      } else {
        settings.freezer_alarm_c = constrain(
            settings.freezer_alarm_c + delta * step_c,
            hw::kFreezerAlarmMinC,
            hw::kFreezerAlarmMaxC);
      }
      setting_changed = true;
    } else if (selected_setting == 5) {
      display_fahrenheit = !display_fahrenheit;
      setting_changed = true;
    } else if (selected_setting <= 8) {
      const uint8_t role = selected_setting - 6;
      float shown_offset = display_fahrenheit
                               ? calibration_c[role] * 1.8f
                               : calibration_c[role];
      const float shown_limit = display_fahrenheit
                                    ? hw::kCalibrationLimitC * 1.8f
                                    : hw::kCalibrationLimitC;
      shown_offset = constrain(
          shown_offset + delta * hw::kTemperatureEditStepC,
          -shown_limit, shown_limit);
      calibration_c[role] = display_fahrenheit ? shown_offset / 1.8f
                                                 : shown_offset;
      setting_changed = true;
    } else if (selected_setting == 9) {
      settings.fan_delay_s = constrain(
          static_cast<int>(settings.fan_delay_s) +
              delta * hw::kFanDelayStepS,
          static_cast<int>(hw::kFanDelayMinS),
          static_cast<int>(hw::kFanDelayMaxS));
      setting_changed = true;
    } else if (selected_setting == 10) {
      settings.spillover_min_on_min = constrain(
          static_cast<int>(settings.spillover_min_on_min) + delta,
          static_cast<int>(hw::kFanMinimumOnMin),
          static_cast<int>(hw::kFanMinimumOnMax));
      setting_changed = true;
    } else if (selected_setting == 11) {
      settings.circulation_min_on_min = constrain(
          static_cast<int>(settings.circulation_min_on_min) + delta,
          static_cast<int>(hw::kFanMinimumOnMin),
          static_cast<int>(hw::kFanMinimumOnMax));
      setting_changed = true;
    } else if (selected_setting == 12) {
      uint8_t option = 0;
      while (option < hw::kEmergencySpilloverOptionCount - 1 &&
             hw::kEmergencySpilloverOptions[option] !=
                 settings.emergency_spillover_on_min) {
        option++;
      }
      int32_t next_option =
          (static_cast<int32_t>(option) + delta) %
          hw::kEmergencySpilloverOptionCount;
      if (next_option < 0) next_option += hw::kEmergencySpilloverOptionCount;
      settings.emergency_spillover_on_min =
          hw::kEmergencySpilloverOptions[next_option];
      setting_changed = true;
    } else if (selected_setting == 13) {
      settings.buzzer_enabled = !settings.buzzer_enabled;
      setting_changed = true;
    } else if (selected_setting == 14 && faults.count() > 0) {
      selected_error = wrapped_index(selected_error, delta, faults.count());
    } else if (selected_setting == 15) {
      settings.oled_contrast_percent = constrain(
          static_cast<int>(settings.oled_contrast_percent) +
              delta * hw::kOledContrastStepPercent,
          static_cast<int>(hw::kOledContrastMinPercent),
          static_cast<int>(hw::kOledContrastMaxPercent));
      display.set_contrast(settings.oled_contrast_percent);
      setting_changed = true;
    } else if (selected_setting == 16) {
      uint8_t option = 0;
      while (option < hw::kDisplayTimeoutOptionCount - 1 &&
             hw::kDisplayTimeoutOptions[option] !=
                 settings.display_timeout_min) {
        option++;
      }
      int32_t next_option =
          (static_cast<int32_t>(option) + delta) %
          hw::kDisplayTimeoutOptionCount;
      if (next_option < 0) next_option += hw::kDisplayTimeoutOptionCount;
      settings.display_timeout_min = hw::kDisplayTimeoutOptions[next_option];
      setting_changed = true;
    } else if (selected_setting == kLayoutSetting) {
      settings.fridge_on_left = !settings.fridge_on_left;
      setting_changed = true;
    }
    if (setting_changed) {
      NormalizeControllerSettings(settings);
      mark_settings_dirty();
    }
  }

  if (button_down) menu_editing = false;
}

void update_display() {
  if (applied_oled_contrast_percent != settings.oled_contrast_percent) {
    display.set_contrast(settings.oled_contrast_percent);
    applied_oled_contrast_percent = settings.oled_contrast_percent;
  }
  if (display_awake && settings.display_timeout_min != 0 &&
      !alarm_visual_active &&
      millis() - last_display_activity_ms >=
          static_cast<uint32_t>(settings.display_timeout_min) *
              60UL * 1000UL) {
    display_awake = false;
    display.set_enabled(false);
  }
  if (!display_awake) return;

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
                     alarm_visual_active,
                     critical_probe_alarm,
                     assignment_mode,
                     menu_active,
                     menu_editing,
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
  for (uint8_t sensor = 0; sensor < temperatures.detected_count(); ++sensor) {
    const String number(sensor + 1);
    const String base_path =
        String("environment.inside.refrigerator.detectedProbe") + number;
    sk_detected_temp[sensor] =
        std::make_shared<SKOutput<float>>(base_path + ".temperature");
    sk_detected_rom[sensor] =
        std::make_shared<SKOutput<String>>(base_path + ".rom");
  }
}

}  // namespace

void setup() {
  SetupLogging(ESP_LOG_INFO);
  digitalWrite(hw::kSpilloverFanPin, hw::kFanActiveHigh ? LOW : HIGH);
  digitalWrite(hw::kCirculationFanPin, hw::kFanActiveHigh ? LOW : HIGH);
  pinMode(hw::kSpilloverFanPin, OUTPUT);
  pinMode(hw::kCirculationFanPin, OUTPUT);
  pinMode(hw::kBuzzerPin, OUTPUT);
  noTone(hw::kBuzzerPin);
  write_fan(hw::kSpilloverFanPin, false);
  write_fan(hw::kCirculationFanPin, false);

  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("fridge-controller")->get_app();
  ConfigItem(settings_store)
      ->set_title("Fridge Controller")
      ->set_description(
          "Thermostat, alarms, fan timing, display layout, calibration, and sensor assignments. "
          "Numeric limits shown below are enforced by the controller when settings are saved.")
      ->set_sort_order(500);
  load_settings();
  Wire.begin(hw::kI2cSdaPin, hw::kI2cSclPin);
  encoder_available = encoder.begin() == NO_ERR;
  if (encoder_available) {
    encoder.setGainCoefficient(hw::kEncoderGain);
    encoder.setEncoderValue(hw::kEncoderInitialValue);
    last_encoder_position = encoder.getEncoderValue();
  }
  display.begin();
  display.set_contrast(settings.oled_contrast_percent);
  applied_oled_contrast_percent = settings.oled_contrast_percent;
  last_display_activity_ms = millis();
  temperatures.begin();

  setup_signalk();
  sensesp_app->start();
  splash_started_ms = millis();
  const uint8_t splash_seconds = static_cast<uint8_t>(
      (hw::kSplashDurationMs + 999UL) / 1000UL);
  display.draw_splash(vessel_name.c_str(), hw::kFirmwareVersion,
                      temperatures.detected_count(), splash_seconds);
  write_fan(hw::kSpilloverFanPin, true);
  write_fan(hw::kCirculationFanPin, true);
}

void loop() {
  event_loop()->tick();
  const uint32_t now = millis();
  if (splash_active) {
    read_temperatures();
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
    // End the physical output test deterministically before normal control.
    write_fan(hw::kSpilloverFanPin, false);
    write_fan(hw::kCirculationFanPin, false);
    control_output.spillover = false;
    control_output.circulation = false;
    if (encoder_available) {
      last_encoder_position = encoder.getEncoderValue();
      encoder.detectButtonDown();
    }
    last_control_ms = now;
    last_display_ms = now;
    last_display_activity_ms = now;
    read_temperatures();
    update_controller();
    update_display();
    return;
  }

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
