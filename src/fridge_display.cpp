#include "fridge_display.h"

namespace {
constexpr uint8_t kSettingCount = 18;
}

FridgeDisplay::FridgeDisplay(uint8_t cs, uint8_t dc, uint8_t reset,
                             uint32_t shift_period_ms)
    : oled_(U8G2_R0, cs, dc, reset),
      shift_period_ms_(shift_period_ms) {}

void FridgeDisplay::begin() { oled_.begin(); }

void FridgeDisplay::set_contrast(uint8_t percent) {
  percent = constrain(percent, 10, 100);
  oled_.setContrast(map(percent, 10, 100, 25, 255));
}

void FridgeDisplay::set_enabled(bool enabled) {
  oled_.setPowerSave(enabled ? 0 : 1);
}

void FridgeDisplay::draw_splash(const char* vessel_name, const char* version,
                                uint8_t detected_count,
                                uint8_t seconds_remaining) {
  oled_.clearBuffer();

  // Corissa under sail: mast, boom, mainsail, jib, hull, pennant, and sea.
  oled_.drawVLine(63, 14, 30);
  oled_.drawHLine(42, 43, 43);
  oled_.drawTriangle(65, 16, 65, 41, 84, 41);  // mainsail
  oled_.drawTriangle(61, 19, 61, 41, 44, 41);  // jib
  oled_.drawTriangle(64, 14, 72, 16, 64, 18);  // pennant
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
  oled_.drawStr(54, 63, "FANS OFF");
  snprintf(status, sizeof(status), "%us", seconds_remaining);
  const int countdown_width = oled_.getStrWidth(status);
  oled_.drawStr(127 - countdown_width, 63, status);

  oled_.sendBuffer();
}

float FridgeDisplay::shown_temperature(float celsius, bool fahrenheit) const {
  // Conversion is presentation-only; control and persisted thresholds stay C.
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
                                          bool fahrenheit) {
  // Keeps the decimal place. A single u8g2 font size can't be trusted to fit
  // every possible string ("-1.0" vs "-40.0") inside a 62px-wide column, so
  // this measures at logisoso20 first and drops to logisoso16 only if the
  // rendered string (digits + unit letter) would actually overflow.
  char text[8];
  const float shown = shown_temperature(value, fahrenheit);
  if (std::isfinite(shown)) {
    snprintf(text, sizeof(text), "%.1f", shown);
  } else {
    snprintf(text, sizeof(text), "--.-");
  }

  const uint8_t* hero_fonts[] = {u8g2_font_logisoso20_tf,
                                 u8g2_font_logisoso16_tf};
  const int column_width = 58;  // ~62px column minus a couple px margin
  oled_.setFont(u8g2_font_helvB08_tf);
  const int unit_w = oled_.getStrWidth(fahrenheit ? "F" : "C");

  uint8_t font_index = 0;
  oled_.setFont(hero_fonts[font_index]);
  int digit_w = oled_.getStrWidth(text);
  if (digit_w + 2 + unit_w > column_width) {
    font_index = 1;
    oled_.setFont(hero_fonts[font_index]);
    digit_w = oled_.getStrWidth(text);
  }
  oled_.setFont(hero_fonts[font_index]);
  oled_.drawStr(x, y, text);
  oled_.setFont(u8g2_font_helvB08_tf);
  oled_.drawStr(x + digit_w + 2, y, fahrenheit ? "F" : "C");
}

