// Central hardware map and timing policy for the fridge controller.
// Keep board pins and system-wide intervals here so electrical or behavioral
// changes do not require searching through the application logic.
#pragma once

#include <Arduino.h>

namespace hw {

// DFRobot FireBeetle 2 ESP32-E N16R2 (DFR1139): sensors and outputs.
constexpr uint8_t kOneWirePin = 4;
constexpr uint8_t kSpilloverFanPin = 13;
constexpr uint8_t kCirculationFanPin = 17;
constexpr uint8_t kBuzzerPin = 19;
constexpr bool kFanActiveHigh = true;

constexpr uint8_t kI2cSdaPin = 21;
constexpr uint8_t kI2cSclPin = 22;
constexpr uint8_t kEncoderAddress = 0x54;  // SEN0502 DIP switches both OFF
constexpr uint8_t kEncoderGainCoefficient = 1;  // minimum supported gain for rotation tracking
constexpr uint8_t kEncoderCountsPerStep = 1;   // normalize 1-2 raw encoder counts to one UI step
constexpr uint8_t kEncoderGain = kEncoderGainCoefficient;
constexpr uint16_t kEncoderInitialValue = 0;
constexpr float kTemperatureEditStepC = 0.1f;
constexpr float kFreezerThresholdMinC = -10.0f;
constexpr float kFreezerThresholdMaxC = 10.0f;
constexpr float kFridgeAlarmMinC = 3.0f;
constexpr float kFridgeAlarmMaxC = 45.0f;
constexpr float kFreezerAlarmMinC = -15.0f;
constexpr float kFreezerAlarmMaxC = 45.0f;
constexpr float kCalibrationLimitC = 5.0f;

// Safety and fault qualification intervals.
constexpr uint32_t kSignalKFaultGraceMs = 60UL * 1000UL;
constexpr uint32_t kStartupAlarmGraceMs = 2UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kLongFanRunMs = 60UL * 60UL * 1000UL;

// SSD1309 128x64 OLED, 4-wire SPI. The display manual labels SPI
// clock/data as D0/SCK and D1/SDA; these map to the ESP32 VSPI SCK/MOSI
// pins used by U8g2 hardware SPI.
constexpr uint8_t kOledClockPin = 18;
constexpr uint8_t kOledDataPin = 23;
constexpr uint8_t kOledCsPin = 14;
constexpr uint8_t kOledDcPin = 25;
constexpr uint8_t kOledResetPin = 26;

constexpr uint16_t kBuzzerFrequencyHz = 2400;
constexpr uint32_t kAlarmSnoozeMs = 60UL * 60UL * 1000UL;

// Cooperative-loop schedules and user-interface timing.
// Temperature sampling is intentionally slow to ignore short warm-air events
// when a refrigerated compartment is opened.
constexpr uint32_t kTemperaturePeriodMs = 30UL * 1000UL;
constexpr uint32_t kControlPeriodMs = 250;
constexpr uint32_t kDisplayPeriodMs = 250;
constexpr uint32_t kSettingsSaveDelayMs = 2UL * 1000UL;
constexpr uint32_t kEncoderButtonGuardMs = 750;
constexpr uint32_t kEncoderContinuousLimitMs = 10UL * 1000UL;
constexpr uint32_t kEncoderRecoveryQuietMs = 2UL * 1000UL;
constexpr int32_t kEncoderMaxDeltaPerPoll = 255;
constexpr uint32_t kPixelShiftPeriodMs = 5500;
constexpr uint32_t kSplashDurationMs = 15UL * 1000UL;
constexpr float kHysteresisC = 0.5f;
// Valid bounds for fridge control threshold settings.
constexpr float kFridgeControlMinC = -40.0f;
constexpr float kFridgeControlMaxC = 30.0f;
constexpr char kFirmwareVersion[] = "v0.1.0";

}  // namespace hw
