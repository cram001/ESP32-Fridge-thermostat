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
// Navigation mode uses minimum gain and a small nonzero neutral count. At this
// gain the visual ring remains effectively dark while both directions retain
// enough count headroom for reliable input.
constexpr uint8_t kEncoderNavigationGain = 1;
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

// Single persisted buzzer mode used by both the rotary menu and web settings.
constexpr uint8_t kBuzzerModeOff = 0;
constexpr uint8_t kBuzzerModeSteady = 1;
constexpr uint8_t kBuzzerModeDouble = 2;
constexpr uint8_t kBuzzerModeHiLo = 3;
constexpr uint8_t kBuzzerModeTriple = 4;
constexpr uint8_t kBuzzerModeCount = 5;
constexpr uint8_t kDefaultBuzzerMode = kBuzzerModeHiLo;
constexpr uint16_t kBuzzerSteadyFrequencyHz = 2400;
constexpr uint16_t kBuzzerHighFrequencyHz = 2800;
constexpr uint16_t kBuzzerLowFrequencyHz = 1800;

constexpr uint32_t kTemperaturePeriodMs = 5UL * 1000UL;
constexpr uint8_t kTemperatureFilterSamples = 6;
// DS18B20 temperature register power-on/reset value. In this refrigeration
// application +85 C is not a plausible compartment reading, so treat it as an
// invalid sensor state rather than accepting a reset scratchpad value.
constexpr float kDs18b20PowerOnResetC = 85.0f;
// Re-discover OneWire devices periodically. Two consecutive matching scans are
// required before the active list changes, rejecting one-off bus noise while
// still recovering automatically from replacements/reconnections.
constexpr uint32_t kSensorRescanIntervalMs = 5UL * 1000UL;
constexpr uint8_t kSensorDiscoveryConfirmations = 2;
// Sensor input is considered healthy only while recent CRC-valid samples keep
// arriving. Two transient failures are tolerated; the third failed 5-second
// sample, or 15 seconds without a good sample, marks the role read-failed.
constexpr uint8_t kSensorReadFailureLimit = 3;
constexpr uint32_t kSensorFreshnessTimeoutMs = 15UL * 1000UL;

constexpr uint32_t kControlPeriodMs = 250;
constexpr uint32_t kDisplayPeriodMs = 250;
constexpr uint32_t kEncoderButtonGuardMs = 750;
constexpr uint32_t kEncoderRecoveryQuietMs = 2UL * 1000UL;
// A lightweight I2C presence probe runs on this cadence so a dropped or
// reconnected encoder is detected during normal operation, not only at boot.
constexpr uint32_t kEncoderHealthCheckIntervalMs = 5UL * 1000UL;
constexpr int32_t kEncoderMaxDeltaPerPoll = 255;
// This SEN0502 reports two counterclockwise transitions per detent on this
// hardware; collapse those two transitions to one menu/edit step.
constexpr int32_t kEncoderCounterclockwiseCountsPerDetent = 2;
constexpr uint32_t kPixelShiftPeriodMs = 5500;
constexpr uint32_t kSplashDurationMs = 15UL * 1000UL;
// Task watchdog protects an unattended controller from a wedged loop or
// peripheral/library call. Normal loop work completes in far less than this.
constexpr uint32_t kTaskWatchdogTimeoutS = 15;

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
constexpr uint8_t kOledContrastOptions[] = {
    5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
constexpr uint8_t kOledContrastOptionCount =
    sizeof(kOledContrastOptions) / sizeof(kOledContrastOptions[0]);
constexpr uint8_t kEmergencySpilloverOptions[] = {0, 5, 10, 20, 30, 40};
constexpr uint8_t kEmergencySpilloverOptionCount = 6;
constexpr uint8_t kDisplayTimeoutOptions[] = {0, 1, 5, 10, 15, 20, 30, 60};
constexpr uint8_t kDisplayTimeoutOptionCount = 8;

// v1.0.0 is reserved for the first stable release.
constexpr char kFirmwareVersion[] = "v0.13.2";

}  // namespace hw
