# ESP32 Marine Spillover Fridge Controller

A standalone ESP32 thermostat/controller for a two-fan marine spillover refrigerator, with local OLED/rotary control and optional Signal K integration.

The controller is designed to run unattended 24/7 for long periods. It monitors refrigerator, freezer, and ambient temperatures; controls the spillover and circulation fans; provides alarms and a sensor-failure fallback mode; and automatically recovers from supported sensor and encoder disconnect/reconnect events without requiring a reboot.

**Current beta:** `v0.13.4-beta.1`

> `v1.0.0` is reserved for the first stable release.

For the complete operating and menu reference, see [Device setup and menu guide](docs/USER_GUIDE.md). For fault behavior and simulation coverage, see [Failure-mode analysis and simulation](docs/FAILURE_MODE_ANALYSIS.md).

---

## Features

### Refrigeration control

- Two-fan spillover refrigerator control:
  - **Spillover fan** moves cold air from the freezer into the refrigerator.
  - **Circulation fan** circulates air within the refrigerator compartment.
- Configurable refrigerator ON and OFF temperature thresholds.
- Configurable fan start delay to avoid reacting to short temperature changes.
- Independent minimum run times for spillover and circulation fans.
- Optional freezer-temperature lockout that immediately blocks spillover when the freezer is too warm.
- Refrigerator, freezer, and ambient temperature calibration.
- Celsius or Fahrenheit OLED display.
- Six-sample rolling average for displayed/control temperatures, while freezer lockout uses the latest valid freezer sample for faster protective action.

### Temperature sensors

- Up to three DS18B20 roles:
  - Refrigerator
  - Freezer
  - Ambient/cabin
- Sensors are assigned by their unique 64-bit ROM address, **not by OneWire bus order**.
- Refrigerator probe is required for automatic thermostat operation.
- Freezer probe is optional; automatic refrigerator control continues without it, but freezer lockout is unavailable.
- Ambient probe is optional.
- Direct OneWire ROM rediscovery every 5 seconds.
- New/reconnected sensors normally appear within approximately 5–10 seconds without rebooting.
- Sensor validity checks include ROM CRC, scratchpad validity, failed/disconnected reads, freshness, and rejection of the DS18B20 `+85 C` power-on/reset value.
- A sensor is declared failed after three consecutive failed 5-second reads or approximately 15 seconds without a valid sample.
- A single later valid sample automatically restores that sensor role.

### GET-HOME fallback mode

If the refrigerator probe fails, normal thermostat control is disabled and the spillover fan defaults OFF.

The user can manually select a temporary spillover duty cycle of:

- OFF
- 5 minutes/hour
- 10 minutes/hour
- 20 minutes/hour
- 30 minutes/hour
- 40 minutes/hour

When the refrigerator sensor becomes healthy again, **GET-HOME automatically exits and normal automatic thermostat control resumes**.

### Local user interface

- 2.42-inch 128x64 SSD1309 SPI OLED.
- DFRobot SEN0502 I2C rotary encoder with pushbutton.
- 23-item local settings/service menu.
- Transactional editing: changes are previewed in RAM and only committed when the encoder is pressed again.
- Timed-out, interrupted, or aborted edits restore the previous value instead of saving partial changes.
- `SAVED` confirmation after committed changes.
- User-selectable display layout:
  - `FRDG | FRZ`
  - `FRZ | FRDG`
- Fan indications follow the physical compartment layout:
  - **CIRC** is grouped with **FRDG**.
  - **SPILL** is grouped with **FRZ**.
- Animated fan symbols indicate active outputs.
- Configurable OLED contrast and display timeout.
- Pixel shifting reduces OLED burn-in risk.
- `About` screen shows firmware version and build date.

### Alarms and service functions

- Configurable high refrigerator alarm.
- Configurable high freezer alarm.
- Persistent alarm state until the underlying fault clears.
- Encoder press acknowledges the audible/full-screen alert without hiding the underlying active fault.
- Selectable buzzer modes:
  - OFF
  - STEADY
  - DOUBLE
  - HI-LO
  - TRIPLE
- `ACTIVE ERRORS` menu.
- Five-second output test for:
  - Spillover fan
  - Circulation fan
  - Buzzer
- The 15-second startup splash also runs both fans as an immediate installation/output check.

### Signal K / networking

- Built on SensESP.
- Wi-Fi configuration through the SensESP web interface.
- Publishes temperatures and controller state to Signal K.
- Signal K is **optional** for local thermostat operation.
- Signal K data can be used by dashboards, Node-RED, logging systems, and other marine integrations.

