# Periodic circulation mixing

The refrigerator circulation fan can run periodically even when the thermostat is not otherwise requesting circulation. This helps reduce top-to-bottom temperature stratification inside the refrigerator compartment.

## Setting

The OLED menu and SensESP web UI expose **Circ mix interval** with these choices:

- OFF
- 6 min
- 10 min
- 15 min
- 20 min
- 30 min
- 45 min
- 60 min

The default is **OFF** so existing installations retain their previous behavior after upgrading.

## Behavior

When enabled, the selected interval is the amount of time the circulation fan may remain idle before a periodic mixing cycle starts.

A periodic mixing cycle:

- runs the **circulation fan only**;
- never starts the spillover fan;
- runs for a fixed **3 minutes**;
- restarts the idle interval when it stops.

Any normal thermostat-driven circulation run satisfies the mixing requirement and restarts the idle interval. If normal circulation demand begins during a periodic mixing run, the fan remains running and normal controller logic takes ownership without cycling the fan off and on.

Periodic mixing is inhibited when:

- `Circ mix interval = OFF`;
- `Override Fans = All fans off`;
- the refrigerator probe is failed/missing/invalid.

Turning the interval to OFF while a timer-owned periodic mixing run is active stops that periodic run immediately unless normal thermostat logic is independently requesting circulation.

## Commissioning test

For a quick hardware test, temporarily set `Circ mix interval = 6 min` while the refrigerator temperature is between the normal MIN/MAX thresholds and spillover is OFF.

Expected sequence:

1. circulation remains OFF for 6 minutes;
2. circulation runs for 3 minutes;
3. circulation stops;
4. after another 6 minutes of circulation idle time, the cycle repeats.

After verifying operation, select the desired interval. A practical starting point for a compartment showing significant vertical temperature stratification is **10–15 minutes** and then adjust based on measured top-to-bottom temperature spread.
