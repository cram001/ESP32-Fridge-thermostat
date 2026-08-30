# OTA follow-up test

This branch adds ESP32-side OTA receive hardening after PR #25 still failed near 90%.

- ArduinoOTA receive timeout increased to 10 seconds.
- Wi-Fi modem sleep disabled only while an OTA transfer is active.
- Existing OTA-exclusive application pause remains in place.

Remove this note after validation if desired.