void FridgeDisplay::draw_wifi_icon(int x, int y, bool connected) {
  // Three nested arcs + a dot, the standard wifi glyph, built from u8g2's
  // quarter-circle draw flags rather than a bitmap so it stays crisp at any
  // contrast setting. x,y is the anchor point (the dot).
  oled_.drawDisc(x, y, 1);
  oled_.drawCircle(x, y, 4, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  oled_.drawCircle(x, y, 7, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  if (!connected) {
    oled_.drawLine(x - 7, y - 8, x + 7, y + 2);
  }
}

void FridgeDisplay::draw_assignment(int x, int y,
                                    const DisplayModel& model) {
  const char* roles[] = {"Fridge", "Freezer", "Ambient"};
  char line[24];
  oled_.setFont(u8g2_font_6x10_tf);
  snprintf(line, sizeof(line), "ASSIGN %s", roles[model.assignment_role]);
  oled_.drawStr(x, y + 10, line);
  if (model.detected_count == 0) {
    oled_.drawStr(x, y + 28, "No sensors detected");
    oled_.drawStr(x, y + 44, "Check bus and reset");
    return;
  }
  snprintf(line, sizeof(line), "Probe %u of %u", model.assignment_sensor + 1,
           model.detected_count);
  oled_.drawStr(x, y + 22, line);
  draw_temperature(x, y + 35, "Live", model.assignment_temp_c,
                   model.fahrenheit);
  snprintf(line, sizeof(line), "ROM ...%s",
           model.assignment_rom.substring(8).c_str());
  oled_.drawStr(x, y + 47, line);
  oled_.drawStr(x, y + 59, "Rotate / press assign");
}

void FridgeDisplay::draw_fan(int center_x, int center_y, uint8_t phase) {
  // Three short blades rotate through six frames. This represents a commanded
  // ON state only; the installation has no tachometer to prove fan motion.
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
  // Idle default, four bands top to bottom:
  //   1. wifi icon + ambient reading                     (y  0-12)
  //   2. small FRZ/FRDG column labels                    (y 12-20)
  //   3. hero digits w/ one decimal, Freezer left /       (y 20-42, ~20px --
  //      Fridge right, auto-shrinks if a string overflows  see below)
  //   4. fan status strip                                 (y 42-62)
  // Coordinates are estimates against nominal U8g2 glyph metrics; expect to
  // nudge a few pixels once it's on real hardware.
  draw_wifi_icon(x + 9, y + 9, model.signalk_connected);

  char ambient_text[8];
  const float ambient = shown_temperature(model.role_temp_c[2], model.fahrenheit);
  if (std::isfinite(ambient)) {
    snprintf(ambient_text, sizeof(ambient_text), "%.1f%c", ambient,
             model.fahrenheit ? 'F' : 'C');
  } else {
    snprintf(ambient_text, sizeof(ambient_text), "--.-%c",
             model.fahrenheit ? 'F' : 'C');
  }
  oled_.setFont(u8g2_font_helvB10_tf);
  const int ambient_w = oled_.getStrWidth(ambient_text);
  oled_.drawStr(x + 126 - ambient_w, y + 11, ambient_text);

  oled_.setFont(u8g2_font_6x10_tf);
  oled_.drawStr(x + 2, y + 19, "FRZ");
  oled_.drawStr(x + 66, y + 19, "FRDG");

  draw_hero_temperature(x + 2, y + 42, model.role_temp_c[1], model.fahrenheit);
  draw_hero_temperature(x + 66, y + 42, model.role_temp_c[0], model.fahrenheit);

  oled_.setFont(u8g2_font_6x10_tf);
  const uint8_t fan_phase = (millis() / 200) % 6;
  oled_.drawStr(x + 2, y + 61, "SPILL");
  if (model.control->spillover) draw_fan(x + 34, y + 56, fan_phase);
  else oled_.drawStr(x + 31, y + 61, "-");
  oled_.drawStr(x + 68, y + 61, "CIRC");
  if (model.control->circulation) draw_fan(x + 96, y + 56, fan_phase);
  else oled_.drawStr(x + 93, y + 61, "-");
}

void FridgeDisplay::draw_alarm(const DisplayModel& model) {
  // Full-screen inversion is far harder to miss from across a cabin than a
  // word appended to a small settings line plus a 10px corner triangle.
  oled_.setDrawColor(1);
  oled_.drawBox(0, 0, 128, 62);
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

  oled_.setDrawColor(1);  // restore default before any overlay draws
}

FridgeDisplay::SettingText FridgeDisplay::build_setting_text(
    const DisplayModel& model) const {
  SettingText t{"", ""};
  const char* names[] = {"High", "Low", "Lockout", "Fridge alarm",
                         "Freezer alarm"};
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
    t.name = "Failsafe off";
    snprintf(t.value, sizeof(t.value), "%um", model.settings->failsafe_off_min);
  } else if (model.selected_setting == 13) {
    t.name = "Buzzer";
    snprintf(t.value, sizeof(t.value), "%s",
             model.settings->buzzer_enabled ? "ON" : "OFF");
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
  } else {
    t.name = "Assign sensors";
    t.value[0] = '\0';
  }
  return t;
}

void FridgeDisplay::draw_menu(int x, int y, const DisplayModel& model) {
  // Dedicated screen while the encoder is in active use: the item being
  // edited gets a big font instead of sharing space with the temperatures,
  // and a fraction + progress bar replace guessing your position by memory.
  const SettingText t = build_setting_text(model);
  oled_.setFont(u8g2_font_6x10_tf);
  oled_.drawStr(x, y + 10, t.name);

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
  // Same content as before, but re-spaced to fit inside the 62px canvas --
  // the previous "Rotate to browse" line at y+63 was drawn off-screen.
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
  oled_.drawStr(x, y + 46, model.fault_message);
  oled_.drawStr(x, y + 58, "Rotate to browse");
}

void FridgeDisplay::draw(const DisplayModel& model) {
  // A small repeating offset distributes OLED wear without making the
  // movement visually distracting.
  static const int8_t shifts[][2] = {{0, 0}, {2, 0}, {2, 2},
                                     {0, 2}, {1, 1}};
  if (millis() - last_shift_ms_ >= shift_period_ms_) {
    last_shift_ms_ = millis();
    shift_index_ = (shift_index_ + 1) % 5;
  }
  const int x = shifts[shift_index_][0];
  const int y = shifts[shift_index_][1];
  oled_.clearBuffer();

  const bool showing_errors =
      model.selected_setting == 14 && (model.menu_active || model.fault_count > 0);

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

  // The fault triangle is redundant on screens that already communicate the
  // fault (alarm banner, error browser) so it only overlays home/menu.
  if (!model.alarm_active && !showing_errors && !model.assignment_mode &&
      model.fault_count > 0) {
    if (millis() % 2000UL < 650UL) draw_warning_triangle(x + 116, y + 1);
  }
  oled_.sendBuffer();
}
