// Declares the SSD1309 OLED presentation layer and its read-only view model.
// Rendering is kept separate from control logic: this class formats current
// state but never changes thermostat settings or output decisions.
#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include "fridge_controller.h"

// Snapshot assembled by main.cpp for one frame. Pointer members refer to
// application-owned state and must remain valid for the duration of draw().
struct DisplayModel {
  const float* role_temp_c;
  const float* calibration_c;
  const ControllerSettings* settings;
  const ControllerOutput* control;
  bool fahrenheit;
  bool alarm_active;
  bool critical_probe_alarm;
  bool assignment_mode;
  bool menu_active;  // true while the user is actively browsing/editing
  uint8_t selected_setting;
  uint8_t assignment_role;
  uint8_t assignment_sensor;
  uint8_t detected_count;
  float assignment_temp_c;
  String assignment_rom;
  uint8_t fault_count;
  uint8_t fault_code;
  const char* fault_message;
  bool signalk_connected;
};

class FridgeDisplay {
 public:
  FridgeDisplay(uint8_t cs, uint8_t dc, uint8_t reset,
                uint32_t shift_period_ms);
  void begin();
  void set_contrast(uint8_t percent);
  void set_enabled(bool enabled);
  void draw_splash(const char* vessel_name, const char* version,
                   uint8_t detected_count,
                   uint8_t seconds_remaining);
  void draw(const DisplayModel& model);

 private:
  struct SettingText {
    const char* name;
    char value[20];
  };

  float shown_temperature(float celsius, bool fahrenheit) const;
  SettingText build_setting_text(const DisplayModel& model) const;

  // Screen states. Exactly one of these (plus the fault triangle overlay)
  // runs per frame -- see the dispatch in draw().
  void draw_home(int x, int y, const DisplayModel& model);
  void draw_alarm(const DisplayModel& model);
  void draw_menu(int x, int y, const DisplayModel& model);
  void draw_errors(int x, int y, const DisplayModel& model);
  void draw_assignment(int x, int y, const DisplayModel& model);

  void draw_temperature(int x, int y, const char* label, float value,
                        bool fahrenheit);
  void draw_hero_temperature(int x, int y, float value, bool fahrenheit);
  void draw_fan(int center_x, int center_y, uint8_t phase);
  void draw_warning_triangle(int x, int y);
  void draw_wifi_icon(int x, int y, bool connected);

  // Full-frame buffering avoids visible partial redraws on the SPI display.
  U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI oled_;
  uint32_t shift_period_ms_;
  uint32_t last_shift_ms_ = 0;
  uint8_t shift_index_ = 0;
};
