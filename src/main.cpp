#include <Arduino.h>
#include <DFRobot_VisualRotaryEncoder.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#include "buzzer_controller.h"
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
BuzzerController buzzer(hw::kBuzzerPin);

FridgeController controller;
EmergencySpilloverController emergency_spillover_controller;
ControllerOutput control_output;
FaultManager faults;

float fridge_c = NAN;
float freezer_c = NAN;
float freezer_safety_c = NAN;
float ambient_c = NAN;
bool temperature_sample_ready = false;
bool temperature_sample_updated = false;


bool assignment_mode = false;
uint8_t assignment_role = 0;
uint8_t assignment_sensor = 0;

bool encoder_available = false;
uint32_t last_encoder_health_check_ms = 0;
uint8_t selected_setting = 0;
bool menu_editing = false;
bool edit_changed = false;
ControllerSettings edit_original_settings;
float edit_original_calibration_c[3] = {0.0f, 0.0f, 0.0f};
bool edit_original_fahrenheit = false;
bool edit_snapshot_valid = false;

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
constexpr uint32_t kSavedMessageDurationMs = 750;
constexpr uint32_t kOutputTestDurationMs = 5UL * 1000UL;
constexpr uint8_t kSettingCount = 23;
constexpr uint8_t kLayoutSetting = 17;
constexpr uint8_t kOutputTestSetting = 18;
constexpr uint8_t kFirstAssignmentSetting = 19;
constexpr uint8_t kLastAssignmentSetting = 21;
constexpr uint8_t kAboutSetting = 22;
constexpr uint8_t kOutputTestOptionCount = 4;
constexpr uint8_t kOutputTestSpillover = 0;
constexpr uint8_t kOutputTestCirculation = 1;
constexpr uint8_t kOutputTestBuzzer = 2;
constexpr uint8_t kOutputTestExit = 3;

bool output_test_mode = false;
bool output_test_active = false;
uint8_t output_test_selection = 0;
uint32_t output_test_ends_ms = 0;
uint32_t saved_message_until_ms = 0;

uint32_t splash_started_ms = 0;
bool splash_active = true;

bool encoder_input_locked = false;
uint32_t encoder_quiet_started_ms = 0;
int32_t encoder_counterclockwise_substeps = 0;
uint32_t last_button_event_ms = 0;
bool encoder_button_ready = true;

bool task_watchdog_enabled = false;

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

void setup_task_watchdog() {
  if (esp_task_wdt_init(hw::kTaskWatchdogTimeoutS, true) != ESP_OK) return;

  esp_err_t status = esp_task_wdt_status(nullptr);
  if (status == ESP_ERR_NOT_FOUND) status = esp_task_wdt_add(nullptr);
  task_watchdog_enabled = status == ESP_OK;
}

void feed_task_watchdog() {
  if (task_watchdog_enabled) esp_task_wdt_reset();
}

void write_fan(uint8_t pin, bool on) {
  digitalWrite(pin, on == hw::kFanActiveHigh ? HIGH : LOW);
}

void restore_normal_physical_outputs() {
  write_fan(hw::kSpilloverFanPin, control_output.spillover);
  write_fan(hw::kCirculationFanPin, control_output.circulation);
}

void stop_output_test() {
  const bool buzzer_was_tested =
      output_test_active && output_test_selection == kOutputTestBuzzer;
  output_test_active = false;
  output_test_ends_ms = 0;
  restore_normal_physical_outputs();
  if (buzzer_was_tested) buzzer.stop();
}

void start_output_test(uint32_t now) {
  if (output_test_selection >= kOutputTestExit) return;

  output_test_active = true;
  output_test_ends_ms = now + kOutputTestDurationMs;

  write_fan(hw::kSpilloverFanPin, false);
  write_fan(hw::kCirculationFanPin, false);
  buzzer.stop();

  if (output_test_selection == kOutputTestSpillover) {
    write_fan(hw::kSpilloverFanPin, true);
  } else if (output_test_selection == kOutputTestCirculation) {
    write_fan(hw::kCirculationFanPin, true);
  } else if (output_test_selection == kOutputTestBuzzer) {
    buzzer.test(settings.buzzer_mode, now, kOutputTestDurationMs);
  }
}

