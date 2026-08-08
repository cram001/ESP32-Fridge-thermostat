# ESP32 fridge temperatures via Cerbo GX Node-RED

The hardware-tested architecture is:

```text
ESP32 -> MQTT -> Cerbo GX broker -> Node-RED -> Victron Virtual Temperature Sensors -> VRM
```

For each temperature, build:

```text
MQTT IN -> validation Function -> Victron Virtual Temperature Sensor
```

## MQTT topics

| Sensor | Topic |
|---|---|
| Fridge | `marinefridge/fridge/temperature` |
| Freezer | `marinefridge/freezer/temperature` |
| Cabin Ambient | `marinefridge/ambient/temperature` |

Payloads are retained plain decimal Celsius values.

## Cerbo GX prerequisites

- Venus OS Large / Node-RED enabled.
- MQTT Access enabled so the ESP32 can connect to the Cerbo broker over the LAN.
- ESP32 configured with the Cerbo GX host/IP, MQTT port, optional credentials, and a non-OFF reporting interval.

The current ESP32 firmware supports plain MQTT/TCP, normally port `1883`; TLS/8883 is not implemented yet.

## Validation Function

Use a Function node after each MQTT-in node:

```javascript
const value = Number(msg.payload);
if (!Number.isFinite(value) || value < -55 || value > 85) {
    return null;
}
msg.payload = { Temperature: value };
return msg;
```

This converts the scalar MQTT payload to the object expected by the Victron virtual-temperature node.

## Victron virtual temperature devices

Create the three **Victron Energy Virtual Device** nodes manually in the Cerbo Node-RED editor.

Use:

- **Fridge** — Device: `Temperature sensor`; Temperature type: `Fridge`
- **Freezer** — Device: `Temperature sensor`; Temperature type: `Freezer`
- **Cabin Ambient** — Device: `Temperature sensor`; Temperature type: `Room`

Wire each validation Function to its matching virtual device.

### Important hardware-tested note

Do **not** rely on a hand-authored imported `victron-virtual` node definition as the canonical setup.

During testing an imported virtual-device definition reported:

```text
Virtual device setup failed
```

Deleting that node and creating a fresh Victron Energy Virtual Device node through the installed Cerbo palette fixed the issue.

## Recommended commissioning sequence

1. Create the three virtual temperature devices manually.
2. Add temporary Inject nodes with known numeric values.
3. Confirm the virtual devices update and the values appear in VRM.
4. Add the validation Function nodes.
5. Add the three MQTT-in nodes using the exact topics above.
6. Enable ESP32 MQTT publishing.
7. Confirm the OLED MQTT icon shows connected.
8. Verify live fridge/freezer/ambient temperatures in VRM.

Known-good manual test values used during hardware verification included:

- Fridge: `2.3`
- Freezer: `-10.3`
- Cabin Ambient: `28.8`

The manual-inject test successfully produced virtual temperatures visible in VRM, proving the Node-RED -> Victron virtual-device -> VRM path.

## Troubleshooting

- **MQTT-in node receives nothing:** check exact topic spelling, ESP32 Wi-Fi, Cerbo host/IP, MQTT Access, port, credentials, and OLED MQTT connection status.
- **Function node receives data but virtual device does not update:** inspect `msg.payload`; it must be an object like `{ Temperature: 2.3 }`.
- **`Virtual device setup failed`:** delete the imported virtual-device node and recreate it manually from the installed Victron palette.
- **Manual Inject works but ESP32 does not:** the Victron/VRM path is proven; troubleshoot MQTT between ESP32 and Cerbo.

See [`../docs/CERBO_MQTT.md`](../docs/CERBO_MQTT.md) for the complete end-to-end commissioning guide.
