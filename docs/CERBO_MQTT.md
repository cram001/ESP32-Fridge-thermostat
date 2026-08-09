# Cerbo GX MQTT temperature integration

This integration sends the ESP32 fridge controller temperatures directly to a Victron Cerbo GX / Venus OS installation without requiring Signal K.

## Architecture

```text
ESP32 -> MQTT -> Cerbo GX broker -> Node-RED -> Victron Virtual Temperature Sensors -> VRM
```

Signal K and Cerbo MQTT are independent integrations. Either can be disabled without affecting the local thermostat, OLED, alarms, or fan control.

## MQTT topic contract

The ESP32 publishes retained calibrated Celsius values on:

- `marinefridge/fridge/temperature`
- `marinefridge/freezer/temperature`
- `marinefridge/ambient/temperature`

Payloads are plain decimal numbers such as `2.3`, `-10.3`, or `28.8`. The ESP32 always publishes Celsius regardless of the OLED Units setting.

Invalid/non-finite temperature values are not published as bogus numbers.

## 1. Prepare the Cerbo GX

The intended path uses supported Venus OS Large / Node-RED functionality only; it does not require custom Venus services or filesystem changes.

On the Cerbo GX:

1. Ensure Venus OS Large features are available/enabled.
2. Enable **Node-RED** under the Venus OS Large / Integrations settings.
3. Enable **MQTT Access** under Integrations so an external LAN client such as the ESP32 may connect to the Cerbo broker.
4. Determine the Cerbo GX LAN IP address or hostname.
5. For the current firmware implementation, use a network/security configuration that permits standard MQTT/TCP. The default ESP32 broker port is `1883`.

A DHCP reservation or otherwise stable Cerbo LAN address is recommended.

### Security limitation

The current ESP32 publisher supports configurable host/IP, port, optional username/password, and **plain MQTT/TCP only**. TLS/8883 is **not yet implemented**.

**Use this integration only on a trusted vessel/local network or trusted VLAN.** MQTT usernames/passwords and published temperature data are not encrypted and may be visible to devices capable of observing LAN traffic. Do not expose the Cerbo MQTT broker directly to an untrusted network or the public Internet.

Do not configure the ESP32 for a Cerbo security profile that requires TLS-only MQTT and expect it to work. Changing the port to `8883` does not enable TLS.

## 2. Configure the ESP32

Open the SensESP web configuration interface and find **Cerbo GX MQTT**.

Configure:

- **Cerbo GX MQTT host / IP** — the Cerbo GX LAN hostname or IP address.
- **MQTT port** — normally `1883` for the currently supported plain MQTT mode.
- **MQTT username (optional)**.
- **MQTT password (optional)**.
- **Cerbo MQTT reporting interval** — OFF / 30 sec / 1 min / 2 min / 5 min / 10 min.

The reporting interval can also be changed locally from the OLED menu item `Cerbo MQTT`.

### OLED MQTT status

- `OFF` — no MQTT icon is shown and the ESP32 does not attempt broker connections.
- Enabled + connected — compact broker/network icon shown beside the Signal K indicator.
- Enabled + disconnected — same icon with a diagonal slash.

The MQTT icon is independent of the Signal K icon. The ambient-temperature font size and right-aligned position are unchanged.

## 3. Open Node-RED on the Cerbo GX

Open the Cerbo GX Node-RED editor using the supported local/VRM Venus OS Large interface for your installation.

The hardware-tested arrangement for each temperature is:

```text
MQTT IN -> validation Function -> Victron Virtual Temperature Sensor
```

Create one path each for:

- Fridge
- Freezer
- Cabin Ambient

## 4. Create the MQTT input nodes

Create the MQTT-in and validation nodes manually using the directions below. Click **Deploy** after modifying the Node-RED flow.

Use the Cerbo's local MQTT broker from Node-RED. A Node-RED flow running on the Cerbo can normally use `127.0.0.1:1883` for the local broker when plain MQTT access is available.

Subscribe to:

| Sensor | Topic |
|---|---|
| Fridge | `marinefridge/fridge/temperature` |
| Freezer | `marinefridge/freezer/temperature` |
| Cabin Ambient | `marinefridge/ambient/temperature` |

The ESP32 publishes retained messages, so a newly deployed/restarted Node-RED flow should receive the latest valid temperature after subscribing.

## 5. Add validation/conversion Function nodes

The MQTT payload is a scalar Celsius value. The Victron Virtual Temperature Sensor expects an object containing `Temperature`.

