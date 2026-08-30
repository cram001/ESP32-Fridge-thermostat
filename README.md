# ESP32 Marine Spillover Fridge Controller

A standalone ESP32 thermostat/controller for a two-fan marine spillover refrigerator, with local OLED/rotary control plus optional Signal K and direct Victron Cerbo GX integration.

The controller is designed to run unattended 24/7 for long periods. It monitors refrigerator, freezer, and ambient temperatures; controls the spillover and circulation fans; provides alarms and a sensor-failure fallback mode; and automatically recovers from supported sensor and encoder disconnect/reconnect events without requiring a reboot.

**Current beta:** `v0.13.4-beta.1`

> `v1.0.0` is reserved for the first stable release.

For the complete operating and menu reference, see [Device setup and menu guide](docs/USER_GUIDE.md). For Cerbo GX / MQTT / Node-RED commissioning, see [Cerbo GX MQTT integration](docs/CERBO_MQTT.md). For fault behavior and simulation coverage, see [Failure-mode analysis and simulation](docs/FAILURE_MODE_ANALYSIS.md).

---

## Possible Migrations

It would be relatively simple to either migrate the code or replicate the logic on a Shelly Device (Shelly Plus Uni or similar). The Shelly Plus Uni is a self contained ESP32 with ?? MB RAM. It has all the required IO and two build-in relays and could be implemented much simpler (physically) and cheaper (less components) as it could be built without a display and rotary encoder. This would allow the Shelly to be located anywhere, even out of sight, so long as access to the button is maintained. The configuration would be done via a web interface (or the shelly app). The button access is retained in case the built-in access point must be re-enabled.

The disadvantage of such an implementation is there would not be a display near the fridge/freezer and the users would need to know how to access the web UI. They also would need to remember where this box is in case it's located out of sigh! (near the compressor would be best for ease of maintennace).

Another possible upgrade would be to use Ruuvi Pro 2-in-1 tags for temp sensing.  These are wireless and with the right battery, resist the cold well. This would simplify installation as it reduces the number of wires. The 2-in-1 tags are aprox USD 50 each.



## Features

### Refrigeration control

- Two-fan spillover refrigerator control:
  - **Spillover fan** moves cold air from the freezer into the refrigerator.
  - **Circulation fan** circulates air within the refrigerator compartment.
- Configurable refrigerator ON and OFF temperature thresholds.
- Configurable fan start delay to avoid reacting to short temperature changes.
- Independent minimum run times for spillover and circulation fans.
- Optional freezer-temperature lockout that immediately blocks spillover when the freezer is too warm.
- Persistent **Override Fans** mode for freezer-only operation; `All fans off` forces both fan outputs OFF until the operator returns the setting to `Normal operation`.
- Refrigerator, freezer, and ambient temperature calibration.
- Celsius or Fahrenheit OLED display.
- Six-sample rolling average for displayed/control temperatures, while freezer lockout uses the latest valid freezer sample for faster protective action.

### Temperature sensors

- Up to three DS18B20 roles: Refrigerator, Freezer, Ambient/cabin.
- Sensors are assigned by their unique 64-bit ROM address, **not by OneWire bus order**.
- Refrigerator probe is required for automatic thermostat operation.
- Freezer probe is optional; refrigerator control continues without it, but freezer lockout is unavailable.
- Ambient probe is optional.
- Direct OneWire ROM rediscovery every 5 seconds.
- New/reconnected sensors normally appear within approximately 5–10 seconds without rebooting.
- Sensor validity checks include ROM CRC, failed/disconnected reads, freshness, and rejection of the DS18B20 `+85 C` power-on/reset value.
- A sensor is declared failed after three consecutive failed 5-second reads or approximately 15 seconds without a valid sample.
- A single later valid sample automatically restores that sensor role.

### GET-HOME fallback mode

If the refrigerator probe fails, normal thermostat control is disabled and the spillover fan defaults OFF. The user can manually select a temporary spillover duty cycle of OFF, 5, 10, 20, 30, or 40 minutes/hour.

When the refrigerator sensor becomes healthy again, **GET-HOME automatically exits and normal automatic thermostat control resumes**.

`Override Fans = All fans off` has higher priority than GET-HOME and keeps both fan outputs physically OFF. Temperature monitoring and alarms continue.

### Local user interface