### Long-uptime reliability

The project is deliberately optimized for unattended embedded operation rather than only short bench tests.

- Periodic temperature-sensor rediscovery and automatic recovery.
- Periodic encoder I2C health checks and automatic reinitialization.
- Vendored DFRobot SEN0502 driver with retry/bus-recovery handling.
- Last valid encoder count is preserved across transient failed reads to avoid false large input jumps.
- ESP32 task watchdog protection.
- Unsigned elapsed-time logic for normal operation through `millis()` rollover.
- Hot/periodic code paths avoid repeated Arduino `String` allocations where practical.
- Platform and major library versions are pinned for reproducible builds.
- Native controller/failure-mode tests plus a full PlatformIO firmware build run in GitHub Actions.

---

## Hardware requirements

The current firmware targets the following hardware.

### Controller and interface

| Qty | Hardware | Notes |
|---:|---|---|
| 1 | **DFRobot FireBeetle 2 ESP32-E N16R2 (DFR1139)** | 16 MB flash version used for development/testing |
| 1 | **DFRobot DFR0923 FireBeetle 2 Terminal Block Expansion Board** | Recommended; provides labelled terminals and accepts 7–24 V input |
| 1 | **DFRobot SEN0502 Gravity Visual Rotary Encoder** | I2C, default address `0x54` |
| 1 | **2.42-inch SSD1309 128x64 OLED** | 4-wire SPI; HiLetgo module used for development |
| 1 | **DFRobot DFR0032 Gravity Digital Buzzer** | 5 V module |

### Temperature sensing

| Qty | Hardware | Notes |
|---:|---|---|
| 1–3 | **DS18B20 waterproof temperature probes** | Refrigerator required; freezer and ambient optional |
| 1 | **4.7 kΩ resistor** | OneWire data pull-up to 3.3 V |

### Fan switching

| Qty | Hardware | Notes |
|---:|---|---|
| 2 | **Logic-compatible MOSFET/relay fan driver modules** | One for spillover, one for circulation |
| 2 | **12 V fans** or application-appropriate fans | Size/current depend on installation |

**Do not power the fans directly from ESP32 GPIO pins.** The ESP32 outputs must drive suitable MOSFET/relay modules. Use appropriate flyback/transient protection for inductive loads.

### Power

- DFR0923 external input: **7–24 VDC**.
- In a typical marine installation the controller can therefore be supplied from the vessel's nominal 12 VDC system through the DFR0923 board.
- Verify polarity, fusing, fan voltage, and fan-driver ratings before connecting vessel power.

---

## Wiring reference

These are the current firmware assignments using the **DFR0923 terminal labels**.

| Function | DFR0923 terminal / connection |
|---|---|
| DS18B20 OneWire DATA | `D12` |
| DS18B20 VCC | `3V3` |
| DS18B20 GND | `GND` |
| OneWire pull-up | 4.7 kΩ between `D12` and `3V3` |
| Spillover fan driver signal | `D10` |
| Circulation fan driver signal | `D7` |
| Buzzer signal | `MI` |
| Buzzer VCC | `5V` |
| Buzzer GND | `GND` |
| SEN0502 encoder | I2C `SDA`, `SCL`, power and GND |
| OLED SCK | `SCK` |
| OLED SDA/MOSI | `MO` |
| OLED CS | `D6` |
| OLED DC | `D2` |
| OLED RESET | `D3` |

The SEN0502 is expected at I2C address `0x54` with both address DIP switches OFF.

Before energizing a new installation, compare these connections with [`include/hardware_config.h`](include/hardware_config.h), which is the firmware source of truth.

---

## New installation / first configuration

### 1. Assemble and wire the hardware

1. Install the FireBeetle 2 ESP32-E on the DFR0923 terminal board.
2. Connect the OLED, rotary encoder, buzzer, and DS18B20 OneWire bus using the wiring table above.
3. Install the 4.7 kΩ OneWire pull-up between `D12` and `3V3`.
4. Connect the spillover and circulation outputs to their external MOSFET/relay drivers.
5. Connect the fans to the fan drivers and their correctly fused power source.
6. Confirm the spillover fan physically moves cold air from the freezer toward the refrigerator.
7. Confirm the circulation fan circulates air in the refrigerator compartment.
8. Re-check power polarity and output wiring before applying vessel power.

### 2. Build and upload the firmware

This is a PlatformIO project.

1. Clone/download this repository.
2. Open the project folder in VS Code with the PlatformIO extension installed.
3. In PlatformIO, select the environment:
   - `firebeetle2_esp32e_n16r2`
