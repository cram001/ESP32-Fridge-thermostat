# Cerbo GX MQTT temperature integration

This integration is intended for installations that want Fridge Controller temperatures in Venus OS / VRM without requiring a Signal K server.

## Architecture

```text
ESP32 -> MQTT -> Cerbo GX broker -> Node-RED -> Victron virtual temperature devices -> VRM
```

The ESP32 publishes Celsius values to these retained MQTT topics:

- `marinefridge/fridge/temperature`
- `marinefridge/freezer/temperature`
- `marinefridge/ambient/temperature`

The Node-RED flow converts each scalar MQTT payload to the `{ "Temperature": value }` object expected by a Victron Virtual Temperature Sensor.

## Cerbo GX prerequisites

- Venus OS Large / Node-RED enabled.
- MQTT Access enabled under Venus OS integrations.
- For the first implementation, the ESP32 uses standard MQTT over TCP. The broker host, port, optional username, and optional password will be configurable through the SensESP web UI.

## Reporting interval

The physical controller menu will expose:

- OFF
- 30 sec
- 1 min
- 2 min
- 5 min
- 10 min

`OFF` disables the Cerbo MQTT connection and removes the MQTT status icon from the normal display.

## Reliability requirements

- No `String` allocation in the periodic MQTT service/publish path.
- Unique fixed client ID derived from the ESP32 MAC/eFuse ID.
- Bounded reconnect backoff (5 s to 60 s), rather than a tight retry loop.
- Immediate publish after successful reconnect.
- Retained temperature messages so a Node-RED restart receives the current reading immediately.
- Invalid / non-finite sensor values are not published as temperatures.
- `millis()` interval arithmetic is rollover-safe.
- Signal K and Cerbo MQTT remain independent integrations; failure of either must not interfere with the thermostat control loop.

## Draft-PR implementation status

The MQTT transport/publisher is implemented in `CerboMqttPublisher` and the PubSubClient dependency is pinned. The remaining draft work is to wire it into the existing application settings/UI:

1. Persist MQTT host, port, optional username/password, and reporting interval.
2. Expose broker connection fields in the SensESP web UI.
3. Add the OFF/30 s/1/2/5/10 min reporting interval to the physical rotary menu.
4. Service the publisher from the main loop using the three calibrated role temperatures.
5. Add a Cerbo/MQTT status icon beside the existing Signal K icon and adjust the top-row layout.
6. Update the supplied Node-RED flow/instructions based on the hardware-tested virtual-device setup.
7. Run firmware CI and a final long-uptime/stale-artifact review before the PR is made ready for review.
