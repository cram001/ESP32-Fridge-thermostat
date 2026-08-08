# Cerbo GX MQTT data flow

```text
ESP32
  |
  | retained Celsius MQTT values
  v
Cerbo GX MQTT broker
  |
  v
Node-RED MQTT-in nodes
  |
  | validate + convert to { Temperature: value }
  v
Victron Virtual Temperature Sensors
  |
  v
Venus OS / VRM
```

Topics:

- `marinefridge/fridge/temperature`
- `marinefridge/freezer/temperature`
- `marinefridge/ambient/temperature`

Signal K is not part of this path and may be disabled independently.

See [CERBO_MQTT.md](CERBO_MQTT.md) for complete commissioning instructions.
