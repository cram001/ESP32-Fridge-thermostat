from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}: {old[:80]!r}")
    p.write_text(text.replace(old, new, 1))


# ----- MQTT publisher API / behavior -----
replace_once(
    "include/cerbo_mqtt.h",
    "  bool enabled() const { return publish_interval_ms_ != 0 && host_[0] != '\\0'; }\n\n  // PubSubClient::connected() is not declared const even though we use it only\n  // as a status query here. Keep the public status API const so the display can\n  // inspect connection state without owning or mutating publisher state.\n  bool connected() const {\n    return enabled() && const_cast<PubSubClient&>(mqtt_).connected();\n  }\n",
    "  // Enabled reflects the user's reporting choice. A missing/invalid broker\n  // host is therefore shown as enabled-but-disconnected rather than silently\n  // hiding the MQTT status indicator.\n  bool enabled() const { return publish_interval_ms_ != 0; }\n  bool connected() { return enabled() && mqtt_.connected(); }\n",
)

replace_once(
    "src/cerbo_mqtt.cpp",
    "CerboMqttPublisher::CerboMqttPublisher() : mqtt_(network_client_) {\n  build_client_id();\n}\n",
    "CerboMqttPublisher::CerboMqttPublisher() : mqtt_(network_client_) {\n  build_client_id();\n  // Keep a local-broker outage from holding the application in PubSubClient's\n  // CONNACK wait for its much longer default timeout. TCP connect itself is\n  // still provided by WiFiClient, so reconnect attempts remain rate-limited.\n  mqtt_.setSocketTimeout(1);\n  mqtt_.setKeepAlive(30);\n}\n",
)

replace_once(
    "src/cerbo_mqtt.cpp",
    "void CerboMqttPublisher::reconnect(uint32_t now) {\n  if (!enabled() || WiFi.status() != WL_CONNECTED) return;\n",
    "void CerboMqttPublisher::reconnect(uint32_t now) {\n  if (!enabled() || host_[0] == '\\0' || WiFi.status() != WL_CONNECTED) return;\n",
)

# ----- Display model: keep network state outside the renderer -----
replace_once(
    "include/fridge_display.h",
    "  const char* fault_message;\n  bool signalk_connected;\n};\n",
    "  const char* fault_message;\n  bool signalk_connected;\n  uint16_t cerbo_mqtt_interval_s;\n  bool cerbo_mqtt_connected;\n};\n",
)

replace_once(
    "src/fridge_display.cpp",
    '#include "cerbo_mqtt.h"\n',
    '#include "cerbo_mqtt_interval.h"\n',
)

replace_once(
    "src/fridge_display.cpp",
    "constexpr uint8_t kSettingCount = 23;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kOutputTestSetting = 18;\nconstexpr uint8_t kFirstAssignmentSetting = 19;\nconstexpr uint8_t kLastAssignmentSetting = 21;\nconstexpr uint8_t kAboutSetting = 22;\n",
    "constexpr uint8_t kSettingCount = 24;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kCerboMqttSetting = 18;\nconstexpr uint8_t kOutputTestSetting = 19;\nconstexpr uint8_t kFirstAssignmentSetting = 20;\nconstexpr uint8_t kLastAssignmentSetting = 22;\nconstexpr uint8_t kAboutSetting = 23;\n",
)

replace_once(
    "src/fridge_display.cpp",
    "  const CerboMqttPublisher& mqtt = cerbo_mqtt_publisher();\n  if (mqtt.enabled()) {\n",
    "  if (model.cerbo_mqtt_interval_s != 0) {\n",
)

replace_once(
    "src/fridge_display.cpp",
    "    if (!mqtt.connected()) {\n",
    "    if (!model.cerbo_mqtt_connected) {\n",
)

replace_once(
    "src/fridge_display.cpp",
    "  } else if (model.selected_setting == kLayoutSetting) {\n    t.name = \"Display layout\";\n    snprintf(t.value, sizeof(t.value), \"%s\",\n             model.settings->fridge_on_left ? \"FRDG | FRZ\" : \"FRZ | FRDG\");\n  } else if (model.selected_setting == kOutputTestSetting) {\n",
    "  } else if (model.selected_setting == kLayoutSetting) {\n    t.name = \"Display layout\";\n    snprintf(t.value, sizeof(t.value), \"%s\",\n             model.settings->fridge_on_left ? \"FRDG | FRZ\" : \"FRZ | FRDG\");\n  } else if (model.selected_setting == kCerboMqttSetting) {\n    t.name = \"Cerbo MQTT\";\n    snprintf(t.value, sizeof(t.value), \"%s\",\n             cerbo_mqtt::ReportIntervalLabel(model.cerbo_mqtt_interval_s));\n  } else if (model.selected_setting == kOutputTestSetting) {\n",
)

# ----- Main application integration -----
replace_once(
    "src/main.cpp",
    '#include "buzzer_controller.h"\n',
    '#include "buzzer_controller.h"\n#include "cerbo_mqtt.h"\n#include "cerbo_mqtt_interval.h"\n#include "cerbo_mqtt_settings.h"\n',
)