- 2.42-inch 128x64 SSD1309 SPI OLED.
- DFRobot SEN0502 I2C rotary encoder with pushbutton.
- **25-item** local settings/service menu.
- Transactional editing: changes are previewed in RAM and only committed when the encoder is pressed again.
- Timed-out, interrupted, or aborted edits restore the previous value instead of saving partial changes.
- `SAVED` confirmation after committed changes.
- `Override Fans`: `Normal operation` / `All fans off`; the selection persists across reboot/power loss.
- User-selectable display layout: `FRDG | FRZ` or `FRZ | FRDG`.
- Fan indications follow the physical compartment layout: **CIRC** with **FRDG**, **SPILL** with **FRZ**.
- Animated fan symbols indicate active outputs; forced-off mode shows a `FANS OFF` banner and slashed static fan symbols for both outputs.
- Configurable OLED contrast and display timeout.
- Pixel shifting reduces OLED burn-in risk.
- `About` screen shows firmware version and build date.
- Top status row independently shows Signal K and Cerbo/MQTT connectivity.

### Signal K and Victron/Cerbo GX networking

The two integrations are independent. Neither is required for local thermostat operation.

#### Signal K

- Built on SensESP.
- Wi-Fi configuration through the SensESP web interface.
- Publishes temperatures and controller state to Signal K.
- Signal K can be used by dashboards, Node-RED, logging systems, and other marine integrations.

#### Direct Cerbo GX / VRM integration

The controller can send its three temperatures directly to a Victron Cerbo GX without requiring Signal K:

```text
ESP32 -> MQTT -> Cerbo GX -> Node-RED -> Victron Virtual Temperature Sensors -> VRM
```

The ESP32 publishes retained Celsius values on:

- `marinefridge/fridge/temperature`
- `marinefridge/freezer/temperature`
- `marinefridge/ambient/temperature`

Cerbo MQTT settings are configured through the SensESP web UI:

- Cerbo GX host/IP
- MQTT port, default `1883`
- Optional username
- Optional password
- Reporting interval

The OLED menu also provides the reporting interval choices:

- OFF
- 30 sec
- 1 min
- 2 min
- 5 min
- 10 min

`OFF` completely disables the Cerbo MQTT connection and removes the MQTT status icon from the home screen. When enabled, the compact broker icon is shown beside the Signal K indicator; a slash through the icon means MQTT is configured but not connected. The ambient-temperature font and right-aligned display position are unchanged.

See [docs/CERBO_MQTT.md](docs/CERBO_MQTT.md) for the complete Cerbo GX and Node-RED commissioning procedure. The Node-RED-specific notes are also available in [Node-Red/README-MQTT.md](Node-Red/README-MQTT.md).

### Long-uptime reliability

The project is deliberately optimized for unattended embedded operation rather than only short bench tests.

- Periodic temperature-sensor rediscovery and automatic recovery.
- Periodic encoder I2C health checks and automatic reinitialization.
- Vendored DFRobot SEN0502 driver with retry/bus-recovery handling.
- Last valid encoder count is preserved across transient failed reads to avoid false large input jumps.
- ESP32 task watchdog protection.
- Unsigned elapsed-time logic for normal operation through `millis()` rollover.
- Hot/periodic code paths avoid repeated Arduino `String` allocations where practical.
- MQTT publish path uses fixed buffers and bounded reconnect backoff instead of a tight reconnect loop.
- MQTT publishes immediately after a successful reconnect and does not publish invalid/non-finite temperatures.
- Platform and major library versions are pinned for reproducible builds.
- Native controller/failure-mode tests plus a full PlatformIO firmware build run in GitHub Actions.

---

## Hardware requirements

### Controller and interface

| Qty | Hardware | Notes |
|---:|---|---|
| 1 | **DFRobot FireBeetle 2 ESP32-E N16R2 (DFR1139)** | 16 MB flash version used for development/testing |
| 1 | **DFRobot DFR0923 FireBeetle 2 Terminal Block Expansion Board** | Recommended; labelled terminals, 7–24 V input |
| 1 | **DFRobot SEN0502 Gravity Visual Rotary Encoder** | I2C, default address `0x54` |
| 1 | **2.42-inch SSD1309 128x64 OLED** | 4-wire SPI |
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

**Do not power the fans directly from ESP32 GPIO pins.** Use suitable MOSFET/relay modules with appropriate flyback/transient protection.

### Power

- DFR0923 external input: **7–24 VDC**.
- In a typical marine installation the controller can be supplied from the vessel's nominal 12 VDC system through the DFR0923 board.
- Verify polarity, fusing, fan voltage, and fan-driver ratings before connecting vessel power.

---

## Wiring reference

Current firmware assignments using the **DFR0923 terminal labels**:

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