void service_output_test(uint32_t now) {
  if (!output_test_active) return;

  if (alarm_visual_active ||
      static_cast<int32_t>(now - output_test_ends_ms) >= 0) {
    stop_output_test();
    return;
  }

  write_fan(hw::kSpilloverFanPin,
            output_test_selection == kOutputTestSpillover);
  write_fan(hw::kCirculationFanPin,
            output_test_selection == kOutputTestCirculation);
}

void load_settings() {
  settings_store->load();
  if (settings_store->startup_defaults_restored()) {
    settings_store->save();
  }
  settings_store->finish_startup_validation();
}

void save_settings() { settings_store->save(); }

void show_saved_message(uint32_t now) {
  saved_message_until_ms = now + kSavedMessageDurationMs;
  display_awake = true;
  last_display_activity_ms = now;
  display.set_enabled(true);
}

void capture_edit_snapshot() {
  edit_original_settings = settings;
  edit_original_fahrenheit = display_fahrenheit;
  for (uint8_t role = 0; role < 3; ++role) {
    edit_original_calibration_c[role] = calibration_c[role];
  }
  edit_snapshot_valid = true;
}

void discard_edit_snapshot() {
  edit_snapshot_valid = false;
  edit_changed = false;
}

void rollback_edit() {
  if (!edit_snapshot_valid) {
    edit_changed = false;
    return;
  }

  settings = edit_original_settings;
  display_fahrenheit = edit_original_fahrenheit;
  for (uint8_t role = 0; role < 3; ++role) {
    calibration_c[role] = edit_original_calibration_c[role];
  }
  NormalizeControllerSettings(settings);

  display.set_contrast(settings.oled_contrast_percent);
  applied_oled_contrast_percent = settings.oled_contrast_percent;
  buzzer.stop();

  edit_snapshot_valid = false;
  edit_changed = false;
}

void publish_detected_probe_metadata() {
  // ROM metadata changes only when the debounced OneWire discovery list
  // changes. String construction here is therefore an exceptional event, not
  // a periodic/hot-path allocation.
  for (uint8_t sensor = 0; sensor < TemperatureManager::kMaxSensors; ++sensor) {
    char rom[17];
    temperatures.detected_rom_chars(sensor, rom);
    sk_detected_rom[sensor]->set(String(rom));
  }
}

