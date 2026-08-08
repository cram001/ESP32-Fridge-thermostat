from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f'{path}: expected exactly one match for {old!r}, found {text.count(old)}')
    p.write_text(text.replace(old, new, 1))

# Menu constants and read-only About behavior.
replace_once('src/main.cpp',
'''constexpr uint8_t kSettingCount = 22;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kOutputTestSetting = 18;\nconstexpr uint8_t kFirstAssignmentSetting = 19;\nconstexpr uint8_t kLastAssignmentSetting = 21;''',
'''constexpr uint8_t kSettingCount = 23;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kOutputTestSetting = 18;\nconstexpr uint8_t kFirstAssignmentSetting = 19;\nconstexpr uint8_t kLastAssignmentSetting = 21;\nconstexpr uint8_t kAboutSetting = 22;''')

replace_once('src/main.cpp',
'''      } else {\n        if (selected_setting != 14) capture_edit_snapshot();\n        menu_editing = true;''',
'''      } else {\n        if (selected_setting != 14 && selected_setting != kAboutSetting) {\n          capture_edit_snapshot();\n        }\n        menu_editing = true;''')

replace_once('src/main.cpp',
'''  if (button_down) {\n    if (selected_setting != 14) {\n      // Avoid unnecessary filesystem writes if edit was entered/exited without\n      // changing anything, while keeping the user-facing commit confirmation.\n      if (edit_changed) save_settings();\n      show_saved_message(now);\n      discard_edit_snapshot();\n    }\n    menu_editing = false;''',
'''  if (button_down) {\n    if (selected_setting != 14 && selected_setting != kAboutSetting) {\n      // Avoid unnecessary filesystem writes if edit was entered/exited without\n      // changing anything, while keeping the user-facing commit confirmation.\n      if (edit_changed) save_settings();\n      show_saved_message(now);\n      discard_edit_snapshot();\n    }\n    menu_editing = false;''')

# Display constants, entry, screen and fan-off alignment.
replace_once('src/fridge_display.cpp',
'''constexpr uint8_t kSettingCount = 22;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kOutputTestSetting = 18;\nconstexpr uint8_t kFirstAssignmentSetting = 19;\nconstexpr uint8_t kLastAssignmentSetting = 21;''',
'''constexpr uint8_t kSettingCount = 23;\nconstexpr uint8_t kLayoutSetting = 17;\nconstexpr uint8_t kOutputTestSetting = 18;\nconstexpr uint8_t kFirstAssignmentSetting = 19;\nconstexpr uint8_t kLastAssignmentSetting = 21;\nconstexpr uint8_t kAboutSetting = 22;''')

replace_once('src/fridge_display.cpp',
'''  } else if (model.selected_setting >= kFirstAssignmentSetting &&\n             model.selected_setting <= kLastAssignmentSetting) {\n    const char* roles[] = {"Assign fridge", "Assign freezer",\n                           "Assign ambient"};\n    t.name = roles[model.selected_setting - kFirstAssignmentSetting];\n    t.value[0] = '\\0';\n  }\n  return t;''',
'''  } else if (model.selected_setting >= kFirstAssignmentSetting &&\n             model.selected_setting <= kLastAssignmentSetting) {\n    const char* roles[] = {"Assign fridge", "Assign freezer",\n                           "Assign ambient"};\n    t.name = roles[model.selected_setting - kFirstAssignmentSetting];\n    t.value[0] = '\\0';\n  } else if (model.selected_setting == kAboutSetting) {\n    t.name = "About";\n    snprintf(t.value, sizeof(t.value), "Press to open");\n  }\n  return t;''')

replace_once('src/fridge_display.cpp',
'''void FridgeDisplay::draw_errors(int x, int y, const DisplayModel& model) {''',
'''void FridgeDisplay::draw_about(int x, int y) {\n  oled_.setFont(u8g2_font_6x10_tf);\n  oled_.drawStr(x + 2, y + 10, "ABOUT");\n\n  char line[24];\n  snprintf(line, sizeof(line), "Firmware %s", hw::kFirmwareVersion);\n  oled_.drawStr(x + 2, y + 25, line);\n  snprintf(line, sizeof(line), "Built %s", __DATE__);\n  oled_.drawStr(x + 2, y + 38, line);\n\n  oled_.setFont(u8g2_font_5x7_tf);\n  oled_.drawStr(x + 2, y + 51, "(c) Marc Archambault");\n  oled_.drawStr(x + 2, y + 62, "Press to return");\n}\n\nvoid FridgeDisplay::draw_errors(int x, int y, const DisplayModel& model) {''')

replace_once('src/fridge_display.cpp',
'''  } else if (showing_errors) {\n    draw_errors(x, y, model);\n  } else if (model.alarm_active) {''',
'''  } else if (showing_errors) {\n    draw_errors(x, y, model);\n  } else if (model.menu_active && model.menu_editing &&\n             model.selected_setting == kAboutSetting) {\n    draw_about(x, y);\n  } else if (model.alarm_active) {''')

replace_once('src/fridge_display.cpp',
'''    oled_.drawStr(x + 31, y + 62, "-");''',
'''    oled_.drawStr(x + 34, y + 62, "-");''')
replace_once('src/fridge_display.cpp',
'''    oled_.drawStr(x + 93, y + 62, "-");''',
'''    oled_.drawStr(x + 96, y + 62, "-");''')

replace_once('include/fridge_display.h',
'''  void draw_errors(int x, int y, const DisplayModel& model);\n  void draw_assignment(int x, int y, const DisplayModel& model);''',
'''  void draw_errors(int x, int y, const DisplayModel& model);\n  void draw_about(int x, int y);\n  void draw_assignment(int x, int y, const DisplayModel& model);''')

replace_once('include/hardware_config.h',
'''constexpr char kFirmwareVersion[] = "v0.12.9";''',
'''constexpr char kFirmwareVersion[] = "v0.13.0";''')

# User guide: menu count and About item, plus current sensor scan cadence if stale.
replace_once('docs/USER_GUIDE.md',
'''- Rotate clockwise or counterclockwise to browse all 22 items. Browsing wraps''',
'''- Rotate clockwise or counterclockwise to browse all 23 items. Browsing wraps''')
replace_once('docs/USER_GUIDE.md',
'''| 22 | `Assign ambient` | Assign/clear ambient probe. |''',
'''| 22 | `Assign ambient` | Assign/clear ambient probe. |\n| 23 | `About` | Firmware version, build date, and copyright/author information. |''')

print('About menu and fan-off alignment patch applied')
