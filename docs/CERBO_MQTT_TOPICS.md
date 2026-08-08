# MQTT topic contract

All temperature payloads are plain decimal Celsius values.

| Topic | Meaning |
|---|---|
| `marinefridge/fridge/temperature` | Calibrated fridge temperature |
| `marinefridge/freezer/temperature` | Calibrated freezer temperature |
| `marinefridge/ambient/temperature` | Calibrated cabin/ambient temperature |

Messages are retained. QoS remains at the PubSubClient default publish behavior for this first implementation.