4. Run **Build**.
5. Connect the FireBeetle by USB.
6. Run **Upload**.
7. Confirm the OLED splash screen shows the expected firmware version.

The firmware version shown on the OLED is the easiest way to confirm exactly which build is running.

### 3. Verify the startup hardware test

At boot the splash screen remains visible for approximately **15 seconds**.

During that period:

- Spillover fan output should be ON.
- Circulation fan output should be ON.
- The splash screen counts down.

Confirm both physical fans operate correctly. When the splash ends, the controller leaves the startup test and enters normal control.

If a fan does not run, correct the wiring/driver problem before relying on the thermostat.

### 4. Configure Wi-Fi and optional Signal K

Use the SensESP web configuration interface to configure Wi-Fi and, if desired, Signal K.

Signal K is not required for local thermostat operation. The OLED, encoder, temperature inputs, alarms, and fan control continue to operate locally if Signal K is unavailable.

### 5. Assign the temperature probes

Sensor roles are **never assigned automatically from OneWire order**.

Using the OLED menu:

1. Press the encoder from the home screen.
2. Navigate to `Assign fridge`.
3. Warm the intended refrigerator probe with your hand.
4. Rotate through the detected probes while watching the live temperature.
5. Press the encoder on the correct probe to save its ROM address.
6. Repeat for `Assign freezer` if a freezer probe is installed.
7. Repeat for `Assign ambient` if an ambient probe is installed.

A replacement sensor can be assigned the same way. Saving the replacement ROM overwrites the old assignment for that role.

### 6. Configure the thermostat

Review these settings before leaving the controller unattended:

- `Fridge max T` — temperature at which spillover cooling is requested.
- `Fridge min T` — lower control threshold.
- `Freez T lockout` — freezer temperature above which spillover is blocked.
- `Fridge alarm`.
- `Freezer alarm`.
- `Fan delay`.
- `Spill min ON`.
- `Circ min ON`.
- `Get-me-home fan` — normally leave OFF; used only after refrigerator-probe failure.
- `Buzzer`.
- `OLED contrast`.
- `Display off`.
- `Display layout`.

`Fridge min T` must remain at least **0.5 C below** `Fridge max T`.

### 7. Run the service output test

Before commissioning:

1. Open `Test outputs`.
2. Test `SPILLOVER`.
3. Test `CIRCULATION`.
4. Test `BUZZER`.
5. Confirm each physical device matches the selected output.

Each test automatically stops after five seconds and can also be stopped early by pressing the encoder.

### 8. Test the sensor-failure recovery path

For a new installation it is worth validating the fallback before leaving the system unattended:

1. Disconnect the refrigerator temperature probe/OneWire bus.
2. Confirm the controller eventually reports the refrigerator sensor fault and leaves normal thermostat control.
3. If desired, select a GET-HOME duty cycle and confirm the fallback fan operation.
4. Reconnect the temperature sensor bus.
5. Confirm the assigned probes return automatically.
6. Confirm GET-HOME clears and automatic thermostat control resumes once the refrigerator probe is healthy.

---

## Normal control summary

| Condition | Spillover fan | Circulation fan |
|---|---:|---:|
| Refrigerator at/above `Fridge max T` after delay | ON | ON |
| Refrigerator between MIN and MAX | Normally OFF | Normally OFF |
| Refrigerator at/below `Fridge min T` after delay | OFF after minimum runtime | ON |
| Valid freezer at/above `Freez T lockout` | Immediately OFF | Controlled normally |
| Refrigerator probe failed, GET-HOME OFF | OFF | No automatic thermostat control |
| Refrigerator probe failed, GET-HOME selected | Timed duty cycle | No normal thermostat control |
| Refrigerator probe becomes healthy again | Automatic control resumes | Automatic control resumes |

---

## Development and testing

Run the host-side controller tests and failure-mode simulations without ESP32 hardware:

```sh
./tools/run-native-tests.sh
```

GitHub Actions runs:

- Native controller/failure-mode tests.
- Full PlatformIO firmware build for `firebeetle2_esp32e_n16r2`.

The test suite includes fan timing, GET-HOME timing, sensor freshness/recovery, failure simulations, and `millis()` rollover coverage.

---

## Enclosure

A 3D-printable enclosure is available on Thingiverse:

https://www.thingiverse.com/thing:7381757

---

## Future ideas

- PWM/variable-speed fan control based on temperature delta and freezer condition.
- Humidity sensing to help track evaporator icing/defrost requirements.
- Ambient-light sensing for automatic OLED brightness control.
- Door-open sensing and alarm integration.