Use a Function node for each topic that validates the number before forwarding it:

```javascript
const value = Number(msg.payload);
if (!Number.isFinite(value) || value < -55 || value > 85) {
    return null;
}
msg.payload = { Temperature: value };
return msg;
```

This rejects malformed or implausible values and produces the object expected by the Victron virtual device.

## 6. Create the Victron Virtual Temperature Sensors

**Create these nodes manually in the Cerbo Node-RED editor.**

During hardware testing, a hand-authored/imported preconfigured virtual-device definition produced:

```text
Virtual device setup failed
```

Deleting that imported virtual-device node and creating a fresh **Victron Energy Virtual Device** node through the installed Cerbo palette fixed the problem.

Create:

| Name | Device | Temperature type |
|---|---|---|
| Fridge | Temperature sensor | Fridge |
| Freezer | Temperature sensor | Freezer |
| Cabin Ambient | Temperature sensor | Room |

Leave unsupported/unneeded humidity, pressure, battery, or default-value options disabled unless your installed Victron node version explicitly requires otherwise.

Wire each validation Function to its matching virtual temperature sensor.

## 7. Test the Node-RED -> Victron -> VRM path first

Before depending on MQTT, inject known values manually into the validation/virtual-device path.

Example values:

- Fridge: `2.3`
- Freezer: `-10.3`
- Cabin Ambient: `28.8`

The hardware-tested system successfully showed manually injected virtual temperatures in VRM. This proves the Node-RED -> Victron Virtual Temperature Sensor -> VRM path independently of the ESP32 MQTT connection.

## 8. Enable ESP32 MQTT publishing

After the Node-RED path is working:

1. Enter the correct Cerbo host/IP and port in the ESP32 SensESP web UI.
2. Add username/password only if your Cerbo MQTT configuration requires them.
3. Select a reporting interval other than `OFF`.
4. Save the configuration.
5. Watch the OLED MQTT icon.

When the ESP32 connects successfully, the slash disappears from the MQTT icon and the controller immediately publishes the current valid temperatures instead of waiting for the next full reporting interval.

## 9. Verify MQTT values in Node-RED and VRM

Confirm, in order:

1. MQTT-in nodes receive the three numeric Celsius payloads.
2. Function nodes output `{ Temperature: value }`.
3. Victron virtual temperature devices update locally.
4. Fridge, Freezer, and Cabin Ambient values appear in VRM.

If the Node-RED path works with manual Inject nodes but not from the ESP32, troubleshoot the MQTT connection rather than the Victron virtual-device configuration.

## Reporting interval

Available intervals:

- OFF
- 30 sec
- 1 min
- 2 min
- 5 min
- 10 min

`OFF` disables the MQTT connection and publishing completely.

## Connection/recovery behavior

The publisher is designed for unattended operation:

- no Arduino `String` allocation in the periodic publish path;
- unique fixed client ID derived from the ESP32 MAC/eFuse ID;
- bounded reconnect backoff rather than a tight retry loop;
- immediate publish after successful reconnect;
- retained temperature messages;
- invalid/non-finite values skipped;
- rollover-safe interval timing;
- MQTT and Signal K failures remain independent of thermostat control.

## Troubleshooting

### MQTT icon not shown

The reporting interval is `OFF`. Select a non-OFF interval.

### MQTT icon is slashed

MQTT is enabled but not connected. Check:

- ESP32 Wi-Fi connectivity
- Cerbo GX host/IP
- port
- MQTT Access enabled on the Cerbo
- username/password if required
- whether the Cerbo security profile permits plain MQTT/TCP

### Node-RED receives no messages

Confirm the exact topic spelling and ensure the ESP32 MQTT icon shows connected.

### Node-RED receives values but virtual temperature does not update

Inspect the Function output and verify it is an object such as:

```json
{"Temperature":2.3}
```

Then recreate the Victron Virtual Temperature Sensor manually if the imported node reports `Virtual device setup failed`.

### VRM does not show the temperature

First verify the virtual temperature sensor updates locally on the Cerbo. If manual Inject values reach the virtual device but do not appear in VRM, the problem is downstream of MQTT and should be diagnosed as a Victron/VRM issue.

## Related files

- [USER_GUIDE.md](USER_GUIDE.md) — local menu and ESP32 configuration reference.
- [../Node-Red/README-MQTT.md](../Node-Red/README-MQTT.md) — pointer to this canonical guide and explanation of the removed legacy flow export.
