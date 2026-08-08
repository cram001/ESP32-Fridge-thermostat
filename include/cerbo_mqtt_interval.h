#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cerbo_mqtt {

constexpr uint16_t kReportIntervalsS[] = {0, 30, 60, 120, 300, 600};
constexpr size_t kReportIntervalCount =
    sizeof(kReportIntervalsS) / sizeof(kReportIntervalsS[0]);

inline bool IsAllowedReportInterval(uint16_t seconds) {
  for (size_t i = 0; i < kReportIntervalCount; ++i) {
    if (kReportIntervalsS[i] == seconds) return true;
  }
  return false;
}

inline size_t ReportIntervalIndex(uint16_t seconds) {
  for (size_t i = 0; i < kReportIntervalCount; ++i) {
    if (kReportIntervalsS[i] == seconds) return i;
  }
  return 0;
}

inline const char* ReportIntervalLabel(uint16_t seconds) {
  switch (seconds) {
    case 30:
      return "30 sec";
    case 60:
      return "1 min";
    case 120:
      return "2 min";
    case 300:
      return "5 min";
    case 600:
      return "10 min";
    default:
      return "OFF";
  }
}

}  // namespace cerbo_mqtt
