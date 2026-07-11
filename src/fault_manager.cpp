#include "fault_manager.h"

namespace {
constexpr FaultEntry kFaults[] = {
    {FaultCode::kFridgeMissing, "Fridge sensor missing"},
    {FaultCode::kFridgeRange, "Fridge sensor range"},
    {FaultCode::kFreezerMissing, "Freezer sensor missing"},
    {FaultCode::kFreezerRange, "Freezer sensor range"},
    {FaultCode::kAmbientMissing, "Ambient sensor missing"},
    {FaultCode::kAmbientRange, "Ambient sensor range"},
    {FaultCode::kSignalKOffline, "Signal K offline"},
    {FaultCode::kSpilloverLongRun, "Spill fan over 60m"},
    {FaultCode::kEncoderOffline, "Encoder offline"},
};
}

void FaultManager::set(FaultCode code, bool now_active) {
  const uint16_t bit = 1U << (static_cast<uint8_t>(code) - 1);
  if (now_active) active_mask_ |= bit; else active_mask_ &= ~bit;
}
bool FaultManager::active(FaultCode code) const {
  return active_mask_ & (1U << (static_cast<uint8_t>(code) - 1));
}
uint8_t FaultManager::count() const {
  uint8_t n = 0;
  for (const auto& fault : kFaults) n += active(fault.code);
  return n;
}
FaultEntry FaultManager::entry(uint8_t index) const {
  for (const auto& fault : kFaults) {
    if (active(fault.code) && index-- == 0) return fault;
  }
  return {FaultCode::kFridgeMissing, "No active errors"};
}
