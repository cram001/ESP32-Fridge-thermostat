# ESP32 marine spillover fridge controller

SensESP firmware for a two-fan marine spillover refrigerator. 

Designed to improve refrigerator temperature consistency in a marine spillover fridge setup, where cold air is pulled from a freezer via  fan. A second fan is used in the refrigerator to maintain a consistent temperature and circulate cold air back to the top when needed.

It reads fridge,freezer, and cabin DS18B20 temp sensors; controls spillover and internal circulation
fans; displays status on a 128x64 SSD1309 OLED; and publishes readings and state to Signal K (via wifi).

Total daily power consumption, with OLED always ON is less than 1 Ah (12VDC system).

For complete installation, first-start, sensor-assignment, control, and menu
instructions, see [Device setup and menu guide](docs/USER_GUIDE.md).

## Bring-up

1. Verify every pin and output polarity in `include/hardware_config.h` before
   connecting fan drivers. Fans require MOSFET/relay drivers and flyback
   protection; do not power them from ESP32 GPIO pins.
2. Connect DS18B20 sensors to the shared 1-Wire bus with a 4.7 kohm pull-up.
3. Build and upload with PlatformIO, then configure Wi-Fi and Signal K using the
   SensESP web interface. Signal K is optional, but it can forward temperatures
   to an NMEA 2000 network, support custom dashboards, and integrate with
   Node-RED.
4. The 15-second splash screen is also an output test: both fan outputs remain
   ON while the splash is visible. Confirm that both fans run, then confirm
   that normal temperature control takes over when the countdown ends.
5. Assign each sensor explicitly. Warm one probe by hand and identify its live
   temperature under `environment.inside.refrigerator.detectedProbeN` in the
   Signal K data browser, or on the OLED assignment screen. On the OLED, choose
   `Assign fridge`, `Assign freezer`, or `Assign ambient`, rotate to the matching
   probe, and press to save its ROM.
6. Press the rotary button from the home screen to open settings at item 1.
   Rotate in either direction to browse, press to edit, rotate to change the
   value, and press again to return to browsing.

Sensor roles are never assigned automatically by OneWire bus order. They follow
the saved 64-bit ROM, so reconnecting wiring cannot exchange fridge and freezer
roles. To replace a sensor, power-cycle so the new ROM is discovered, warm the
new probe to identify it, and use the appropriate `Assign ...` item. Saving the
new selection overwrites the old ROM for that role. If that physical sensor was
assigned elsewhere, the old duplicate assignment is cleared automatically.

The firmware version shown on the startup screen advances once per firmware PR
using pre-release minor versions (`v0.1.0`, `v0.2.0`, ... `v0.10.0`). After
uploading, confirm this value on the splash screen to verify that the expected
revision is running.

The freezer probe is optional for control. If it is missing, the fridge probe
continues to control the spillover fan. If the freezer probe is present and its
valid temperature is at or above the configured lockout, the spillover fan remains
off. (avoids pushing too much warm air into the freezer, potentially thawing food while the compressor/evaporator kick in to remove the additional heat).

## Fridge temperature control

The displayed fridge, freezer, and ambient temperatures are rolling averages of
six readings sampled about every five seconds. Normal fan control uses the
filtered fridge temperature. Freezer lockout uses the latest valid freezer
reading so it can stop spillover without waiting for the average.

- At `Fridge max T`, a persistent warm condition starts spillover after the fan
  trigger delay. Circulation starts with it.
- At `Fridge min T`, spillover stops only after its configured minimum runtime
  has elapsed. Reaching the freezer lockout is the temperature exception and
  stops spillover immediately.
- At or below `Fridge min T`, circulation can also start independently after
  the fan trigger delay. Its own minimum runtime is enforced once it starts.
- `Fridge min T` must remain at least 0.5 C below `Fridge max T`.

## Fridge-probe failure / get-me-home mode

A missing or invalid fridge probe raises a persistent alarm and disables normal
thermostat control. The spillover fan remains OFF by default. The user may select
an explicit get-me-home duty cycle of 5, 10, 20, 30, or 40 minutes ON per hour.
Changing the setting starts a new ON interval immediately. Select OFF when the
temporary mode is no longer required.

Pressing the encoder acknowledges an active alarm. This stops the buzzer and
full-screen visual alert, but the alarm remains active in Signal K until its
underlying condition clears.

## Development checks

Run the host-side controller tests without ESP32 hardware:

```sh
./tools/run-native-tests.sh
```

## Hardware

From DFRobot:
1. DF Roboto Firebeetle 2 ESP32-E (N16R2) IoT Board (Dual-Core 240MHz, WiFi/Bluetooth, LVGL Support) 16 MB
2. Gravity: Digital Buzzer for Arduino / ESP32 / micro:bit / Raspberry Pi
3. Gravity: MOSFET Power Controller (x2)
4. Waterproof DS18B20 Digital Temperature Sensor for Arduino (IP68, -10°C to +85°C) (x2)
5. Gravity: Waterproof DS18B20 Temperature Sensor Kit (x1)
6. Gravity: 360 Degree Rotary Encoder Module
7. Terminal Block Board for FireBeetle 2 ESP32-E IoT Microcontroller	DFR0923 (optional, it includes a DC step down converter)
8. Wires, cables, etc...

From other sources:
1. HiLetgo 2.42" SSD1309 128x64 OLED Display Module 2.42 Inch (SPI connection) (white is prefered for a marine environment)
2. 3D printed enclosure  ... see thingiverse  https://www.thingiverse.com/thing:7381757

## Future implementations:
1. Optional PWM fan (instead of on/off fan) .... would allow to implement PID control and vary fan speed based on temperature delta from desired and based on freezer temp (reduce freezer warming effect when freezer temp is close to high limit)
2. Humidity sensor to help minimize evaporator icing up and recommened defrost cycles
3. Ambient light sensor to automatically reduce display brightness
4. Door open sensor - linked to alarm state in case it's left open or not fully latched closed
5. 
   