Install the FireBeetle on the DFR0923 board, connect the OLED/encoder/buzzer/OneWire bus, add the 4.7 kΩ pull-up, and connect the two GPIO outputs to external fan drivers. Verify the spillover fan moves freezer air toward the refrigerator and the circulation fan circulates air inside the refrigerator.

### 2. Build and upload the firmware

This is a PlatformIO project. Select environment `firebeetle2_esp32e_n16r2`, build, connect the FireBeetle by USB, and upload. Confirm the expected firmware version on the OLED splash screen.

### 3. Verify the startup hardware test

At boot the splash screen remains visible for approximately **15 seconds** and both fan outputs run in `Normal operation`. If the persisted `Override Fans` setting is `All fans off`, the startup fan test is deliberately suppressed and the splash screen shows `FANS OFF`.

### 4. Configure Wi-Fi and optional integrations

Use the SensESP web configuration interface to configure Wi-Fi. On a new/unconfigured controller, connect to the controller's temporary setup access point:

- Wi-Fi AP / SSID: `fridge-controller`
- Default AP password: `thisisfine`
- Configuration page: `http://192.168.4.1`

These credentials are for the temporary SensESP configuration access point used to configure the controller's normal Wi-Fi connection.

- Signal K is optional.
- Cerbo GX MQTT is optional and independent of Signal K.
- For Cerbo GX MQTT, configure the broker host/IP, port and optional credentials in the **Cerbo GX MQTT** section of the SensESP web UI, then select a non-OFF reporting interval either there or from the OLED menu.

For the complete Cerbo GX setup, including Node-RED and VRM verification, follow [docs/CERBO_MQTT.md](docs/CERBO_MQTT.md).

### 5. Assign the temperature probes

Sensor roles are never assigned automatically from OneWire order. Use `Assign fridge`, `Assign freezer`, and `Assign ambient`; warm the desired probe with your hand, select it by live temperature, and press to save its ROM address.

### 6. Configure the thermostat

Review `Fridge max T`, `Fridge min T`, `Freez T lockout`, the two high-temperature alarms, fan delay, minimum run times, GET-HOME setting, buzzer, OLED contrast, display timeout, display layout, Cerbo MQTT reporting interval, and `Override Fans`.

For normal two-compartment operation use `Override Fans = Normal operation`. To use only the freezer and prevent cold-air transfer/circulation in the refrigerator side, select `All fans off`. The override remains active after reboot until manually changed back.

`Fridge min T` must remain at least **0.5 C below** `Fridge max T`.

### 7. Run the service output test

Use `Test outputs` to test `SPILLOVER`, `CIRCULATION`, and `BUZZER`. Each test stops automatically after five seconds and can also be stopped early by pressing the encoder. Fan output tests cannot energize a fan while `Override Fans = All fans off`; return to `Normal operation` before intentionally testing either fan.

### 8. Test the sensor-failure recovery path

Disconnect the refrigerator probe, confirm the fault and GET-HOME behavior, reconnect the bus, and confirm the assigned probes return automatically and normal control resumes. If `All fans off` is selected, GET-HOME remains inhibited by the operator override.

### 9. If using Cerbo GX, verify VRM temperatures

After completing the Node-RED setup, verify that Fridge, Freezer, and Cabin Ambient temperatures appear as Victron virtual temperature sensors locally and in VRM. See [docs/CERBO_MQTT.md](docs/CERBO_MQTT.md).

---

## Normal control summary

| Condition | Spillover fan | Circulation fan |
|---|---:|---:|
| `Override Fans = All fans off` | **FORCED OFF** | **FORCED OFF** |
| Refrigerator at/above `Fridge max T` after delay | ON | ON |
| Refrigerator between MIN and MAX | Normally OFF | Normally OFF |
| Refrigerator at/below `Fridge min T` after delay | OFF after minimum runtime | ON |
| Valid freezer at/above `Freez T lockout` | Immediately OFF | Controlled normally |
| Refrigerator probe failed, GET-HOME OFF | OFF | No automatic thermostat control |
| Refrigerator probe failed, GET-HOME selected | Timed duty cycle | No normal thermostat control |
| Refrigerator probe becomes healthy again | Automatic control resumes | Automatic control resumes |

The override is the highest-priority fan command. It does not disable temperature sensing or alarms.

---

## Development and testing

Run the host-side controller tests and failure-mode simulations without ESP32 hardware:

```sh
./tools/run-native-tests.sh
```

GitHub Actions runs native controller/failure-mode tests and a full PlatformIO firmware build for `firebeetle2_esp32e_n16r2`.

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
- Optional TLS MQTT mode for installations that require port 8883/security profiles that do not permit plain MQTT.
