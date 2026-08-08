# MQTT topic contract

All temperature payloads are plain decimal **Celsius** values, regardless of the OLED display Units setting.

| Topic | Meaning |
|---|---|
| `marinefridge/fridge/temperature` | Calibrated fridge temperature |
| `marinefridge/freezer/temperature` | Calibrated freezer temperature |
| `marinefridge/ambient/temperature` | Calibrated cabin/ambient temperature |

Example payloads:

```text
2.3
-10.3
28.8
```

Behavior:

- Messages are retained.
- Invalid/non-finite sensor values are not published as numeric temperatures.
- Topics are fixed in firmware and are not user-configurable.
- A successful broker reconnect triggers an immediate publish of current valid readings.
- QoS uses PubSubClient's default publish behavior in the current implementation.

Node-RED converts each scalar payload to the Victron virtual-temperature object:

```javascript
msg.payload = { Temperature: Number(msg.payload) };
```

See [CERBO_MQTT.md](CERBO_MQTT.md) for the complete integration procedure.