replace_once(
    "src/main.cpp",
    "auto settings_store = std::make_shared<SettingsStore>(\n    settings, display_fahrenheit, calibration_c, assigned_rom, vessel_name);\nTemperatureManager temperatures(hw::kOneWirePin);\n",
    "auto settings_store = std::make_shared<SettingsStore>(\n    settings, display_fahrenheit, calibration_c, assigned_rom, vessel_name);\nCerboMqttSettings cerbo_mqtt_settings;\nauto cerbo_mqtt_settings_store =\n    std::make_shared<CerboMqttSettingsStore>(cerbo_mqtt_settings);\nTemperatureManager temperatures(hw::kOneWirePin);\n",
)

replace_once(
    "src/main.cpp",
    "bool edit_original_fahrenheit = false;\nbool edit_snapshot_valid = false;\n",
    "bool edit_original_fahrenheit = false;\nuint16_t edit_original_cerbo_mqtt_interval_s = 0;\nbool edit_snapshot_valid = false;\n",
)

replace_once(
    "src/main.cpp",
    "constexpr uint8_t kSettingCount = 23;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kOutputTestSetting = 18;\nconstexpr uint8_t kFirstAssignmentSetting = 19;\nconstexpr uint8_t kLastAssignmentSetting = 21;\nconstexpr uint8_t kAboutSetting = 22;\n",
    "constexpr uint8_t kSettingCount = 24;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kCerboMqttSetting = 18;\nconstexpr uint8_t kOutputTestSetting = 19;\nconstexpr uint8_t kFirstAssignmentSetting = 20;\nconstexpr uint8_t kLastAssignmentSetting = 22;\nconstexpr uint8_t kAboutSetting = 23;\n",
)

replace_once(
    "src/main.cpp",
    "bool task_watchdog_enabled = false;\n\nstd::shared_ptr<SKOutput<float>> sk_fridge;\n",
    "bool task_watchdog_enabled = false;\n\n// Fixed snapshots let us notice SensESP web-UI edits without allocating in the\n// main loop. Configuration strings themselves are persistent UI state; the hot\n// MQTT publish path remains fixed-buffer only.\nchar applied_mqtt_host[64] = {};\nchar applied_mqtt_username[48] = {};\nchar applied_mqtt_password[48] = {};\nuint16_t applied_mqtt_port = 0;\nuint16_t applied_mqtt_interval_s = UINT16_MAX;\n\nstd::shared_ptr<SKOutput<float>> sk_fridge;\n",
)

replace_once(
    "src/main.cpp",
    "void save_settings() { settings_store->save(); }\n\nvoid show_saved_message(uint32_t now) {\n",
    "void save_settings() { settings_store->save(); }\n\nbool cerbo_mqtt_config_changed() {\n  return strcmp(applied_mqtt_host, cerbo_mqtt_settings.host.c_str()) != 0 ||\n         strcmp(applied_mqtt_username, cerbo_mqtt_settings.username.c_str()) !=\n             0 ||\n         strcmp(applied_mqtt_password, cerbo_mqtt_settings.password.c_str()) !=\n             0 ||\n         applied_mqtt_port != cerbo_mqtt_settings.port ||\n         applied_mqtt_interval_s != cerbo_mqtt_settings.report_interval_s;\n}\n\nvoid apply_cerbo_mqtt_config() {\n  const uint32_t interval_ms =\n      static_cast<uint32_t>(cerbo_mqtt_settings.report_interval_s) * 1000UL;\n  cerbo_mqtt_publisher().configure(\n      cerbo_mqtt_settings.host.c_str(), cerbo_mqtt_settings.port,\n      cerbo_mqtt_settings.username.c_str(), cerbo_mqtt_settings.password.c_str(),\n      interval_ms);\n\n  snprintf(applied_mqtt_host, sizeof(applied_mqtt_host), \"%s\",\n           cerbo_mqtt_settings.host.c_str());\n  snprintf(applied_mqtt_username, sizeof(applied_mqtt_username), \"%s\",\n           cerbo_mqtt_settings.username.c_str());\n  snprintf(applied_mqtt_password, sizeof(applied_mqtt_password), \"%s\",\n           cerbo_mqtt_settings.password.c_str());\n  applied_mqtt_port = cerbo_mqtt_settings.port;\n  applied_mqtt_interval_s = cerbo_mqtt_settings.report_interval_s;\n}\n\nvoid service_cerbo_mqtt(uint32_t now) {\n  if (cerbo_mqtt_config_changed()) apply_cerbo_mqtt_config();\n  cerbo_mqtt_publisher().service(now, fridge_c, freezer_c, ambient_c);\n}\n\nvoid show_saved_message(uint32_t now) {\n",
)

replace_once(
    "src/main.cpp",
    "  edit_original_fahrenheit = display_fahrenheit;\n  for (uint8_t role = 0; role < 3; ++role) {\n",
    "  edit_original_fahrenheit = display_fahrenheit;\n  edit_original_cerbo_mqtt_interval_s =\n      cerbo_mqtt_settings.report_interval_s;\n  for (uint8_t role = 0; role < 3; ++role) {\n",
)

