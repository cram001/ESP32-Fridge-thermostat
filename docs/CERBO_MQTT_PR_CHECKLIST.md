# Cerbo MQTT draft PR checklist

- [x] Add fixed-buffer MQTT publisher.
- [x] Pin PubSubClient dependency.
- [x] Use retained Celsius temperature topics for fridge/freezer/ambient.
- [x] Add bounded reconnect backoff and immediate publish on reconnect.
- [x] Document hardware-tested Node-RED virtual-temperature setup.
- [ ] Persist Cerbo host, port, optional username/password, and interval.
- [ ] Add SensESP Web UI configuration fields.
- [ ] Add physical menu setting: OFF / 30 sec / 1 min / 2 min / 5 min / 10 min.
- [ ] Wire publisher into the main loop.
- [ ] Add Cerbo/MQTT home-screen connection icon next to Signal K.
- [ ] Adjust top-row layout for two connectivity indicators.
- [ ] Add/convert native timing tests.
- [ ] Firmware build CI green on final head.
- [ ] Long-uptime reliability and stale-artifact sweep complete.
