// Declares the compact active-fault registry used by alarms and the OLED UI.
// Faults are transient conditions represented in a bit mask; clearing the
// underlying condition removes the corresponding entry automatically.
#pragma once
#include <Arduino.h>

// Values start at one because each code maps to bit (value - 1).
enum class FaultCode : uint8_t {
  kFridgeMissing = 1, kFridgeRange, kFreezerMissing, kFreezerRange,
  kAmbientMissing, kAmbientRange, kSignalKOffline, kSpilloverLongRun,
  kEncoderOffline, kEncoderErratic
};

struct FaultEntry { FaultCode code; const char* message; };

// Active-state fault list: messages disappear when their condition clears.
class FaultManager {
 public:
  void set(FaultCode code, bool active);
  bool active(FaultCode code) const;
  uint8_t count() const;
  FaultEntry entry(uint8_t active_index) const;
 private:
  uint16_t active_mask_ = 0;
};
