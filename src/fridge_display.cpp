#include "fridge_display.h"

namespace {
constexpr uint8_t kSettingCount = 22;
constexpr uint8_t kLayoutSetting = 17;
constexpr uint8_t kOutputTestSetting = 18;
constexpr uint8_t kFirstAssignmentSetting = 19;
constexpr uint8_t kLastAssignmentSetting = 21;
}

FridgeDisplay::FridgeDisplay(uint8_t cs, uint8_t dc, uint8_t reset,
                             uint32_t shift_period_ms)
    : oled_(U8G2_R0, cs, dc, reset),
      shift_period_ms_(shift_period_ms) {}

void FridgeDisplay::begin() { oled_.begin(); }

void FridgeDisplay::set_contrast(uint8_t percent) {
  constexpr uint8_t kMinimumPercent = hw::kOledContrastOptions[0];
  constexpr uint8_t kMaximumPercent =
      hw::kOledContrastOptions[hw::kOledContrastOptionCount - 1];
  percent = constrain(percent, kMinimumPercent, kMaximumPercent);
  oled_.setContrast(map(percent, kMinimumPercent, kMaximumPercent, 13, 255));
}

void FridgeDisplay::set_enabled(bool enabled) {
  oled_.setPowerSave(enabled ? 0 : 1);
}

void FridgeDisplay::draw_splash(const char* vessel_name, const char* version,
                                uint8_t detected_count,
                                uint8_t seconds_remaining) {
  oled_.clearBuffer();

  oled_.drawVLine(63, 14, 30);
  oled_.drawHLine(42, 43, 43);
  oled_.drawTriangle(65, 16, 65, 41, 84, 41);
  oled_.drawTriangle(61, 19, 61, 41, 44, 41);
  oled_.drawTriangle(64, 14, 72, 16, 64, 18);
  oled_.drawLine(36, 45, 91, 45);
  oled_.drawLine(36, 45, 44, 51);
  oled_.drawLine(44, 51, 82, 51);
  oled_.drawLine(82, 51, 91, 45);
  oled_.drawLine(31, 54, 43, 56);
  oled_.drawLine(43, 56, 55, 54);
  oled_.drawLine(55, 54, 67, 56);
  oled_.drawLine(67, 56, 79, 54);
  oled_.drawLine(79, 54, 96, 56);

  char title[25];
  snprintf(title, sizeof(title), "%s",
           vessel_name && vessel_name[0] ? vessel_name : "FRIDGE CTRL");
  oled_.setFont(u8g2_font_helvB10_tf);
  if (oled_.getStrWidth(title) > 88) oled_.setFont(u8g2_font_helvB08_tf);
  while (title[0] && oled_.getStrWidth(title) > 88) {
    title[strlen(title) - 1] = '\0';
  }
  oled_.drawStr(2, 12, title);

  oled_.setFont(u8g2_font_5x7_tf);
  const int version_width = oled_.getStrWidth(version);
  oled_.drawStr(127 - version_width, 7, version);

  char status[18];
  snprintf(status, sizeof(status), "PROBES %u", detected_count);
  oled_.drawStr(2, 63, status);
  oled_.drawStr(54, 63, "FANS ON");
  snprintf(status, sizeof(status), "%us", seconds_remaining);
  const int countdown_width = oled_.getStrWidth(status);
  oled_.drawStr(127 - countdown_width, 63, status);
  oled_.sendBuffer();
}

void FridgeDisplay::draw_saved() {
  oled_.clearBuffer();
  oled_.setFont(u8g2_font_helvB12_tf);
  const char* text = "SAVED";
  const int width = oled_.getStrWidth(text);
  oled_.drawStr((128 - width) / 2, 36, text);
  oled_.sendBuffer();
}

void FridgeDisplay::draw_output_test(uint8_t selection, bool active,
                                     uint8_t seconds_remaining) {
  static const char* names[] = {"SPILLOVER", "CIRCULATION", "BUZZER", "EXIT"};
  selection = constrain(selection, static_cast<uint8_t>(0),
                        static_cast<uint8_t>(3));
  oled_.clearBuffer();
  oled_.setFont(u8g2_font_6x10_tf);
  oled_.drawStr(2, 10, "OUTPUT TEST");

  if (active) {
    oled_.setFont(u8g2_font_helvB10_tf);
    const int name_width = oled_.getStrWidth(names[selection]);
    oled_.drawStr((128 - name_width) / 2, 29, names[selection]);
    oled_.setFont(u8g2_font_6x10_tf);
    char line[20];
    snprintf(line, sizeof(line), "RUNNING  %us", seconds_remaining);
    const int line_width = oled_.getStrWidth(line);
    oled_.drawStr((128 - line_width) / 2, 44, line);
    oled_.drawStr(24, 60, "Press to stop");
    oled_.sendBuffer();
    return;
  }

  for (uint8_t item = 0; item < 4; ++item) {
    char line[20];
    snprintf(line, sizeof(line), "%c %s", item == selection ? '>' : ' ',
             names[item]);
    oled_.drawStr(2, 22 + item * 12, line);
  }
  oled_.sendBuffer();
}

