# Cerbo MQTT draft PR checklist

- [x] Add fixed-buffer MQTT publisher.
- [x] Pin PubSubClient dependency.
- [x] Use retained Celsius temperature topics for fridge/freezer/ambient.
- [x] Add bounded reconnect backoff and immediate publish on reconnect.
- [x] Document hardware-tested Node-RED virtual-temperature setup.
- [x] Persist Cerbo host, port, optional username/password, and interval.
- [x] Add SensESP Web UI configuration fields.
- [x] Add physical menu setting: OFF / 30 sec / 1 min / 2 min / 5 min / 10 min.
- [x] Wire publisher into the main loop.
- [x] Add Cerbo/MQTT home-screen connection icon next to Signal K.
- [x] Adjust top-row layout for two connectivity indicators without reducing the ambient-temperature font.
- [x] Add native reporting-interval tests.
- [x] Firmware build CI green on final head.
- [x] Native controller tests green on final head.
- [x] Long-uptime reliability and stale-artifact sweep complete.

Hardware verification still required before merge:

- Confirm the SensESP Cerbo MQTT settings page renders correctly on the target ESP32 firmware.
- Confirm ESP32 connects to the actual Cerbo GX broker using the installation's configured MQTT access/security settings.
- Confirm fridge, freezer, and ambient values arrive on the three documented MQTT topics and update the existing Node-RED virtual temperature sensors / VRM.
- Confirm MQTT OFF removes the MQTT icon and stops broker reconnect/publish activity.
- Confirm Wi-Fi or Cerbo broker interruption shows the disconnected icon and automatically recovers when service returns.
- Confirm LOCKOUT / GET-HOME / warning / ambient top-row layout remains readable on the physical OLED.
