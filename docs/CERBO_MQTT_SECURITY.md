# MQTT security scope

The current Cerbo GX integration uses standard MQTT/TCP with:

- configurable broker host/IP
- configurable port, default `1883`
- optional username
- optional password

## Current limitation

TLS/8883 is **not implemented** in the current ESP32 publisher.

Therefore:

- use a Cerbo/Venus OS network security configuration that permits plain MQTT/TCP for this firmware;
- do not expect a TLS-only MQTT configuration to work by changing the port to `8883`;
- the firmware does not silently downgrade a TLS configuration because no TLS mode is exposed yet.

MQTT should be used only on a trusted vessel/local network appropriate for plain MQTT traffic.

A future TLS implementation should be an explicit security mode and should preserve the existing MQTT topic contract and Node-RED flow.
