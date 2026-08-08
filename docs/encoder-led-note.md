# SEN0502 encoder LEDs

The SEN0502 ring is tied to the encoder module's internal count and the DFRobot
API does not provide independent LED control. Attempts to use that count as a
setting-value gauge caused inconsistent detent handling and count/readback races
on the physical hardware.

Firmware therefore **does not use the LED ring as a setting gauge**. Menu
navigation and value editing both use the same stable gain-1 encoder behavior,
with the count returned to a small neutral value after movement. The ring is
left effectively inactive.

Counterclockwise movement on this hardware produces two transitions per physical
detent; firmware retains the proven compensation that collapses those two
transitions into one menu/edit step. Runtime I2C health is checked periodically;
if the encoder disappears, an in-progress edit is rolled back and the encoder is
reinitialized automatically when it reconnects.
