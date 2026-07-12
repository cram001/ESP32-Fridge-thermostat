# ESP32 marine spillover fridge controller

SensESP firmware for a two-fan marine spillover refrigerator. 

Designed to improve refrigerator temperature consistency in a marine spillover fridge setup, where cold air is pulled from a freezer via  fan. A second fan is used in the refrigerator to maintain a consistent temperature and circulate cold air back to the top when needed.

It reads fridge,freezer, and cabin DS18B20 temp sensors; controls spillover and internal circulation
fans; displays status on a 128x64 SSD1309 OLED; and publishes readings and state to Signal K (via wifi).

Total daily power consumption, with OLED always ON is less than 1 Ah (12VDC system).

## Bring-up

1. Verify every pin and output polarity in `include/hardware_config.h` before
   connecting fan drivers. Fans require MOSFET/relay drivers and flyback
   protection; do not power them from ESP32 GPIO pins.
2. Connect DS18B20 sensors to the shared 1-Wire bus with a 4.7 kohm pull-up.
   The temporary role order is fridge, freezer, ambient by bus index.
3. Build and upload with PlatformIO, then configure Wi-Fi and Signal K using the
   SensESP web interface.
4. Press the rotary button to select high, low, or freezer-lockout temperature;
   rotate in 0.5 C steps.

Before permanent installation, configure sensors by their unique ROM addresses
so reconnecting or replacing wiring cannot exchange fridge and freezer roles. Changing a DS18B20 sensor requires a reboot (power cycle). 

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
   
