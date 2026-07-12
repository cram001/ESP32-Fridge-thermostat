#pragma once
#include <Arduino.h>

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
