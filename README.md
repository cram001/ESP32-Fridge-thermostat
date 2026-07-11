# ESP32 marine spillover fridge controller

SensESP firmware for a two-fan marine spillover refrigerator. It reads fridge,
freezer, and cabin DS18B20 sensors; controls spillover and internal circulation
fans; displays status on a 128x64 SSD1309 OLED; and publishes readings and state
to Signal K.

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
so reconnecting or replacing wiring cannot exchange fridge and freezer roles.

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
1. HiLetgo 2.42" SSD1309 128x64 OLED Display Module 2.42 Inch (SPI connection)
2. Enclosure (can be 3D printed) ... see thingaverse 