float FridgeDisplay::shown_temperature(float celsius, bool fahrenheit) const {
  return fahrenheit && std::isfinite(celsius) ? celsius * 1.8f + 32.0f
                                               : celsius;
}

void FridgeDisplay::draw_temperature(int x, int y, const char* label,
                                     float value, bool fahrenheit) {
  char text[22];
  const float shown = shown_temperature(value, fahrenheit);
  const char unit = fahrenheit ? 'F' : 'C';
  if (std::isfinite(shown)) {
    snprintf(text, sizeof(text), "%s %5.1f%c", label, shown, unit);
  } else {
    snprintf(text, sizeof(text), "%s  --.-%c", label, unit);
  }
  oled_.drawStr(x, y, text);
}

void FridgeDisplay::draw_hero_temperature(int x, int y, float value,
                                          bool fahrenheit,
                                          const uint8_t* font) {
  char text[8];
  const float shown = shown_temperature(value, fahrenheit);
  if (std::isfinite(shown)) {
    snprintf(text, sizeof(text), "%.1f", shown);
  } else {
    snprintf(text, sizeof(text), "--.-");
  }

  oled_.setFont(font);
  int digit_w = oled_.getStrWidth(text);
  oled_.drawStr(x, y, text);
  oled_.setFont(u8g2_font_helvB08_tf);
  oled_.drawStr(x + digit_w + 2, y, fahrenheit ? "F" : "C");
}

