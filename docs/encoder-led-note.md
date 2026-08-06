# SEN0502 encoder LEDs

The SEN0502 ring is a monochrome position indicator driven by the encoder
count; it is not an RGB status ring and has no separate off control in the
DFRobot API. Firmware uses the minimum gain of 1 so the ring remains effectively
inactive while each encoder detent produces one predictable input count. The
bounded hardware counter is reset to a small neutral value after movement,
preventing either rotation direction from reaching an endpoint without moving
the counter far enough to illuminate the ring.