void read_temperatures() {
  if (!temperatures.poll(assigned_rom, calibration_c,
                         hw::kTemperaturePeriodMs)) {
    return;
  }

  fridge_c = temperatures.role_temperature(0);
  freezer_c = temperatures.role_temperature(1);
  freezer_safety_c = temperatures.role_raw_temperature(1);
  ambient_c = temperatures.role_temperature(2);
  temperature_sample_ready = true;
  temperature_sample_updated = true;

  sk_fridge->set(std::isfinite(fridge_c) ? fridge_c + 273.15f : NAN);
  sk_freezer->set(std::isfinite(freezer_c) ? freezer_c + 273.15f : NAN);
  sk_ambient->set(std::isfinite(ambient_c) ? ambient_c + 273.15f : NAN);

  const uint8_t count = temperatures.detected_count();
  for (uint8_t sensor = 0; sensor < TemperatureManager::kMaxSensors; ++sensor) {
    const float detected_c =
        sensor < count ? temperatures.detected_temperature(sensor) : NAN;
    sk_detected_temp[sensor]->set(
        std::isfinite(detected_c) ? detected_c + 273.15f : NAN);
  }

  if (temperatures.take_discovery_changed()) {
    publish_detected_probe_metadata();
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
  faults.set(FaultCode::kFridgeReadFailed,
             fridge_status == Status::kReadFailed);
  faults.set(FaultCode::kFreezerReadFailed,
             freezer_status == Status::kReadFailed);
  faults.set(FaultCode::kAmbientReadFailed,
             ambient_status == Status::kReadFailed);
  faults.set(FaultCode::kEncoderOffline, !encoder_available);
  faults.set(FaultCode::kEncoderErratic, encoder_input_locked);

  const bool sk_offline =
      now >= hw::kSignalKFaultGraceMs &&
      (!sensesp_app || !sensesp_app->get_ws_client()->is_connected());
  faults.set(FaultCode::kSignalKOffline, sk_offline);

  control_output = controller.update(fridge_c, freezer_safety_c, settings,
                                     temperature_sample_updated);
  temperature_sample_updated = false;

  if (fridge_status != Status::kOk) {
    control_output.spillover = emergency_spillover_controller.update(
        now, true, freezer_status == Status::kOk, freezer_safety_c, settings);
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

  const bool temperature_alarm_condition =
      temperature_alarm_armed &&
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

  buzzer.update(now, alarm_visual_active, settings.buzzer_mode);
  sk_alarm->set(alarm_active);
}

uint8_t option_index(const uint8_t* options, uint8_t count, uint8_t value) {
  if (count == 0) return 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (options[i] == value) return i;
  }
  return 0;
}

void set_encoder_navigation_mode() {
  if (!encoder_available) return;
  encoder.setGainCoefficient(hw::kEncoderNavigationGain);
  encoder.setEncoderValue(hw::kEncoderNeutralValue);
  encoder_counterclockwise_substeps = 0;
}

bool encoder_bus_present() {
  Wire.beginTransmission(hw::kEncoderAddress);
  return Wire.endTransmission() == 0;
}

void check_encoder_health(uint32_t now) {
  if (now - last_encoder_health_check_ms < hw::kEncoderHealthCheckIntervalMs) {
    return;
  }
  last_encoder_health_check_ms = now;

  const bool present = encoder_bus_present();
  if (present == encoder_available) return;

  if (present) {
    encoder_available = encoder.begin() == NO_ERR;
    if (encoder_available) {
      set_encoder_navigation_mode();
      encoder.detectButtonDown();
    }
  } else {
    if (menu_editing) rollback_edit();
    encoder_available = false;
    encoder_input_locked = false;
    encoder_quiet_started_ms = 0;
    if (output_test_active) stop_output_test();
    output_test_mode = false;
    assignment_mode = false;
    menu_editing = false;
    edit_changed = false;
  }
}

int32_t read_encoder_delta() {
  const int32_t position = encoder.getEncoderValue();
  const int32_t delta =
      position - static_cast<int32_t>(hw::kEncoderNeutralValue);
  if (delta != 0) {
    encoder.setEncoderValue(hw::kEncoderNeutralValue);
  }
  return delta;
}

uint8_t wrapped_index(uint8_t current, int32_t delta, uint8_t count) {
  if (count == 0) return 0;
  int32_t next = (static_cast<int32_t>(current) + delta) % count;
  if (next < 0) next += count;
  return static_cast<uint8_t>(next);
}

void update_encoder() {
  if (!encoder_available) return;

  const int32_t raw_delta = read_encoder_delta();
  const bool raw_button_down = encoder.detectButtonDown();
  if (raw_button_down) {
    encoder.setEncoderValue(hw::kEncoderNeutralValue);
    encoder_counterclockwise_substeps = 0;
  }

  const uint32_t now = millis();
  const bool raw_activity = raw_delta != 0 || raw_button_down;

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

  if (abs(raw_delta) > hw::kEncoderMaxDeltaPerPoll) {
    encoder_input_locked = true;
    encoder_quiet_started_ms = 0;
    encoder_counterclockwise_substeps = 0;
    return;
  }

  int32_t delta = 0;
  if (raw_delta > 0) {
    encoder_counterclockwise_substeps = 0;
    delta = raw_delta;
  } else if (raw_delta < 0) {
    const int32_t accumulated_counts =
        encoder_counterclockwise_substeps - raw_delta;
    delta = -(accumulated_counts /
              hw::kEncoderCounterclockwiseCountsPerDetent);
    encoder_counterclockwise_substeps =
        accumulated_counts % hw::kEncoderCounterclockwiseCountsPerDetent;
  }

  if (!raw_button_down) encoder_button_ready = true;
  const bool button_down =
      raw_button_down && encoder_button_ready &&
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
    set_encoder_navigation_mode();
    display.set_enabled(true);
    return;
  }
  if (delta != 0 || button_down) last_display_activity_ms = now;

  if (alarm_visual_active) {
    if (button_down) {
      if (menu_editing) rollback_edit();
      alarm_acknowledged = true;
      alarm_visual_active = false;
      menu_editing = false;
      edit_changed = false;
      if (output_test_active) stop_output_test();
      output_test_mode = false;
      set_encoder_navigation_mode();
      buzzer.stop();
    }
    return;
  }

  if (output_test_mode) {
    if (!output_test_active && last_menu_activity_ms != 0 &&
        now - last_menu_activity_ms >= kMenuActivityTimeoutMs) {
      output_test_mode = false;
      output_test_selection = 0;
      set_encoder_navigation_mode();
      return;
    }

    if (output_test_active) {
      if (button_down) {
        stop_output_test();
        last_menu_activity_ms = now;
      }
      return;
    }

    if (delta != 0) {
      output_test_selection =
          wrapped_index(output_test_selection, delta, kOutputTestOptionCount);
      last_menu_activity_ms = now;
    }

    if (button_down) {
      last_menu_activity_ms = now;
      if (output_test_selection == kOutputTestExit) {
        output_test_mode = false;
        output_test_selection = 0;
        set_encoder_navigation_mode();
      } else {
        start_output_test(now);
      }
    }
    return;
  }

  if (assignment_mode) {
    const uint8_t detected_count = temperatures.detected_count();
    if (delta != 0) {
      assignment_sensor =
          wrapped_index(assignment_sensor, delta, detected_count + 1);
    }
    if (delta != 0 || button_down) last_menu_activity_ms = now;
    if (!button_down) return;

    if (assignment_sensor >= detected_count) {
      assigned_rom[assignment_role] = "";
      save_settings();
    } else {
      char selected_rom[17];
      temperatures.detected_rom_chars(assignment_sensor, selected_rom);
      for (uint8_t role = 0; role < 3; ++role) {
        if (role != assignment_role &&
            assigned_rom[role].equalsIgnoreCase(selected_rom)) {
          assigned_rom[role] = "";
        }
      }
      assigned_rom[assignment_role] = selected_rom;
      save_settings();
    }

    show_saved_message(now);
    assignment_mode = false;
    assignment_sensor = 0;
    set_encoder_navigation_mode();
    return;
  }

  const bool menu_active =
      last_menu_activity_ms != 0 &&
      now - last_menu_activity_ms < kMenuActivityTimeoutMs;

  if (!menu_active) {
    last_menu_activity_ms = 0;
    if (menu_editing) {
      rollback_edit();
      menu_editing = false;
      edit_changed = false;
      set_encoder_navigation_mode();
    }
    if (button_down) {
      selected_setting = 0;
      last_menu_activity_ms = now;
      set_encoder_navigation_mode();
    }
    return;
  }

  if (delta != 0 || button_down) last_menu_activity_ms = now;

  if (!menu_editing) {
    if (delta != 0) {
      selected_setting = wrapped_index(selected_setting, delta, kSettingCount);
    }

    if (button_down) {
      if (selected_setting == kOutputTestSetting) {
        output_test_mode = true;
        output_test_active = false;
        output_test_selection = 0;
        set_encoder_navigation_mode();
      } else if (selected_setting >= kFirstAssignmentSetting &&
                 selected_setting <= kLastAssignmentSetting) {
        assignment_mode = true;
        assignment_role = selected_setting - kFirstAssignmentSetting;
        const uint8_t detected_count = temperatures.detected_count();
        assignment_sensor = detected_count;
        for (uint8_t sensor = 0; sensor < detected_count; ++sensor) {
          char sensor_rom[17];
          temperatures.detected_rom_chars(sensor, sensor_rom);
          if (assigned_rom[assignment_role].equalsIgnoreCase(sensor_rom)) {
            assignment_sensor = sensor;
            break;
          }
        }
        set_encoder_navigation_mode();
      } else {
        if (selected_setting != 14 && selected_setting != kAboutSetting) {
          capture_edit_snapshot();
        }
        menu_editing = true;
        edit_changed = false;
        // Editing deliberately uses the same proven gain-1 detent handling as
        // menu navigation. The SEN0502 LED setting gauge is disabled.
        set_encoder_navigation_mode();
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
            settings.low_c + hw::kFridgeControlMinimumBandC,
            hw::kFridgeControlMaxC);
      } else if (selected_setting == 1) {
        settings.low_c = constrain(
            settings.low_c + delta * step_c,
            hw::kFridgeControlMinC,
            settings.high_c - hw::kFridgeControlMinimumBandC);
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
      int32_t next_mode =
          (static_cast<int32_t>(settings.buzzer_mode) + delta) %
          hw::kBuzzerModeCount;
      if (next_mode < 0) next_mode += hw::kBuzzerModeCount;
      settings.buzzer_mode = static_cast<uint8_t>(next_mode);
      buzzer.preview(settings.buzzer_mode, now);
      setting_changed = true;
    } else if (selected_setting == 14 && faults.count() > 0) {
      selected_error = wrapped_index(selected_error, delta, faults.count());
    } else if (selected_setting == 15) {
      uint8_t option = 0;
      while (option < hw::kOledContrastOptionCount - 1 &&
             hw::kOledContrastOptions[option] !=
                 settings.oled_contrast_percent) {
        option++;
      }
      int32_t next_option =
          (static_cast<int32_t>(option) + delta) %
          hw::kOledContrastOptionCount;
      if (next_option < 0) next_option += hw::kOledContrastOptionCount;
      settings.oled_contrast_percent =
          hw::kOledContrastOptions[next_option];
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
      edit_changed = true;
    }
  }

  if (button_down) {
    if (selected_setting != 14 && selected_setting != kAboutSetting) {
      // Avoid unnecessary filesystem writes if edit was entered/exited without
      // changing anything, while keeping the user-facing commit confirmation.
      if (edit_changed) save_settings();
      show_saved_message(now);
      discard_edit_snapshot();
    }
    menu_editing = false;
    edit_changed = false;
    set_encoder_navigation_mode();
  }
}

