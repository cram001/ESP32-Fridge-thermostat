# Cerbo MQTT implementation status

The Cerbo GX MQTT application integration is implemented on this branch and remains in draft PR status pending hardware validation.

Implemented:

- retained Celsius publishing for fridge/freezer/ambient;
- persisted Cerbo host/IP, port, optional username/password, and reporting interval;
- SensESP web-UI configuration;
- OLED `Cerbo MQTT` menu item with OFF / 30 sec / 1 min / 2 min / 5 min / 10 min;
- main-loop publisher servicing with calibrated role temperatures;
- bounded reconnect backoff and immediate publish after reconnect;
- home-screen MQTT connected/disconnected icon beside Signal K;
- native MQTT interval tests and PlatformIO CI;
- end-to-end Cerbo/Node-RED/VRM documentation.

The current security implementation is plain MQTT/TCP; TLS/8883 is not yet supported.

The remaining acceptance step is hardware testing of the ESP32 MQTT connection against the actual Cerbo GX installation, including Wi-Fi stability and reconnect behavior.
