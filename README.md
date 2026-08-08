# ESP32 marine spillover fridge controller

SensESP firmware for a two-fan marine spillover refrigerator.

Designed to improve refrigerator temperature consistency in a marine spillover fridge setup, where cold air is pulled from a freezer via a fan. A second fan is used in the refrigerator to maintain a consistent temperature and circulate cold air back to the top when needed.

It reads fridge, freezer, and cabin DS18B20 temp sensors; controls spillover and internal circulation fans; displays status on a 128x64 SSD1309 OLED; and publishes readings and state to Signal K (via Wi-Fi).

Total daily power consumption, with OLED always ON, is less than 1 Ah (12VDC system).

For complete installation, first-start, sensor-assignment, control, and menu instructions, see [Device setup and menu guide](docs/USER_GUIDE.md).

For unattended fault behavior and the native sensor/fan/compressor simulations, see [Failure-mode analysis and simulation](docs/FAILURE_MODE_ANALYSIS.md).

## Bring-up

1. Verify every pin and output polarity in `include/hardware_config.h` before connecting fan drivers. Fans require MOSFET/relay drivers and flyback protection; do not power them from ESP32 GPIO pins.
2. Connect DS18B20 sensors to the shared 1-Wire bus with a 4.7 kohm pull-up.
3. Build and upload with PlatformIO, then configure Wi-Fi and Signal K using the SensESP web interface. Signal K is optional, but it can forward temperatures to an NMEA 2000 network, support custom dashboards, and integrate with Node-RED.
4. The 15-second splash screen is also an output test: both fan outputs remain ON while the splash is visible. Confirm that both fans run, then confirm that normal temperature control takes over when the countdown ends.
5. Assign each sensor explicitly. Warm one probe by hand and identify its live temperature under `environment.inside.refrigerator.detectedProbeN` in the Signal K data browser, or on the OLED assignment screen. On the OLED, choose `Assign fridge`, `Assign freezer`, or `Assign ambient`, rotate to the matching probe, and press to save its ROM.
6. Press the rotary button from the home screen to open settings at item 1. Rotate in either direction to browse, press to edit, rotate to preview a new value, and press again to commit it. A committed change briefly shows `SAVED`.

Sensor roles are never assigned automatically by OneWire bus order. They follow the saved 64-bit ROM, so reconnecting wiring cannot exchange fridge and freezer roles. The controller performs a direct OneWire ROM search every five seconds and requires two matching scans before accepting a changed device list. A replacement or reconnected DS18B20 is therefore normally detected within about 5–10 seconds without rebooting. Use the appropriate `Assign ...` item to bind a replacement ROM to a role. Saving the new selection overwrites the old ROM for that role. If that physical sensor was assigned elsewhere, the old duplicate assignment is cleared automatically.

The firmware version shown on the startup screen is the authoritative way to confirm which build is running. The final `About` menu item also shows the firmware version, compile/build date, and author/copyright information. `v1.0.0` remains reserved for the first stable release; development builds may increment minor or patch versions as behavior changes.

The freezer probe is optional for control. If it is missing, the fridge probe continues to control the spillover fan. If the freezer probe is present and its valid temperature is at or above the configured lockout, the spillover fan remains off. This avoids deliberately adding more heat load to a freezer that is already recovering or unable to maintain temperature.

The home-screen top row has fixed regions for Signal K status, a `LOCKOUT` / `GET-HOME` operational banner, the warning triangle, and ambient temperature. If both LOCKOUT and GET-HOME are active, the banner alternates so neither state is hidden.

## Fridge temperature control

The displayed fridge, freezer, and ambient temperatures are rolling averages of six good readings sampled about every five seconds. Normal fan control uses the filtered fridge temperature. Freezer lockout uses the latest valid freezer reading so it can stop spillover without waiting for the average.

- At `Fridge max T`, a persistent warm condition starts spillover after the fan trigger delay. Circulation starts with it.
- At `Fridge min T`, spillover stops only after its configured minimum runtime has elapsed. Reaching the freezer lockout is the temperature exception and stops spillover immediately.
- At or below `Fridge min T`, circulation can also start independently after the fan trigger delay. Its own minimum runtime is enforced once it starts.
- `Fridge min T` must remain at least 0.5 C below `Fridge max T`.

## DS18B20 input reliability

Sensor validity is based on communication integrity and freshness, not on whether the temperature number changes. Discovery validates the sensor ROM CRC and DS18B20 family. DallasTemperature validates the scratchpad CRC on each temperature read. The firmware rejects disconnected/failed reads and the DS18B20 +85 C power-on/reset value.

Two transient failed five-second reads are tolerated using the most recent known-good value. A third failed read, or about 15 seconds without a good sample, marks that assigned role as read-failed and removes it from control. One later good sample recovers the role automatically. A cabin, refrigerator, or freezer may legitimately remain on exactly the same 10-bit reading for hours; an unchanged numeric value is not treated as a fault.

## Fridge-probe failure / get-me-home mode

A missing, read-failed, or invalid fridge probe raises a persistent alarm and disables normal thermostat control. The spillover fan remains OFF by default. The user may select an explicit get-me-home duty cycle of 5, 10, 20, 30, or 40 minutes ON per hour. Changing the setting starts a new ON interval immediately. Select OFF when the temporary mode is no longer required.

Pressing the encoder acknowledges an active alarm. This stops the buzzer and full-screen visual alert, but the alarm remains active in Signal K until its underlying condition clears.

## Development checks

Run the host-side controller tests and unattended failure-mode simulations without ESP32 hardware:

```sh
./tools/run-native-tests.sh
```

The same test command runs automatically in GitHub Actions for pushes and pull requests. The suite includes controller timing/rollover tests, failure-mode simulations, and sensor freshness/recovery tests.

## Hardware

From DFRobot:
1. DF Robot FireBeetle 2 ESP32-E (N16R2) IoT Board (Dual-Core 240MHz, WiFi/Bluetooth, LVGL Support) 16 MB
2. Gravity: Digital Buzzer for Arduino / ESP32 / micro:bit / Raspberry Pi
3. Gravity: MOSFET Power Controller (x2)
4. Waterproof DS18B20 Digital Temperature Sensor for Arduino (IP68, -10°C to +85°C) (x2)
5. Gravity: Waterproof DS18B20 Temperature Sensor Kit (x1)
6. Gravity: 360 Degree Rotary Encoder Module
7. Terminal Block Board for FireBeetle 2 ESP32-E IoT Microcontroller DFR0923 (optional, it includes a DC step down converter)
8. Wires, cables, etc.

From other sources:
1. HiLetgo 2.42" SSD1309 128x64 OLED Display Module 2.42 Inch (SPI connection) (white is preferred for a marine environment)
2. 3D printed enclosure — see Thingiverse: https://www.thingiverse.com/thing:7381757

## Future implementations

1. Optional PWM fan instead of on/off fan, allowing variable fan speed based on temperature delta and freezer temperature.
2. Humidity sensor to help minimize evaporator icing and recommend defrost cycles.
3. Ambient light sensor to automatically reduce display brightness.
4. Door-open sensor linked to the alarm state if the door is left open or not fully latched.
