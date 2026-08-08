#include <cassert>
#include <cstdint>
#include <cstring>

#include "cerbo_mqtt_interval.h"

int main() {
  using namespace cerbo_mqtt;

  assert(kReportIntervalCount == 6);
  assert(kReportIntervalsS[0] == 0);
  assert(kReportIntervalsS[1] == 30);
  assert(kReportIntervalsS[2] == 60);
  assert(kReportIntervalsS[3] == 120);
  assert(kReportIntervalsS[4] == 300);
  assert(kReportIntervalsS[5] == 600);

  for (size_t i = 0; i < kReportIntervalCount; ++i) {
    assert(IsAllowedReportInterval(kReportIntervalsS[i]));
    assert(ReportIntervalIndex(kReportIntervalsS[i]) == i);
  }

  assert(!IsAllowedReportInterval(1));
  assert(!IsAllowedReportInterval(45));
  assert(!IsAllowedReportInterval(601));
  assert(ReportIntervalIndex(45) == 0);

  assert(std::strcmp(ReportIntervalLabel(0), "OFF") == 0);
  assert(std::strcmp(ReportIntervalLabel(30), "30 sec") == 0);
  assert(std::strcmp(ReportIntervalLabel(60), "1 min") == 0);
  assert(std::strcmp(ReportIntervalLabel(120), "2 min") == 0);
  assert(std::strcmp(ReportIntervalLabel(300), "5 min") == 0);
  assert(std::strcmp(ReportIntervalLabel(600), "10 min") == 0);
  assert(std::strcmp(ReportIntervalLabel(45), "OFF") == 0);

  return 0;
}
