# ESP32 fridge temperatures via Cerbo GX Node-RED

The hardware-tested Node-RED arrangement is:

```text
MQTT IN -> validation Function -> Victron Virtual Temperature Sensor
```

Topics:

- `marinefridge/fridge/temperature`
- `marinefridge/freezer/temperature`
- `marinefridge/ambient/temperature`

The validation Function converts the incoming scalar Celsius payload to:

```javascript
msg.payload = { Temperature: Number(msg.payload) };
return msg;
```

For the current Victron Node-RED version, create the three **Victron Virtual Device** nodes manually in the Cerbo Node-RED editor rather than relying on a fully preconfigured imported virtual-device node. This was hardware-tested after an imported virtual-device definition reported `Virtual device setup failed`.

Use:

- Fridge: `Temperature sensor`, type `Fridge`
- Freezer: `Temperature sensor`, type `Freezer`
- Cabin Ambient: `Temperature sensor`, type `Room`

Connect each validation Function to the corresponding virtual temperature device. The user's hardware test confirmed manually injected temperatures then appear in VRM.
