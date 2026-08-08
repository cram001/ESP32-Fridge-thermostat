from pathlib import Path

p = Path('src/main.cpp')
s = p.read_text()

anchor = '''uint16_t selected_setting_gauge_value() {
  const float fraction = selected_setting_gauge_fraction();
  return static_cast<uint16_t>(roundf(
      hw::kEncoderGaugeMinValue +
      fraction * (hw::kEncoderGaugeMaxValue - hw::kEncoderGaugeMinValue)));
}
'''
replacement = anchor + '''
uint8_t selected_setting_gauge_gain() {
  float edit_steps = 1.0f;
  if (selected_setting == 0 || selected_setting == 1) {
    edit_steps = (hw::kFridgeControlMaxC - hw::kFridgeControlMinC) /
                 hw::kTemperatureEditStepC;
  } else if (selected_setting == 2) {
    edit_steps = (hw::kFreezerThresholdMaxC - hw::kFreezerThresholdMinC) /
                 hw::kTemperatureEditStepC;
  } else if (selected_setting == 3) {
    edit_steps = (hw::kFridgeAlarmMaxC - hw::kFridgeAlarmMinC) /
                 hw::kTemperatureEditStepC;
  } else if (selected_setting == 4) {
    edit_steps = (hw::kFreezerAlarmMaxC - hw::kFreezerAlarmMinC) /
                 hw::kTemperatureEditStepC;
  } else if (selected_setting >= 6 && selected_setting <= 8) {
    edit_steps = (2.0f * hw::kCalibrationLimitC) /
                 hw::kTemperatureEditStepC;
  } else if (selected_setting == 9) {
    edit_steps = static_cast<float>(hw::kFanDelayMaxS - hw::kFanDelayMinS) /
                 hw::kFanDelayStepS;
  } else if (selected_setting == 10 || selected_setting == 11) {
    edit_steps = hw::kFanMinimumOnMax - hw::kFanMinimumOnMin;
  } else if (selected_setting == 12) {
    edit_steps = hw::kEmergencySpilloverOptionCount - 1;
  } else if (selected_setting == 15) {
    edit_steps = hw::kOledContrastOptionCount - 1;
  } else if (selected_setting == 16) {
    edit_steps = hw::kDisplayTimeoutOptionCount - 1;
  }

  const float gauge_span =
      hw::kEncoderGaugeMaxValue - hw::kEncoderGaugeMinValue;
  const int gain = static_cast<int>(roundf(gauge_span / edit_steps));
  return static_cast<uint8_t>(constrain(
      gain, static_cast<int>(hw::kEncoderNavigationGain),
      static_cast<int>(hw::kEncoderGaugeGain)));
}
'''
if anchor not in s:
    raise SystemExit('gauge value anchor not found')
s = s.replace(anchor, replacement, 1)

old = '''  encoder_gain = hw::kEncoderGaugeGain;
  encoder_baseline_value = selected_setting_gauge_value();
'''
new = '''  encoder_gain = selected_setting_gauge_gain();
  encoder_baseline_value = selected_setting_gauge_value();
'''
if old not in s:
    raise SystemExit('gauge gain block not found')
s = s.replace(old, new, 1)

old = '''int32_t read_encoder_delta() {
  const int32_t position = encoder.getEncoderValue();
  const int32_t delta = encoder_delta_filter.decode(position);
  if (position != static_cast<int32_t>(encoder_baseline_value)) {
    encoder.setEncoderValue(encoder_baseline_value);
  }
  return delta;
}
'''
new = '''int32_t read_encoder_delta() {
  const int32_t position = encoder.getEncoderValue();
  return encoder_delta_filter.decode(position);
}
'''
if old not in s:
    raise SystemExit('read encoder block not found')
s = s.replace(old, new, 1)

old = '''  if (raw_button_down) {
    encoder.setEncoderValue(encoder_baseline_value);
    encoder_counterclockwise_substeps = 0;
  }
'''
new = '''  if (raw_button_down) {
    encoder_counterclockwise_substeps = 0;
  }
'''
if old not in s:
    raise SystemExit('button reset block not found')
s = s.replace(old, new, 1)

old = '''    if (setting_changed) {
      NormalizeControllerSettings(settings);
      edit_changed = true;
      if (setting_supports_encoder_gauge(selected_setting)) {
        set_encoder_gauge_mode();
      }
    }
'''
new = '''    if (setting_changed) {
      NormalizeControllerSettings(settings);
      edit_changed = true;
    }
'''
if old not in s:
    raise SystemExit('setting_changed gauge rewrite block not found')
s = s.replace(old, new, 1)

p.write_text(s)