void update_display() {
  const uint32_t now = millis();

  if (applied_oled_contrast_percent != settings.oled_contrast_percent) {
    display.set_contrast(settings.oled_contrast_percent);
    applied_oled_contrast_percent = settings.oled_contrast_percent;
  }

  if (display_awake && settings.display_timeout_min != 0 &&
      !alarm_visual_active &&
      now - last_display_activity_ms >=
          static_cast<uint32_t>(settings.display_timeout_min) *
              60UL * 1000UL) {
    display_awake = false;
    display.set_enabled(false);
  }
  if (!display_awake) return;

  if (!alarm_visual_active && output_test_mode) {
    uint8_t remaining = 0;
    if (output_test_active &&
        static_cast<int32_t>(output_test_ends_ms - now) > 0) {
      remaining = static_cast<uint8_t>(
          (output_test_ends_ms - now + 999UL) / 1000UL);
    }
    display.draw_output_test(output_test_selection, output_test_active,
                             remaining);
    return;
  }

  if (!alarm_visual_active &&
      static_cast<int32_t>(saved_message_until_ms - now) > 0) {
    display.draw_saved();
    return;
  }

  const float role_temps[] = {fridge_c, freezer_c, ambient_c};
  const uint8_t count = temperatures.detected_count();
  const float assignment_temp =
      assignment_sensor < count
          ? temperatures.detected_temperature(assignment_sensor)
          : NAN;
  char assignment_rom[17] = {};
  if (assignment_sensor < count) {
    temperatures.detected_rom_chars(assignment_sensor, assignment_rom);
  }

  const uint8_t fault_count = faults.count();
  if (fault_count == 0 || selected_error >= fault_count) selected_error = 0;
  const FaultEntry fault = faults.entry(selected_error);
  const bool signalk_connected =
      sensesp_app && sensesp_app->get_ws_client()->is_connected();
  const bool menu_active =
      last_menu_activity_ms != 0 &&
      now - last_menu_activity_ms < kMenuActivityTimeoutMs;

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

  // Allocate all possible outputs once during setup. Runtime sensor discovery
  // then needs no new heap objects when a probe is replaced or reconnected.
  for (uint8_t sensor = 0; sensor < TemperatureManager::kMaxSensors; ++sensor) {
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
  buzzer.begin();
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
    set_encoder_navigation_mode();
  }

  display.begin();
  display.set_contrast(settings.oled_contrast_percent);
  applied_oled_contrast_percent = settings.oled_contrast_percent;
  last_display_activity_ms = millis();
  temperatures.begin();

  setup_signalk();
  sensesp_app->start();

  setup_task_watchdog();

  splash_started_ms = millis();
  const uint8_t splash_seconds = static_cast<uint8_t>(
      (hw::kSplashDurationMs + 999UL) / 1000UL);
  display.draw_splash(vessel_name.c_str(), hw::kFirmwareVersion,
                      temperatures.detected_count(), splash_seconds);
  write_fan(hw::kSpilloverFanPin, true);
  write_fan(hw::kCirculationFanPin, true);
}

void loop() {
  feed_task_watchdog();
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
    write_fan(hw::kSpilloverFanPin, false);
    write_fan(hw::kCirculationFanPin, false);
    control_output.spillover = false;
    control_output.circulation = false;

    if (encoder_available) {
      set_encoder_navigation_mode();
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
    check_encoder_health(now);
    update_encoder();
    update_controller();
    service_output_test(now);
  }

  if (now - last_display_ms >= hw::kDisplayPeriodMs) {
    last_display_ms = now;
    update_display();
  }
}
