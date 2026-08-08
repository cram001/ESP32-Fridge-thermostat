# Device setup and menu guide

This guide covers initial setup and normal operation of the ESP32 marine
spillover fridge controller.

## Before applying power

- Verify the ESP32 pin assignments and fan-output polarity in
  `include/hardware_config.h`.
- Drive each fan through its MOSFET/relay module. Do not power a fan directly
  from an ESP32 GPIO pin.
- Connect the fridge, freezer, and ambient DS18B20 probes to the shared 1-Wire
  input with the required pull-up resistor.
- Confirm the spillover fan moves cold freezer air into the top of the fridge.
- Confirm the circulation fan moves air through the refrigerator compartment.
- The DFRobot DFR0032 buzzer module is powered from 5 V; its signal remains on
  the configured ESP32 output.

## First startup

1. Build and upload the firmware with PlatformIO.
2. Confirm the expected firmware version on the OLED splash screen.
3. During the 15-second splash screen, verify that both fans run.
4. Complete Wi-Fi provisioning through SensESP. Signal K is optional for local
   thermostat operation.
5. Assign the probes by ROM code before relying on temperature control.
6. Review the temperature thresholds, fan delay, minimum runtimes, alarms, and
   display settings.

Existing saved settings are retained after a firmware update unless startup
validation finds an invalid/out-of-range configuration. In that case the
control/calibration settings are restored to coherent defaults and written back
to storage; saved sensor assignments and vessel name are retained.

## Home screen

The top line uses fixed regions so information never overlaps:

`Signal K status | LOCKOUT / GET-HOME | warning triangle | ambient temperature`

- `LOCKOUT` means a valid freezer reading is at/above the configured freezer
  lockout temperature, so spillover is inhibited while the freezer recovers.
- `GET-HOME` means the fridge probe has failed and a non-zero emergency
  spillover duty cycle is selected.
- If both apply, the banner alternates between them.
- The warning triangle has a fixed position immediately left of ambient
  temperature and flashes whenever any active fault/advisory exists.

## Using the rotary encoder

- From the home screen, press the encoder to enter settings. The menu opens at
  item 1.
- Rotate clockwise or counterclockwise to browse all 22 items. Browsing wraps
  in both directions.
- Press a normal setting to enter edit mode.
- The original value is captured when edit begins. Rotation previews a change
  in RAM only.
- **Press again to commit the change.** If the value changed, it is written to
  storage; a brief `SAVED` screen confirms leaving edit mode.
- If edit mode times out, the encoder disconnects, or an alarm interrupts the
  edit, the original value is restored instead of saving a partial change.
- Sensor-assignment items save immediately when the desired probe is pressed.
- The menu returns home after approximately 10 seconds without input.
- If the display is asleep, the first rotation/press only wakes it.
- During an active alarm, a press acknowledges the alarm before it can be used
  for menu navigation.

The SEN0502 LED ring is intentionally left effectively inactive. Menu navigation
and value editing both use the same stable gain-1 detent handling; the LED ring
is not used as a setting-value gauge.

## Assigning/replacing temperature probes

Probe roles are stored by each sensor's unique 64-bit ROM code, never by bus
order.

1. Open `Assign fridge`, `Assign freezer`, or `Assign ambient`.
2. Warm the desired probe with your hand.
3. Rotate through detected probes and watch the live temperature.
4. Press to save the selected probe.

Each list includes `No sensor assigned`. One physical ROM cannot remain assigned
to two roles; assigning it to a new role clears the old duplicate assignment.

The controller performs a true 1-Wire ROM search every 5 seconds and requires
two matching scans before changing the detected-device list. A newly connected
or replacement DS18B20 should therefore normally appear within about 5–10
seconds without rebooting. Discovery validates the ROM CRC and accepts the
DS18B20 family used by this controller. Reconnecting the same physical probe
preserves its saved role because assignments are stored by ROM.

A probe can remain assigned even while it is physically absent. Its saved ROM is
not cleared merely because the bus cannot currently see it. Do not intentionally
select and save `No sensor assigned` unless you want to clear that role.

The freezer probe is optional. Without it, fridge control continues but freezer
lockout is unavailable. The fridge probe is required for normal thermostat
control.

## How temperature input validity works

A new conversion is requested about every five seconds. A role is considered
healthy when its assigned ROM is present and recent valid samples continue to
arrive. The input checks are based on communication integrity and freshness,
**not on whether the numeric temperature changes**.

- The 1-Wire discovery ROM must pass CRC and be a DS18B20-family device.
- DallasTemperature validates the sensor scratchpad CRC when reading it.
- `DEVICE_DISCONNECTED`/failed reads are rejected.
- The DS18B20 +85 C power-on/reset value is rejected.
- Values outside the supported physical range are rejected.
- Two transient failed five-second reads are tolerated using the most recent
  known-good sample.
- A third failed read, or approximately 15 seconds without a good sample, marks
  the assigned role `read failed` and removes its temperature from control.
- One subsequent good sample recovers the role automatically.

A cabin, refrigerator, or freezer can legitimately stay on exactly the same
10-bit DS18B20 reading for hours. An unchanged numeric value is therefore **not**
a sensor fault and does not raise an advisory.

The displayed fridge/freezer/ambient values are rolling averages of six good
samples (roughly 30 seconds during normal operation). Freezer lockout uses the
latest valid freezer reading so safety action is not delayed by averaging.

- At `Fridge max T`, a persistent warm condition starts spillover after the fan
  trigger delay. Circulation starts with it.
