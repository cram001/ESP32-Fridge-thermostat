# Failure-mode analysis and simulation

This document records unattended-operation failure cases exercised by
`test/native/failure_mode_tests.cpp`. The simulation intentionally separates
**firmware command state** from **physical hardware state**. The controller can
know that it commanded a fan ON or OFF, but with the present hardware it has no
fan RPM, airflow, current, or compressor-run feedback.

The thermal model used by the tests is deliberately simple and deterministic.
It is not a calibrated refrigeration model and must not be used to predict real
pull-down times. Its purpose is to drive the real `FridgeController` and
`EmergencySpilloverController` through plausible changing temperatures and
verify state transitions, alarms, lockout behavior, and known observability
limits.

## Summary

| Failure | Firmware response | Protection | Limitation |
|---|---|---|---|
| Fridge probe missing/invalid | Critical probe alarm; normal thermostat disabled | Spillover and circulation OFF by default | User must deliberately enable GET-HOME if desired |
| Fridge probe missing + GET-HOME | Timed spillover duty cycle | Valid warm freezer still blocks spillover | No real fridge-temperature control while probe is failed |
| Freezer probe missing/invalid | Active error; fridge control continues | Fridge remains controlled | Freezer lockout and freezer-temperature alarm are unavailable |
| DS18B20 reports +85°C reset value | Reading rejected as invalid immediately | Avoids accepting the sensor power-on/reset register value as real temperature | A real +85°C compartment temperature is intentionally outside this application's accepted behavior |
| Assigned sensor has transient read/CRC failures | Most recent good sample retained briefly | Two failed 5-second reads are tolerated | Third failure or ~15 s without a good sample marks role read-failed |
| Spillover fan mechanically/electrically fails OFF | Controller continues commanding ON | Fridge high-temperature alarm can occur; >60 min commanded-run fault occurs | No direct proof that fan failed; detection depends on resulting temperatures/time |
| Spillover fan/MOSFET stuck ON | Controller may command OFF | Temperature/lockout alarms may eventually reveal consequences | **No direct detection with current hardware** |
| Circulation fan fails OFF | Controller still commands circulation | Fridge temperature control may still function | **No direct detection with current hardware** |
| Circulation fan/MOSFET stuck ON | Controller may command OFF | Usually limited safety consequence | **No direct detection with current hardware** |
| Freezer compressor/refrigeration failure, freezer probe valid | Freezer warms; spillover locks out at configured threshold; freezer alarm follows | Prevents deliberate spillover from adding load to already-warm freezer | Compressor itself is inferred from temperature only |
| Compressor failure + freezer probe missing | Freezer-missing error only | None from freezer temperature | **Lockout/alarm cannot operate; fridge spillover control continues** |
| Compressor failure + freezer probe stuck plausibly cold | Reading remains apparently valid if communication/CRC are good | None from temperature alone | **Cold false reading can mask freezer warming until independent observation** |
| Compressor failure during startup alarm grace | Freezer lockout still works immediately | Spillover protection remains | High-temperature alarm is intentionally delayed until alarm arming/grace criteria are met |

## Sensor failure cases

### Fridge probe disconnected, open circuit, invalid, or out of range

The fridge probe is safety-critical. When it is invalid, normal thermostat
control is disabled and a critical probe alarm is raised. Both fans are OFF by
default. If the user has deliberately selected a GET-HOME duty cycle, the
spillover fan uses that duty cycle instead. A valid freezer reading at or above
the freezer-lockout threshold still blocks GET-HOME spillover.

This is the desired fail-safe behavior: loss of the primary control sensor does
not silently leave a fan energized.

### Freezer probe disconnected or invalid

The freezer probe is optional by design. Its failure raises an error but does
not stop fridge control. The tradeoff is explicit: without a valid freezer
probe, the firmware cannot know that the freezer is warming, so freezer lockout
and freezer-temperature alarm protection are unavailable.

A combined freezer-probe failure plus compressor/refrigeration failure is
therefore a significant degraded mode. The controller can report the missing
probe, but cannot infer freezer temperature from the fridge probe alone.

### DS18B20 +85°C power-on/reset value

The DS18B20 temperature register powers up at +85°C. For this refrigerator
controller, +85°C is not a plausible compartment value, so firmware rejects it
immediately as an invalid reading rather than accepting a reset scratchpad value
as real temperature.

### Ambient probe failure

Ambient temperature is informational and does not participate in refrigeration
control. Missing, read-failed, and range conditions remain advisory faults rather
than changing fan behavior. A stable cabin temperature is not a fault: repeated
CRC-valid samples remain healthy even if the numeric value is unchanged for hours.

### Sensor input validity and freshness

Each assigned role is healthy only while its ROM is present and recent valid
samples continue to arrive. Runtime discovery performs a direct OneWire ROM
search, validates the ROM CRC, and accepts the DS18B20 family used by this
controller. DallasTemperature validates the scratchpad CRC during temperature
reads. The firmware also rejects failed/disconnected reads, the DS18B20 +85°C
power-on value, and readings outside the supported physical range.