replace_once(
    "src/main.cpp",
    "  settings = edit_original_settings;\n  display_fahrenheit = edit_original_fahrenheit;\n",
    "  settings = edit_original_settings;\n  display_fahrenheit = edit_original_fahrenheit;\n  cerbo_mqtt_settings.report_interval_s =\n      edit_original_cerbo_mqtt_interval_s;\n",
)

replace_once(
    "src/main.cpp",
    "    } else if (selected_setting == kLayoutSetting) {\n      settings.fridge_on_left = !settings.fridge_on_left;\n      setting_changed = true;\n    }\n",
    "    } else if (selected_setting == kLayoutSetting) {\n      settings.fridge_on_left = !settings.fridge_on_left;\n      setting_changed = true;\n    } else if (selected_setting == kCerboMqttSetting) {\n      const size_t current =\n          cerbo_mqtt::ReportIntervalIndex(cerbo_mqtt_settings.report_interval_s);\n      int32_t next = (static_cast<int32_t>(current) + delta) %\n                     static_cast<int32_t>(cerbo_mqtt::kReportIntervalCount);\n      if (next < 0) next += cerbo_mqtt::kReportIntervalCount;\n      cerbo_mqtt_settings.report_interval_s =\n          cerbo_mqtt::kReportIntervalsS[next];\n      setting_changed = true;\n    }\n",
)

replace_once(
    "src/main.cpp",
    "      if (edit_changed) save_settings();\n      show_saved_message(now);\n",
    "      if (edit_changed) {\n        if (selected_setting == kCerboMqttSetting) {\n          cerbo_mqtt_settings_store->save();\n          apply_cerbo_mqtt_config();\n        } else {\n          save_settings();\n        }\n      }\n      show_saved_message(now);\n",
)

replace_once(
    "src/main.cpp",
    "  DisplayModel model{role_temps,\n                     calibration_c,\n                     &settings,\n                     &control_output,\n                     display_fahrenheit,\n                     alarm_visual_active,\n                     critical_probe_alarm,\n                     assignment_mode,\n                     menu_active,\n                     menu_editing,\n                     selected_setting,\n                     assignment_role,\n                     assignment_sensor,\n                     count,\n                     assignment_temp,\n                     assignment_rom,\n                     fault_count,\n                     static_cast<uint8_t>(fault.code),\n                     fault.message,\n                     signalk_connected};\n",
    "  CerboMqttPublisher& mqtt = cerbo_mqtt_publisher();\n  DisplayModel model{role_temps,\n                     calibration_c,\n                     &settings,\n                     &control_output,\n                     display_fahrenheit,\n                     alarm_visual_active,\n                     critical_probe_alarm,\n                     assignment_mode,\n                     menu_active,\n                     menu_editing,\n                     selected_setting,\n                     assignment_role,\n                     assignment_sensor,\n                     count,\n                     assignment_temp,\n                     assignment_rom,\n                     fault_count,\n                     static_cast<uint8_t>(fault.code),\n                     fault.message,\n                     signalk_connected,\n                     cerbo_mqtt_settings.report_interval_s,\n                     mqtt.connected()};\n",
)

replace_once(
    "src/main.cpp",
    "  ConfigItem(settings_store)\n      ->set_title(\"Fridge Controller\")\n      ->set_description(\n          \"Thermostat, alarms, fan timing, display layout, calibration, and sensor assignments. \"\n          \"Numeric limits shown below are enforced by the controller when settings are saved.\")\n      ->set_sort_order(500);\n\n  load_settings();\n",
    "  ConfigItem(settings_store)\n      ->set_title(\"Fridge Controller\")\n      ->set_description(\n          \"Thermostat, alarms, fan timing, display layout, calibration, and sensor assignments. \"\n          \"Numeric limits shown below are enforced by the controller when settings are saved.\")\n      ->set_sort_order(500);\n  ConfigItem(cerbo_mqtt_settings_store)\n      ->set_title(\"Cerbo GX MQTT\")\n      ->set_description(\n          \"Optional direct MQTT temperature publishing to a Cerbo GX / Node-RED installation. \"\n          \"Set the reporting interval to OFF to disable MQTT completely.\")\n      ->set_sort_order(510);\n\n  load_settings();\n  cerbo_mqtt_settings_store->load();\n  apply_cerbo_mqtt_config();\n",
)

replace_once(
    "src/main.cpp",
    "    read_temperatures();\n    update_controller();\n    update_display();\n    return;\n",
    "    read_temperatures();\n    update_controller();\n    service_cerbo_mqtt(now);\n    update_display();\n    return;\n",
)

replace_once(
    "src/main.cpp",
    "  read_temperatures();\n\n  if (now - last_control_ms >= hw::kControlPeriodMs) {\n",
    "  read_temperatures();\n  service_cerbo_mqtt(now);\n\n  if (now - last_control_ms >= hw::kControlPeriodMs) {\n",
)

# ----- Remove stale draft timing note now replaced by executable tests -----
p = Path("test/native/cerbo_mqtt_notes.txt")
if p.exists():
    p.unlink()