- At `Fridge min T`, spillover stops only after its minimum runtime has elapsed.
- At/below `Fridge min T`, circulation can run independently after the fan
  trigger delay.
- A valid freezer reading at/above `Freez T lockout` stops/blocks spillover
  immediately, overriding spillover minimum runtime.
- `Fridge min T` must remain at least 0.5 C below `Fridge max T`.

### Control summary

| Condition | Spillover fan | Circulation fan |
|---|---:|---:|
| Fridge at/above `Fridge max T` after delay | ON | ON |
| Fridge between MIN and MAX | Normally OFF | Normally OFF |
| Fridge at/below `Fridge min T` after delay | OFF after minimum runtime | ON |
| Valid freezer at/above `Freez T lockout` | Immediately OFF | Controlled normally |

## Fridge-probe failure / GET-HOME mode

A missing, read-failed, or invalid fridge probe raises a persistent alarm and
disables normal thermostat control. Spillover remains OFF by default.

The user may deliberately select 5, 10, 20, 30, or 40 minutes ON per hour. A
valid warm freezer still enforces lockout. A missing freezer probe does not
prevent GET-HOME operation. Select OFF when the temporary mode is no longer
required.

## Buzzer

The single Buzzer setting selects both enable state and alarm pattern:

- `OFF`
- `STEADY`
- `DOUBLE`
- `HI-LO`
- `TRIPLE`

Rotating through audible modes while editing plays a brief preview. The output
test can still exercise the buzzer even if normal Buzzer mode is OFF.

## Output test

Open `Test outputs` and select:

- `SPILLOVER`
- `CIRCULATION`
- `BUZZER`
- `EXIT`

A selected output runs for five seconds and then stops automatically. Press to
stop it early. A real alarm cancels a service output test.

## Device menu reference

| Item | OLED label | Purpose and limits |
|---:|---|---|
| 1 | `Fridge max T` | Spillover ON threshold. Absolute range −9.5 to 10 C; live lower limit is item 2 + 0.5 C. |
| 2 | `Fridge min T` | Spillover OFF/cold-circulation threshold. Absolute range −10 to 9.5 C; live upper limit is item 1 − 0.5 C. |
| 3 | `Freez T lockout` | Latest valid freezer temperature that immediately blocks spillover. −30 to 10 C. |
| 4 | `Fridge alarm` | High fridge-temperature alarm. 0 to 30 C. |
| 5 | `Freezer alarm` | High freezer-temperature alarm. −20 to 10 C. |
| 6 | `Units` | Celsius/Fahrenheit for OLED/menu. Internal/SensESP values remain Celsius. |
| 7 | `Cal Fridge` | Fridge calibration offset, ±5 C equivalent. |
| 8 | `Cal Freezer` | Freezer calibration offset, ±5 C equivalent. |
| 9 | `Cal Ambient` | Ambient calibration offset, ±5 C equivalent. |
| 10 | `Fan delay` | Persistent start condition, 5–180 s in 5-s steps. |
| 11 | `Spill min ON` | Spillover minimum runtime, 1–5 min. |
| 12 | `Circ min ON` | Circulation minimum runtime, 1–5 min. |
| 13 | `Get-me-home fan` | OFF or 5/10/20/30/40 minutes ON per hour when fridge probe has failed. |
| 14 | `Buzzer` | OFF / STEADY / DOUBLE / HI-LO / TRIPLE. |
| 15 | `ACTIVE ERRORS` | Browse active faults/advisories. |
| 16 | `OLED contrast` | 5–100% selectable values. |
| 17 | `Display off` | Never, 1, 5, 10, 15, 20, 30, or 60 min. |
| 18 | `Display layout` | `FRDG | FRZ` or `FRZ | FRDG`. |
| 19 | `Test outputs` | Five-second spillover/circulation/buzzer service test. |
| 20 | `Assign fridge` | Assign/clear fridge probe. |
| 21 | `Assign freezer` | Assign/clear freezer probe. |
| 22 | `Assign ambient` | Assign/clear ambient probe. |

## Alarms and faults

- A fridge/freezer high-temperature alarm activates only after startup arming
  conditions are satisfied.
- Pressing the encoder acknowledges the audible/full-screen alert but does not
  clear the underlying alarm. The alarm clears only when its condition clears.
- Missing, read-failed, or out-of-range probes, Signal K disconnection, encoder
  problems, and a spillover run exceeding 60 minutes appear under
  `ACTIVE ERRORS`.
- A stable temperature is not a fault. Sensor validity is based on the assigned
  ROM being present plus fresh CRC-valid temperature samples.

## Long-uptime behavior

The firmware is designed for unattended continuous operation:

- periodic sensor and encoder hardware re-checks include automatic recovery;
- 1-Wire discovery uses a direct ROM search rather than DallasTemperature's
  boot-time cached device count, so probes can be added/reconnected at runtime;
- hot temperature/display paths use fixed buffers rather than temporary Arduino
  `String` objects;
- all possible detected-probe Signal K output objects are created at startup so
  runtime discovery does not allocate new output objects;
- the loop task is supervised by the ESP32 task watchdog and will reset on a
  sustained software/peripheral wedge;
- timers use unsigned elapsed-time arithmetic so normal control continues
  through the `millis()` rollover.

## Development checks

Run the host-side controller tests without ESP32 hardware:

```sh
./tools/run-native-tests.sh
```

The native suite includes rollover coverage for fan qualification, minimum fan
runtime, GET-HOME hourly timing, and sensor freshness/failure recovery logic.
