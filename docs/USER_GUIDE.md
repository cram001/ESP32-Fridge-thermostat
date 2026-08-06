# Device setup and menu guide

This guide covers initial setup and normal operation of the ESP32 marine
spillover fridge controller.

## Before applying power

- Verify the ESP32 pin assignments and fan-output polarity in
  `include/hardware_config.h`.
- Drive each fan through its MOSFET or relay module. Do not power a fan directly
  from an ESP32 GPIO pin.
- Connect the fridge, freezer, and ambient DS18B20 probes to the shared 1-Wire
  input with the required pull-up resistor.
- Confirm the spillover fan moves cold freezer air into the top of the fridge
  compartment.
- Confirm the circulation fan moves air from the lower part of the fridge
  compartment toward the top.

## First startup

1. Build and upload the firmware with PlatformIO.
2. Confirm the expected firmware version on the OLED splash screen.
3. During the 15-second splash screen, verify that both fans run. This is an
   intentional output test.
4. Complete Wi-Fi provisioning through SensESP. Signal K is optional for local
   thermostat operation.
5. Assign the three probes by their ROM codes before relying on temperature
   control.
6. Review the temperature thresholds, fan delay, minimum runtimes, alarms, and
   display settings.

Existing saved settings are retained after a firmware update. After uploading a
new release, check the firmware version and review the saved settings rather
than assuming that new default values were applied.

## Using the rotary encoder

- From the home screen, rotation only wakes the display or records activity. It
  does not open settings.
- Press the encoder to enter settings. The menu always opens at item 1.
- Rotate clockwise or counterclockwise to browse all 21 items. Browsing wraps
  in both directions.
- Press a normal menu item to enter edit mode.
- Rotate to change its value, then press again to return to browsing.
- Sensor-assignment items open their assignment screen directly. Rotate to
  select a detected probe and press to save it.
- The menu returns to the home screen after approximately 10 seconds without
  input.
- If the display is asleep, the first rotation or press wakes it. Press again
  to enter the menu.
- During an active alarm, a press acknowledges the alarm before it can be used
  to open settings.

Changes are saved automatically shortly after editing.

## Assigning temperature probes

Probe roles are stored by the sensor's unique 64-bit ROM code, not by discovery
order.

1. Open `Assign fridge`, `Assign freezer`, or `Assign ambient`.
2. Warm the desired probe with your hand.
3. Rotate through the detected probes and watch the live temperature to
   identify it.
4. Press to save the selected probe.
5. Repeat for the other roles.

Each assignment list includes a final `No sensor assigned` option. Select it to
clear that role. The option remains available even when no probes are currently
detected, allowing a previously saved assignment to be removed.

One physical probe cannot remain assigned to multiple roles. Assigning it to a
new role clears its previous role. Power-cycle the controller after adding or
replacing a probe so the 1-Wire bus discovers it.

The freezer probe is optional. Without it, normal fridge control continues but
freezer-temperature lockout is unavailable. The fridge probe is required for
normal thermostat control.

## How fridge temperature control works

The controller takes a new sensor reading about every five seconds. The OLED
and normal thermostat logic use a rolling average of the latest six readings,
or roughly 30 seconds of temperature history. Freezer lockout uses the latest
valid freezer reading so it is not delayed by that average.

The two fridge settings form one control band:

- `Fridge max T` is the warm limit. When the filtered fridge temperature stays
  at or above this value for the fan trigger delay, spillover starts.
- `Fridge min T` is the cool limit. Spillover may stop at or below this value,
  but only after its minimum runtime has completed.
- `Fridge min T` must remain at least 0.5°C below `Fridge max T`.

Circulation starts immediately whenever spillover starts. It can also start
independently when the filtered fridge temperature remains at or below
`Fridge min T` for the fan trigger delay. Once either fan starts, its own
minimum runtime is enforced.

Reaching `Fridge min T` never ends spillover early. Among valid temperature
conditions, reaching `Freez T lockout` is the only event that overrides the
spillover minimum runtime and stops spillover immediately. Sensor faults retain
their separate fail-safe behavior.

### Control summary

| Condition | Spillover fan | Circulation fan |
|---|---:|---:|
| Fridge at or above `Fridge max T` after delay | ON | ON |
| Fridge between MIN and MAX | Normally OFF | Normally OFF |
| Fridge at or below `Fridge min T` after delay | OFF after minimum runtime | ON |
| Freezer at or above `Freez T lockout` | Immediately OFF | Controlled normally |

An active minimum runtime can temporarily keep a fan on even when the table
says it would normally be off.

## Recommended starting values

These are starting points, not universal requirements:

| Setting | Suggested value |
|---|---:|
| Fridge max T | 5.0°C |
| Fridge min T | 4.0°C |
| Fan delay | 15 seconds |
| Spill min ON | 2 minutes |
| Circ min ON | 2 minutes |

