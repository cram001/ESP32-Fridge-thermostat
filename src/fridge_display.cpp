#include "fridge_display.h"

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

void FridgeDisplay::draw_assignment(int x, int y,
                                    const DisplayModel& model) {
  const char* roles[] = {"Fridge", "Freezer", "Ambient"};
  char line[24];
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

void FridgeDisplay::draw_signalk_badge(int x, int y, bool connected) {
  // A labeled badge is less ambiguous than a generic Wi-Fi icon: Wi-Fi may be
  // up while the Signal K WebSocket itself is disconnected.
  oled_.drawFrame(x, y, 17, 10);
  oled_.drawStr(x + 2, y + 8, "SK");
  if (!connected) oled_.drawLine(x, y + 9, x + 16, y);
}

void FridgeDisplay::draw(const DisplayModel& model) {
  // A small repeating offset distributes OLED wear without making the movement
  // visually distracting.
  static const int8_t shifts[][2] = {{0, 0}, {2, 0}, {2, 2},
                                     {0, 2}, {1, 1}};
  if (millis() - last_shift_ms_ >= shift_period_ms_) {
    last_shift_ms_ = millis();
    shift_index_ = (shift_index_ + 1) % 5;
  }
  const int x = shifts[shift_index_][0];
  const int y = shifts[shift_index_][1];
  oled_.clearBuffer();
  oled_.setFont(u8g2_font_6x10_tf);
  if (model.assignment_mode) {
    draw_assignment(x, y, model);
    oled_.sendBuffer();
    return;
  }
  if (model.selected_setting == 14) {
    char line[24];
    oled_.drawStr(x, y + 10, "ACTIVE ERRORS");
    snprintf(line, sizeof(line), "Count: %u", model.fault_count);
    oled_.drawStr(x, y + 24, line);
    if (model.fault_count == 0) {
      oled_.drawStr(x, y + 42, "No active errors");
    } else {
      snprintf(line, sizeof(line), "Code E%02u", model.fault_code);
      oled_.drawStr(x, y + 38, line);
      oled_.drawStr(x, y + 52, model.fault_message);
      oled_.drawStr(x, y + 63, "Rotate to browse");
    }
    oled_.sendBuffer();
    return;
  }

  draw_temperature(x, y + 10, "Fridge ", model.role_temp_c[0],
                   model.fahrenheit);
  draw_temperature(x, y + 21, "Freezer", model.role_temp_c[1],
                   model.fahrenheit);
  draw_temperature(x, y + 32, "Ambient", model.role_temp_c[2],
                   model.fahrenheit);
  draw_signalk_badge(x + 96, y + 1, model.signalk_connected);
  char line[24];
  // Animate only enabled outputs, leaving an unambiguous dash when OFF.
  const uint8_t fan_phase = (millis() / 200) % 6;
  oled_.drawStr(x, y + 44, "SPILL");
  if (model.control->spillover) draw_fan(x + 39, y + 40, fan_phase);
  else oled_.drawStr(x + 36, y + 44, "-");
  oled_.drawStr(x + 51, y + 44, "CIRC");
  if (model.control->circulation) draw_fan(x + 84, y + 40, fan_phase);
  else oled_.drawStr(x + 81, y + 44, "-");

  const char* names[] = {"High", "Low", "Lock", "FrAlm", "FzAlm"};
  if (model.selected_setting <= 4) {
    const float values[] = {model.settings->high_c, model.settings->low_c,
                            model.settings->freezer_lockout_c,
                            model.settings->fridge_alarm_c,
                            model.settings->freezer_alarm_c};
    snprintf(line, sizeof(line), ">%s %.1f%c %s",
             names[model.selected_setting],
             shown_temperature(values[model.selected_setting],
                               model.fahrenheit),
             model.fahrenheit ? 'F' : 'C',
             model.alarm_active ? "ALARM" : "");
  } else if (model.selected_setting == 5) {
    snprintf(line, sizeof(line), ">Units: %c", model.fahrenheit ? 'F' : 'C');
  } else if (model.selected_setting <= 8) {
    const uint8_t role = model.selected_setting - 6;
    const char* roles[] = {"Fridge", "Freezer", "Ambient"};
    const float shown = model.fahrenheit ? model.calibration_c[role] * 1.8f
                                         : model.calibration_c[role];
    snprintf(line, sizeof(line), ">Cal %s %+.1f%c", roles[role], shown,
             model.fahrenheit ? 'F' : 'C');
  } else if (model.selected_setting == 9) {
    snprintf(line, sizeof(line), ">Fan delay %us",
             model.settings->fan_delay_s);
  } else if (model.selected_setting == 10) {
    snprintf(line, sizeof(line), ">Spill min ON %um",
             model.settings->spillover_min_on_min);
  } else if (model.selected_setting == 11) {
    snprintf(line, sizeof(line), ">Circ min ON %um",
             model.settings->circulation_min_on_min);
  } else if (model.selected_setting == 12) {
    snprintf(line, sizeof(line), ">Fail 5 on/%um off",
             model.settings->failsafe_off_min);
  } else if (model.selected_setting == 13) {
    snprintf(line, sizeof(line), ">Buzzer %s",
             model.settings->buzzer_enabled ? "ON" : "OFF");
  } else if (model.selected_setting == 14) {
    if (model.fault_count == 0) snprintf(line, sizeof(line), ">Errors: none");
    else snprintf(line, sizeof(line), "E%02u %s", model.fault_code,
                  model.fault_message);
  } else if (model.selected_setting == 15) {
    snprintf(line, sizeof(line), ">OLED contrast %u%%",
             model.settings->oled_contrast_percent);
  } else if (model.selected_setting == 16) {
    if (model.settings->display_timeout_min == 0) {
      snprintf(line, sizeof(line), ">Off timer disabled");
    } else {
      snprintf(line, sizeof(line), ">Display off %um",
               model.settings->display_timeout_min);
    }
  } else {
    snprintf(line, sizeof(line), ">Assign sensors");
  }
  oled_.drawStr(x, y + 55, line);
  if (model.fault_count > 0) {
    // A short, slow flash attracts attention without making the cabin display
    // visually irritating at night. Details remain in the Errors menu.
    if (millis() % 2000UL < 650UL) draw_warning_triangle(x + 116, y + 1);
  }
  oled_.sendBuffer();
}
