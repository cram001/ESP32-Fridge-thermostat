# ESP32 marine spillover fridge controller

SensESP firmware for a two-fan marine spillover refrigerator.

The controller reads fridge, freezer, and cabin DS18B20 temperature sensors;
controls spillover and internal circulation fans; displays status on a 128x64
SSD1309 OLED; and publishes readings/state to Signal K over Wi-Fi.

It is intended to run unattended, 24/7, for months at a time. Long-uptime
reliability is therefore a design requirement: periodic/hot paths avoid
avoidable heap allocation, peripherals that can be health-checked are rechecked
at runtime and automatically recovered when possible, and the main loop is
supervised by the ESP32 task watchdog.

For complete installation, first-start, sensor-assignment, control, and menu
instructions, see [Device setup and menu guide](docs/USER_GUIDE.md).

## Bring-up

1. Verify every pin and output polarity in `include/hardware_config.h` before
   connecting fan drivers. Fans require MOSFET/relay drivers; do not power them
   from ESP32 GPIO pins.
2. Connect DS18B20 sensors to the shared 1-Wire bus with the required pull-up.
3. Build/upload with PlatformIO, then configure Wi-Fi and Signal K through the
   SensESP web interface.
4. The 15-second splash screen is also a fan output test: both fan outputs stay
   ON for the countdown. Confirm both fans run.
5. Assign each temperature role explicitly by sensor ROM.
6. Use `Test outputs` for an on-demand five-second spillover, circulation, or
   buzzer service test.

Sensor roles are never assigned automatically by OneWire discovery order. They
follow saved 64-bit ROMs, so reconnecting wiring cannot swap fridge/freezer
roles. The bus is re-scanned every 30 seconds and requires two matching scans
before changing the detected-device list, so a replacement/reconnected probe
appears automatically without requiring a reboot.

The firmware version shown on the startup screen is incremented for every
software change. `v1.0.0` is reserved for the first stable release; pre-stable
work may use both minor and patch increments. Confirm the splash-screen version
after every upload.

## Temperature control

The displayed fridge, freezer, and ambient temperatures are rolling averages of
six readings sampled about every five seconds. Normal fan control uses the
filtered fridge temperature. Freezer lockout uses the latest valid freezer
reading so it can stop spillover without waiting for the average.

- At `Fridge max T`, a persistent warm condition starts spillover after the fan
  trigger delay. Circulation starts with it.
- At `Fridge min T`, spillover stops only after its configured minimum runtime.
- At/below `Fridge min T`, circulation can also run independently after the fan
  trigger delay.
- A valid freezer reading at/above `Freez T lockout` immediately blocks/stops
  spillover.
- `Fridge min T` remains at least 0.5 C below `Fridge max T`.

The freezer probe is optional for control. If missing, fridge control continues
without freezer lockout. A failed fridge probe disables normal thermostat
control and raises an alarm.

## Fridge-probe failure / GET-HOME

A failed fridge probe leaves the spillover fan OFF by default. The user can
explicitly select a temporary duty cycle of 5, 10, 20, 30, or 40 minutes ON per
hour. A valid warm freezer still enforces lockout; a missing freezer probe does
not block GET-HOME operation.

The home-screen top row uses fixed regions:

`Signal K | LOCKOUT / GET-HOME | warning triangle | ambient temperature`

`LOCKOUT` indicates spillover is inhibited because the freezer is recovering.
`GET-HOME` indicates fridge-probe-failure duty-cycle mode is configured. If both
apply, the banner alternates between them.

## Alarms and diagnostics

Pressing the encoder acknowledges an active alarm. This silences the buzzer and
full-screen visual alert, but the alarm remains logically active in Signal K
until the underlying condition clears.

The Buzzer setting combines enable state and sound selection:
`OFF`, `STEADY`, `DOUBLE`, `HI-LO`, or `TRIPLE`.

Active errors/advisories include sensor missing/range faults, Signal K offline,
encoder faults, long spillover runtime, and a `temp not changing` advisory when
a valid probe's raw reading has not moved by a DS18B20 count for 30 minutes.
That advisory does not by itself disable thermostat control.

## Long-uptime reliability

Current reliability measures include:

- non-blocking DS18B20 conversions;
- automatic OneWire re-scan/recovery with debounced discovery changes;
- periodic SEN0502 I2C health checks and automatic encoder reinitialization;
- fixed-size ROM/display buffers on periodic paths instead of temporary Arduino
  `String` construction;
- detected-probe Signal K output objects pre-created at startup so runtime
  discovery does not allocate new output objects;
- ROM metadata published only when the discovered sensor list changes;
- startup settings sanity validation with coherent default restoration;
- transactional rotary edits: press commits and shows `SAVED`; timeout,
  encoder loss, or alarm interruption rolls back the in-progress value;
- ESP32 task watchdog supervision;
- unsigned elapsed-time calculations and native rollover tests for long-running
  timers.

Hardware that provides no feedback channel (the SPI OLED and simple fan/buzzer
outputs) cannot be positively health-checked in software. The startup fan test
and `Test outputs` menu provide manual verification for those outputs.

## Development checks

Run the host-side controller tests without ESP32 hardware:

```sh
./tools/run-native-tests.sh
```

The test suite includes controller timing, get-home behavior, configuration
normalization, and `millis()` rollover cases.

## Hardware

From DFRobot:
1. FireBeetle 2 ESP32-E N16R2
2. Gravity Digital Buzzer (DFR0032)
3. Gravity MOSFET Power Controller (x2)
4. Waterproof DS18B20 temperature sensors
5. Gravity SEN0502 visual rotary encoder
6. DFR0923 FireBeetle terminal-block board

Other:
1. HiLetgo 2.42-inch SSD1309 128x64 SPI OLED
2. 3D-printed enclosure

## Future ideas

- Optional PWM fan / variable-speed control
- Humidity sensing for defrost/icing guidance
- Ambient-light-based display brightness
- Door-open sensing and alarm integration
