# Cerbo GX / Node-RED integration

The authoritative setup and troubleshooting guide for the ESP32 fridge controller's Cerbo GX MQTT integration is:

[`../docs/CERBO_MQTT.md`](../docs/CERBO_MQTT.md)

That guide covers:

- Cerbo GX / Venus OS prerequisites;
- MQTT broker settings on the ESP32;
- the three temperature topics;
- Node-RED MQTT-in and validation nodes;
- manual creation of Victron Virtual Temperature Sensor nodes;
- VRM verification;
- reconnect/status behavior;
- current plain-MQTT/TCP security scope and troubleshooting.

A previously stored hand-authored Node-RED flow export was removed because its imported Victron virtual-device definitions were not reliable on the hardware-tested Cerbo GX. The canonical instructions therefore use fresh Victron Virtual Device nodes created from the installed Cerbo Node-RED palette.
