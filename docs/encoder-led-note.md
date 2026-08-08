# SEN0502 encoder LEDs

The SEN0502 ring is a monochrome position indicator driven by the encoder's
internal count; it is not an RGB status ring and the DFRobot API does not expose
independent LED control.

Firmware therefore uses two deliberate modes:

- **Navigation/home:** gain 1 with the counter repeatedly returned to a small
  neutral value. The ring remains effectively inactive while clockwise and
  counterclockwise movement both retain count headroom.
- **Numeric/list editing:** gain 51 with the counter mapped to the currently
  edited setting. The ring acts as a value gauge: minimum values illuminate
  very little of the ring, mid-range values roughly half, and maximum values
  nearly the full ring.

After every programmatic `setEncoderValue()` operation the firmware re-baselines
input handling so an LED/gauge update cannot be mistaken for user rotation.
Runtime I2C health is checked periodically; if the encoder disappears, edit
state is safely abandoned/rolled back and the encoder is reinitialized
automatically when it reconnects.
