# MQTT security scope

The initial Cerbo GX integration targets standard MQTT/TCP with:

- configurable broker host/IP
- configurable port (default 1883)
- optional username
- optional password

TLS/8883 is intentionally not implemented in the first draft publisher. The settings/API are kept separate enough that TLS can be added later without changing the MQTT topic contract or Node-RED flow.

The firmware must not silently downgrade a configured TLS connection; when TLS support is added it should be an explicit mode.
