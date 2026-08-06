#pragma once

#include <Arduino.h>

namespace hw {

// DFRobot FireBeetle 2 ESP32-E N16R2 (DFR1139) installed on the
// DFR0923 terminal-block board. Comments include the DFR0923 screw-terminal
// labels used during assembly.
constexpr uint8_t kOneWirePin = 4;          // D12
constexpr uint8_t kSpilloverFanPin = 17;   // D10
constexpr uint8_t kCirculationFanPin = 13; // D7
constexpr uint8_t kBuzzerPin = 19;          // MI (SPI MISO; unused by OLED)
constexpr bool kFanActiveHigh = true;

constexpr uint8_t kI2cSdaPin = 21;  // SDA
constexpr uint8_t kI2cSclPin = 22;  // SCL
constexpr uint8_t kEncoderAddress = 0x54;  // SEN0502 DIP switches both OFF
// Minimum gain keeps the position ring effectively inactive and provides
// one raw count per detent for predictable input handling.
constexpr uint8_t kEncoderGain = 1;
// The counter is bounded to 0..1023 rather than wrapping. Reset it to a small
// nonzero neutral value after movement: this leaves ample travel between polls
// in either direction without advancing the low-gain LED ring.
constexpr uint16_t kEncoderNeutralValue = 32;

constexpr uint32_t kSignalKFaultGraceMs = 60UL * 1000UL;
constexpr uint32_t kStartupAlarmGraceMs = 2UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kLongFanRunMs = 60UL * 60UL * 1000UL;

// SSD1309 128x64 OLED, 4-wire SPI. OLED silkscreen -> DFR0923 terminal:
// SCK -> SCK, SDA (SPI MOSI) -> MO, RES -> D3, DC -> D2, CS -> D6.
constexpr uint8_t kOledClockPin = 18;  // SCK
constexpr uint8_t kOledDataPin = 23;   // MO
constexpr uint8_t kOledCsPin = 14;     // D6
constexpr uint8_t kOledDcPin = 25;     // D2
constexpr uint8_t kOledResetPin = 26;  // D3

constexpr uint16_t kBuzzerFrequencyHz = 2400;
constexpr uint32_t kTemperaturePeriodMs = 5UL * 1000UL;
constexpr uint8_t kTemperatureFilterSamples = 6;
constexpr uint32_t kControlPeriodMs = 250;
constexpr uint32_t kDisplayPeriodMs = 250;
constexpr uint32_t kSettingsSaveDelayMs = 2UL * 1000UL;
constexpr uint32_t kEncoderButtonGuardMs = 750;
constexpr uint32_t kEncoderRecoveryQuietMs = 2UL * 1000UL;
constexpr int32_t kEncoderMaxDeltaPerPoll = 255;
constexpr uint32_t kPixelShiftPeriodMs = 5500;
constexpr uint32_t kSplashDurationMs = 15UL * 1000UL;
// User-editable temperature ranges and increments.
constexpr float kTemperatureEditStepC = 0.1f;
constexpr float kFridgeControlMinC = -10.0f;
constexpr float kFridgeControlMaxC = 10.0f;
constexpr float kFridgeControlMinimumBandC = 0.5f;
constexpr float kFreezerThresholdMinC = -30.0f;
constexpr float kFreezerThresholdMaxC = 10.0f;
constexpr float kFridgeAlarmMinC = 0.0f;
constexpr float kFridgeAlarmMaxC = 30.0f;
constexpr float kFreezerAlarmMinC = -20.0f;
constexpr float kFreezerAlarmMaxC = 10.0f;
constexpr float kCalibrationLimitC = 5.0f;
constexpr uint16_t kFanDelayMinS = 5;
constexpr uint16_t kFanDelayMaxS = 180;
constexpr uint16_t kFanDelayStepS = 5;
constexpr uint8_t kFanMinimumOnMin = 1;
constexpr uint8_t kFanMinimumOnMax = 5;
constexpr uint8_t kOledContrastMinPercent = 10;
constexpr uint8_t kOledContrastMaxPercent = 100;
constexpr uint8_t kOledContrastStepPercent = 10;
constexpr uint8_t kEmergencySpilloverOptions[] = {0, 5, 10, 20, 30, 40};
constexpr uint8_t kEmergencySpilloverOptionCount = 6;
constexpr uint8_t kDisplayTimeoutOptions[] = {0, 1, 5, 10, 15, 20, 30, 60};
constexpr uint8_t kDisplayTimeoutOptionCount = 8;

// Bump the minor number once per firmware PR so the startup screen confirms
// which merged revision was uploaded: v0.1.0, v0.2.0, ... v0.10.0, etc.
// v1.0.0 is reserved for the first stable release.
constexpr char kFirmwareVersion[] = "v0.4.0";

}  // namespace hw