Two transient failed 5-second reads are tolerated using the most recent known-
good sample. A third failed read, or approximately 15 seconds without a good
sample, changes the role to `read failed`. One subsequent good sample recovers
automatically.

Numeric movement is deliberately not a validity requirement. A refrigerator,
freezer, or cabin can legitimately remain on the same 0.25°C 10-bit reading for
hours, so an unchanged-value alarm would produce false positives and is not
used.

### Runtime discovery / reconnect

DallasTemperature caches its device count during `begin()`, so runtime recovery
does not use that cached count or indexed discovery. The firmware performs a
direct OneWire ROM search every five seconds and requires two matching scans
before committing a changed list. This allows hot-plugged, reconnected, and
replacement DS18B20s to appear without rebooting while still rejecting a single
noisy scan. Newly committed probes are explicitly configured to 10-bit
resolution before their readings are relied upon.

Saved role assignments are independent of current bus presence. Disconnecting a
probe does not erase its ROM assignment; reconnecting the same ROM restores the
role automatically once discovery and valid samples resume.

## Fan failure cases

### Spillover fan fails OFF

The firmware has no tachometer or current sensor. It therefore continues to
command the spillover fan ON when the fridge is warm. In the simulation, the
physical fridge continues warming because the fan produces no airflow. Two
indirect indications are then available:

1. the normal fridge high-temperature alarm when the measured fridge
   temperature reaches its configured limit; and
2. the existing `Spillover running >60m` fault after spillover has been
   commanded continuously for more than 60 minutes.

The 60-minute fault is correctly described as a **commanded long run**, not
proof of fan rotation or fan failure.

### Spillover fan or MOSFET stuck ON

This is not directly detectable with the current wiring. The ESP32 can drive
the output LOW and believe the fan is OFF while a shorted MOSFET, welded relay,
or other downstream failure keeps the fan physically energized.

Freezer warming may eventually cause freezer lockout and alarms, but lockout can
only change the command; it cannot open a failed-short power device. Direct
coverage would require independent feedback such as fan tach/RPM, airflow, or a
current-sense input.

### Circulation fan failed OFF or stuck ON

These failures are also not directly observable. A failed circulation fan can
produce temperature stratification, but the controller has only the compartment
temperature probes and no proof of fan motion. A stuck-ON circulation fan is
usually less safety-critical but is likewise invisible electrically.

The manual Output Test menu remains useful for maintenance because a person can
hear/feel airflow, but it is not unattended failure detection.

## Freezer compressor / refrigeration-system failure

The controller does not command or monitor the freezer compressor. Compressor
failure, loss of refrigerant, failed compressor electronics, blocked condenser,
or similar refrigeration failures are therefore observed indirectly through
the freezer-temperature probe.

With a valid freezer probe, the behavior is good:

1. the freezer begins warming;
2. when it reaches `Freez T lockout`, spillover is stopped immediately, even if
   the spillover minimum runtime has not completed;
3. the `LOCKOUT` banner explains why spillover is inhibited; and
4. if warming continues to the configured freezer alarm temperature after alarm
   arming, the normal high-temperature alarm activates.

This prevents the spillover controller from intentionally adding more heat load
to a freezer that is already unable to maintain temperature.

### Compressor failure combined with freezer-probe failure

This is a two-fault condition the firmware cannot solve using existing sensors.
The missing-freezer error is visible, but freezer lockout and freezer alarm are
necessarily unavailable. If the fridge is warm and its sensor remains healthy,
spillover control continues as designed.

### Compressor failure combined with a stuck-cold freezer reading

A plausible cold reading can prevent lockout and freezer high-temperature alarm
even while the real freezer warms. If the sensor continues returning CRC-valid
data, software cannot reliably distinguish a physically stuck or misplaced probe
from a genuinely stable freezer. Independent compressor/fan/current/temperature
feedback would be required to make this condition fully deterministic.

## Startup alarm grace

Freezer lockout is independent of temperature-alarm arming. Therefore a warm
freezer can block spillover immediately during startup even while the
high-temperature alarm is intentionally in its startup grace period.

This is an important distinction: startup grace suppresses nuisance audible/
full-screen temperature alarms during pull-down, but it does not disable the
freezer-protection lockout.

## Running the simulations

Run the controller unit tests, failure-mode simulations, and sensor-health tests
with:

```sh
./tools/run-native-tests.sh
```

`LIMITATION:` lines are expected results, not test failures. They document
conditions the present hardware architecture cannot directly observe.

The same command runs automatically in the `Native controller tests` GitHub
Actions workflow for pushes and pull requests.

## Hardware improvements that would close remaining blind spots

No firmware-only change can prove fan rotation or compressor operation from an
output command. If later hardware revisions need deterministic detection,
consider one or more of:

- tachometer-capable fans and ESP32 RPM inputs;
- fan-current sensing after each MOSFET;
- airflow switches/sensors;
- freezer compressor current/run-state input; or
- an independent additional freezer-temperature sensor.

The current firmware should not infer these hardware states with certainty from
slow thermal response alone, because door openings, food loading, ambient
conditions, compressor cycling, and sensor placement can produce similar
thermal signatures.