Observe several complete cooling cycles before making changes. If cold air
blows directly on the fridge probe and causes early threshold crossings,
increase the relevant minimum runtime or relocate/shield the probe. Change one
setting at a time and allow the compartment to stabilize.

## Device menu reference

| Item | OLED label | Purpose and limits |
|---:|---|---|
| 1 | `Fridge max T` | Filtered fridge temperature that requests spillover ON. Absolute range: −9.5°C to 10°C; its live lower limit is item 2 plus 0.5°C. |
| 2 | `Fridge min T` | Spillover OFF target after minimum runtime; also requests cold circulation. Absolute range: −10°C to 9.5°C; its live upper limit is item 1 minus 0.5°C. |
| 3 | `Freez T lockout` | Latest freezer temperature that immediately stops and blocks spillover. Range: −30°C to 10°C. |
| 4 | `Fridge alarm` | High fridge-temperature alarm. Range: 0°C to 30°C. |
| 5 | `Freezer alarm` | High freezer-temperature alarm. Range: −20°C to 10°C. |
| 6 | `Units` | Select Celsius or Fahrenheit for the OLED and physical menu. Internal storage and SensESP settings remain Celsius. |
| 7 | `Cal Fridge` | Offset applied to the assigned fridge probe. Range: ±5°C or the equivalent Fahrenheit offset. |
| 8 | `Cal Freezer` | Offset applied to the assigned freezer probe. Range: ±5°C or equivalent. |
| 9 | `Cal Ambient` | Offset applied to the assigned ambient probe. Range: ±5°C or equivalent. |
| 10 | `Fan delay` | How long a fresh, filtered start condition must persist. Range: 5–180 seconds in 5-second steps. Circulation follows an active spillover fan immediately. |
| 11 | `Spill min ON` | Minimum spillover runtime once started. Range: 1–5 minutes. Freezer lockout and fail-safe faults can stop it earlier. |
| 12 | `Circ min ON` | Minimum circulation runtime once started. Range: 1–5 minutes. |
| 13 | `Get-me-home fan` | Optional spillover duty cycle used only when the fridge probe has failed: OFF or 5, 10, 20, 30, or 40 minutes per hour. |
| 14 | `Buzzer` | Enable or disable the audible alarm. Visual and Signal K alarms remain available when the buzzer is disabled. |
| 15 | `ACTIVE ERRORS` | View active fault count, code, and message. Press to browse faults, rotate through them, and press to return. |
| 16 | `OLED contrast` | Display brightness. Range: 10–100% in 10% steps. |
| 17 | `Display off` | Automatic display timeout: Never, 1, 5, 10, 15, 20, 30, or 60 minutes. |
| 18 | `Display layout` | Choose `FRDG \| FRZ` or `FRZ \| FRDG` on the home screen. |
| 19 | `Assign fridge` | Select the probe used for fridge display and control, or select `No sensor assigned` to clear the role. |
| 20 | `Assign freezer` | Select the probe used for freezer display, alarm, and spillover lockout, or clear the role. |
| 21 | `Assign ambient` | Select the probe used for ambient/cabin display and publishing, or clear the role. |

## SensESP web settings

The SensESP web interface exposes the same saved settings and shared numeric
limits. Temperature values in the web interface are in Celsius even when the
OLED is set to Fahrenheit. Device-menu edits and web edits are normalized by
the same runtime limits when saved.

Use the OLED assignment workflow when possible because it shows live probe
temperatures. Advanced users may enter a known 16-character sensor ROM in the
web interface.

## Alarms and faults

- A fridge or freezer high-temperature alarm activates only after startup
  arming conditions are satisfied.
- Press the encoder to acknowledge the audible and full-screen alarm. The
  underlying Signal K alarm remains active until its condition clears.
- A missing or invalid fridge probe disables normal thermostat control.
- Get-me-home mode is OFF by default and must be enabled deliberately.
- A missing freezer probe removes freezer lockout but does not disable normal
  fridge control.
- A long spillover run, encoder problem, missing probe, invalid probe, or
  Signal K connection problem appears under `ACTIVE ERRORS`.

## Fine-tuning

- If the fridge becomes too warm before cooling begins, reduce
  `Fridge max T` or shorten the fan delay.
- If cooling stops too soon, reduce `Fridge min T` or increase
  `Spill min ON`.
- If the compartment overshoots too cold, increase `Fridge min T` or reduce
  `Spill min ON`.
- If temperature layering persists, increase `Circ min ON`.
- If fans respond to a brief door opening or a cold plume at the sensor,
  increase the fan delay.
- Keep the fridge probe out of the direct spillover air stream when practical.