void FridgeDisplay::draw_wifi_icon(int x, int y, bool connected) {
  oled_.drawDisc(x, y, 1);
  oled_.drawCircle(x, y, 4, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  oled_.drawCircle(x, y, 7, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  if (!connected) oled_.drawLine(x - 7, y - 8, x + 7, y + 2);
}

void FridgeDisplay::draw_assignment(int x, int y,
                                    const DisplayModel& model) {
  const char* roles[] = {"Fridge", "Freezer", "Ambient"};
  char line[24];
  oled_.setFont(u8g2_font_6x10_tf);
  snprintf(line, sizeof(line), "ASSIGN %s", roles[model.assignment_role]);
  oled_.drawStr(x, y + 10, line);
  const bool no_sensor_selected =
      model.assignment_sensor >= model.detected_count;
  snprintf(line, sizeof(line), "Option %u of %u",
           model.assignment_sensor + 1, model.detected_count + 1);
  oled_.drawStr(x, y + 22, line);
  if (no_sensor_selected) {
    oled_.drawStr(x, y + 35, "No sensor assigned");
    oled_.drawStr(x, y + 47, "ROM NONE");
  } else {
    draw_temperature(x, y + 35, "Live", model.assignment_temp_c,
                     model.fahrenheit);
    const char* rom_suffix =
        model.assignment_rom && strlen(model.assignment_rom) > 8
            ? model.assignment_rom + 8
            : (model.assignment_rom ? model.assignment_rom : "");
    snprintf(line, sizeof(line), "ROM ...%s", rom_suffix);
    oled_.drawStr(x, y + 47, line);
  }
  oled_.drawStr(x, y + 61, "Rotate / press assign");
}

void FridgeDisplay::draw_fan(int center_x, int center_y, uint8_t phase) {
  const float angle_offset = phase * PI / 6.0f;
  for (uint8_t blade = 0; blade < 3; ++blade) {
    const float angle = angle_offset + blade * 2.0f * PI / 3.0f;
    const int end_x = center_x + roundf(cosf(angle) * 4.0f);
    const int end_y = center_y + roundf(sinf(angle) * 4.0f);
    oled_.drawLine(center_x, center_y, end_x, end_y);
  }
  oled_.drawDisc(center_x, center_y, 1);
}

void FridgeDisplay::draw_warning_triangle(int x, int y) {
  oled_.drawTriangle(x + 5, y, x, y + 9, x + 10, y + 9);
  oled_.drawVLine(x + 5, y + 3, 4);
  oled_.drawPixel(x + 5, y + 8);
}

void FridgeDisplay::draw_home(int x, int y, const DisplayModel& model) {
  // Fixed top-row zones:
  // [ Signal K ] [ LOCKOUT / GET-HOME ] [ warning ] [ ambient ]
  // Keeping these regions stable prevents mode banners and fault indication
  // from ever drawing on top of each other.
  draw_wifi_icon(x + 9, y + 9, model.signalk_connected);

  char ambient_text[8];
  const float ambient =
      shown_temperature(model.role_temp_c[2], model.fahrenheit);
  if (std::isfinite(ambient)) {
    snprintf(ambient_text, sizeof(ambient_text), "%.1f%c", ambient,
             model.fahrenheit ? 'F' : 'C');
  } else {
    snprintf(ambient_text, sizeof(ambient_text), "--.-%c",
             model.fahrenheit ? 'F' : 'C');
  }
  oled_.setFont(u8g2_font_5x7_tf);
  const int ambient_w = oled_.getStrWidth(ambient_text);
  const int ambient_x = x + 125 - ambient_w;
  oled_.drawStr(ambient_x, y + 9, ambient_text);

  constexpr int kWarningTriangleX = 84;
  constexpr int kBannerLeft = 22;
  constexpr int kBannerRight = 81;

  // Operational-mode banner. GET-HOME means the fridge probe is failed and a
  // non-zero emergency duty cycle is selected. LOCKOUT means the freezer is
  // still above its lockout threshold, so normal spillover is inhibited.
  const bool get_home_mode =
      model.control->sensor_fault &&
      model.settings->emergency_spillover_on_min != 0;
  const bool freezer_lockout = model.control->freezer_lockout;
  const char* banner = nullptr;
  if (get_home_mode && freezer_lockout) {
    // Both states matter. Alternate rather than hiding either one.
    banner = ((millis() / 2000UL) % 2U == 0U) ? "LOCKOUT" : "GET-HOME";
  } else if (freezer_lockout) {
    banner = "LOCKOUT";
  } else if (get_home_mode) {
    banner = "GET-HOME";
  }

  if (banner != nullptr) {
    oled_.setFont(u8g2_font_5x7_tf);
    const int text_w = oled_.getStrWidth(banner);
    const int frame_w = text_w + 6;
    const int available_left = x + kBannerLeft;
    const int available_right = x + kBannerRight;
    if (available_right - available_left >= frame_w) {
      const int frame_x =
          available_left + (available_right - available_left - frame_w) / 2;
      oled_.drawRFrame(frame_x, y + 1, frame_w, 10, 2);
      oled_.drawStr(frame_x + 3, y + 8, banner);
    }
  }

  // The warning triangle has a permanent home immediately left of ambient.
  // It no longer shifts or competes with LOCKOUT / GET-HOME banners.
  if (model.fault_count > 0 && millis() % 2000UL < 650UL) {
    draw_warning_triangle(x + kWarningTriangleX, y + 1);
  }

  const uint8_t left_role = model.settings->fridge_on_left ? 0 : 1;
  const uint8_t right_role = model.settings->fridge_on_left ? 1 : 0;
  const char* role_labels[] = {"FRDG", "FRZ"};

  oled_.setFont(u8g2_font_6x10_tf);
  oled_.drawStr(x + 2, y + 22, role_labels[left_role]);
  oled_.drawStr(x + 66, y + 22, role_labels[right_role]);

  const uint8_t* hero_font = u8g2_font_logisoso20_tf;
  constexpr int kHeroColumnWidth = 61;
  oled_.setFont(u8g2_font_helvB08_tf);
  const int unit_w = oled_.getStrWidth(model.fahrenheit ? "F" : "C");
  const uint8_t displayed_roles[] = {left_role, right_role};
  for (uint8_t column = 0; column < 2; ++column) {
    const uint8_t role = displayed_roles[column];
    char text[8];
    const float shown =
        shown_temperature(model.role_temp_c[role], model.fahrenheit);
    if (std::isfinite(shown)) {
      snprintf(text, sizeof(text), "%.1f", shown);
    } else {
      snprintf(text, sizeof(text), "--.-");
    }
    oled_.setFont(hero_font);
    if (oled_.getStrWidth(text) + 2 + unit_w > kHeroColumnWidth) {
      hero_font = u8g2_font_logisoso16_tf;
      break;
    }
  }
  draw_hero_temperature(x + 2, y + 46, model.role_temp_c[left_role],
                        model.fahrenheit, hero_font);
  draw_hero_temperature(x + 66, y + 46, model.role_temp_c[right_role],
                        model.fahrenheit, hero_font);

  oled_.setFont(u8g2_font_6x10_tf);
  const uint8_t fan_phase = (millis() / 200) % 6;
  oled_.drawStr(x + 2, y + 62, "SPILL");
  if (model.control->spillover) {
    draw_fan(x + 37, y + 57, fan_phase);
  } else {
    oled_.drawStr(x + 31, y + 62, "-");
  }
  oled_.drawStr(x + 68, y + 62, "CIRC");
  if (model.control->circulation) {
    draw_fan(x + 99, y + 57, fan_phase);
  } else {
    oled_.drawStr(x + 93, y + 62, "-");
  }
}

void FridgeDisplay::draw_alarm(const DisplayModel& model) {
  oled_.setDrawColor(1);
  oled_.drawBox(0, 0, 128, 64);
  oled_.setDrawColor(0);
  oled_.setFont(u8g2_font_helvB10_tf);
  oled_.drawStr(30, 18, "ALARM");

  if (model.critical_probe_alarm) {
    oled_.setFont(u8g2_font_helvB08_tf);
    oled_.drawStr(19, 36, "SENSOR FAULT");
    const char* message = !std::isfinite(model.role_temp_c[0])
                              ? "CHECK FRIDGE PROBE"
                              : "CHECK FREEZER PROBE";
    const int message_width = oled_.getStrWidth(message);
    oled_.drawStr((128 - message_width) / 2, 52, message);
    oled_.setDrawColor(1);
    return;
  }

  const bool fridge_tripped =
      std::isfinite(model.role_temp_c[0]) &&
      model.role_temp_c[0] >= model.settings->fridge_alarm_c;
  const uint8_t role = fridge_tripped ? 0 : 1;
  const char* names[] = {"FRIDGE", "FREEZER"};
  char line[20];
  snprintf(line, sizeof(line), "%s %.1f%c", names[role],
           shown_temperature(model.role_temp_c[role], model.fahrenheit),
           model.fahrenheit ? 'F' : 'C');
  oled_.setFont(u8g2_font_logisoso16_tf);
  const int w = oled_.getStrWidth(line);
  oled_.drawStr((128 - w) / 2, 48, line);
  oled_.setDrawColor(1);
}

FridgeDisplay::SettingText FridgeDisplay::build_setting_text(
    const DisplayModel& model) const {
  SettingText t{"", ""};
  const char* names[] = {"Fridge max T", "Fridge min T", "Freez T lockout",
                         "Fridge alarm", "Freezer alarm"};
  if (model.selected_setting <= 4) {
    const float values[] = {model.settings->high_c, model.settings->low_c,
                            model.settings->freezer_lockout_c,
                            model.settings->fridge_alarm_c,
                            model.settings->freezer_alarm_c};
    t.name = names[model.selected_setting];
    snprintf(t.value, sizeof(t.value), "%.1f%c",
             shown_temperature(values[model.selected_setting],
                               model.fahrenheit),
             model.fahrenheit ? 'F' : 'C');
  } else if (model.selected_setting == 5) {
    t.name = "Units";
    snprintf(t.value, sizeof(t.value), "%c", model.fahrenheit ? 'F' : 'C');
  } else if (model.selected_setting <= 8) {
    const uint8_t role = model.selected_setting - 6;
    const char* roles[] = {"Cal Fridge", "Cal Freezer", "Cal Ambient"};
    const float shown = model.fahrenheit ? model.calibration_c[role] * 1.8f
                                         : model.calibration_c[role];
    t.name = roles[role];
    snprintf(t.value, sizeof(t.value), "%+.1f%c", shown,
             model.fahrenheit ? 'F' : 'C');
  } else if (model.selected_setting == 9) {
    t.name = "Fan delay";
    snprintf(t.value, sizeof(t.value), "%us", model.settings->fan_delay_s);
  } else if (model.selected_setting == 10) {
    t.name = "Spill min ON";
    snprintf(t.value, sizeof(t.value), "%um",
             model.settings->spillover_min_on_min);
  } else if (model.selected_setting == 11) {
    t.name = "Circ min ON";
    snprintf(t.value, sizeof(t.value), "%um",
             model.settings->circulation_min_on_min);
  } else if (model.selected_setting == 12) {
    t.name = "Get-me-home fan";
    if (model.settings->emergency_spillover_on_min == 0) {
      snprintf(t.value, sizeof(t.value), "OFF");
    } else {
      snprintf(t.value, sizeof(t.value), "%um/hour",
               model.settings->emergency_spillover_on_min);
    }
  } else if (model.selected_setting == 13) {
    static const char* buzzer_names[] = {"OFF", "STEADY", "DOUBLE", "HI-LO",
                                         "TRIPLE"};
    const uint8_t mode = model.settings->buzzer_mode < hw::kBuzzerModeCount
                             ? model.settings->buzzer_mode
                             : hw::kDefaultBuzzerMode;
    t.name = "Buzzer";
    snprintf(t.value, sizeof(t.value), "%s", buzzer_names[mode]);
  } else if (model.selected_setting == 15) {
    t.name = "OLED contrast";
    snprintf(t.value, sizeof(t.value), "%u%%",
             model.settings->oled_contrast_percent);
  } else if (model.selected_setting == 16) {
    t.name = "Display off";
    if (model.settings->display_timeout_min == 0) {
      snprintf(t.value, sizeof(t.value), "Never");
    } else {
      snprintf(t.value, sizeof(t.value), "%um",
               model.settings->display_timeout_min);
    }
  } else if (model.selected_setting == kLayoutSetting) {
    t.name = "Display layout";
    snprintf(t.value, sizeof(t.value), "%s",
             model.settings->fridge_on_left ? "FRDG | FRZ" : "FRZ | FRDG");
  } else if (model.selected_setting == kOutputTestSetting) {
    t.name = "Test outputs";
    snprintf(t.value, sizeof(t.value), "Press to open");
  } else if (model.selected_setting >= kFirstAssignmentSetting &&
             model.selected_setting <= kLastAssignmentSetting) {
    const char* roles[] = {"Assign fridge", "Assign freezer",
                           "Assign ambient"};
    t.name = roles[model.selected_setting - kFirstAssignmentSetting];
    t.value[0] = '\0';
  }
  return t;
}

void FridgeDisplay::draw_menu(int x, int y, const DisplayModel& model) {
  const SettingText t = build_setting_text(model);
  oled_.setFont(u8g2_font_6x10_tf);
  oled_.drawStr(x, y + 10, t.name);
  const char* mode = model.menu_editing ? "EDIT" : "< >";
  const int mode_w = oled_.getStrWidth(mode);
  oled_.drawStr(126 - mode_w, y + 10, mode);
  oled_.setFont(u8g2_font_logisoso16_tf);
  oled_.drawStr(x, y + 34, t.value);
  oled_.setFont(u8g2_font_6x10_tf);
  char frac[8];
  snprintf(frac, sizeof(frac), "%u/%u", model.selected_setting + 1,
           kSettingCount);
  const int w = oled_.getStrWidth(frac);
  oled_.drawStr(126 - w, y + 46, frac);
  oled_.drawFrame(x, y + 50, 124, 4);
  const int fill = (124 * (model.selected_setting + 1)) / kSettingCount;
  oled_.drawBox(x, y + 50, fill, 4);
}

void FridgeDisplay::draw_errors(int x, int y, const DisplayModel& model) {
  oled_.setFont(u8g2_font_6x10_tf);
  char line[24];
  oled_.drawStr(x, y + 10, "ACTIVE ERRORS");
  snprintf(line, sizeof(line), "Count: %u", model.fault_count);
  oled_.drawStr(x, y + 22, line);
  if (model.fault_count == 0) {
    oled_.drawStr(x, y + 38, "No active errors");
    return;
  }
  snprintf(line, sizeof(line), "Code E%02u", model.fault_code);
  oled_.drawStr(x, y + 34, line);
  oled_.setFont(u8g2_font_5x7_tf);
  oled_.drawStr(x, y + 47, model.fault_message);
  oled_.setFont(u8g2_font_6x10_tf);
  oled_.drawStr(x, y + 60,
                model.menu_editing ? "Rotate / press back"
                                   : "Press to browse");
}

void FridgeDisplay::draw(const DisplayModel& model) {
  static const int8_t shifts[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  if (millis() - last_shift_ms_ >= shift_period_ms_) {
    last_shift_ms_ = millis();
    shift_index_ = (shift_index_ + 1) % 4;
  }
  const int x = shifts[shift_index_][0];
  const int y = shifts[shift_index_][1];
  oled_.clearBuffer();

  const bool showing_errors =
      model.selected_setting == 14 &&
      (model.menu_active || model.fault_count > 0);

  if (model.assignment_mode) {
    draw_assignment(x, y, model);
  } else if (showing_errors) {
    draw_errors(x, y, model);
  } else if (model.alarm_active) {
    draw_alarm(model);
  } else if (model.menu_active) {
    draw_menu(x, y, model);
  } else {
    draw_home(x, y, model);
  }

  oled_.sendBuffer();
}
