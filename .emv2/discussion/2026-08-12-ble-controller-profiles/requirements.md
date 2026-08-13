# BLE Controller Profiles - Requirements

## 1. Product scope

Add an independent Bluetooth controller-type setting. It must not reuse or
silently change the USB controller type.

Supported Bluetooth profiles:

1. `XBOX BLE` (default)
2. `GENERIC BLE`
3. `KEYBOARD BLE`
4. `PS5 BLE (PC)` (experimental)

`PS5 BLE (PC)` targets Windows, Steam and SDL-style host testing. It does not
promise PlayStation 5 console compatibility and does not include touch
coordinates, motion sensors or adaptive triggers in the first version.

## 2. Ownership and persistence

- RP2350 is the authoritative source for the selected Bluetooth profile because
  it owns the device menu and product configuration.
- RP2350 persists the selection and sends it to ESP32-C6 through UART.
- ESP32-C6 may cache the last accepted profile in NVS for early-boot fallback,
  but must accept RP2350 as the final source of truth.
- An invalid or unsupported profile falls back to `GENERIC BLE`.

Stable profile IDs:

| ID | Profile |
|---:|---|
| 0 | Generic BLE |
| 1 | Xbox BLE |
| 2 | Keyboard BLE |
| 3 | PS5 BLE (PC) |

## 3. Runtime behavior

- If the profile is changed while USB transport is active, save it and apply it
  the next time Bluetooth transport is entered.
- If the profile is changed while Bluetooth transport is active, send the new
  mode to ESP32-C6, disconnect BLE, rebuild/restart the HID profile and open a
  30-second pairing window.
- Only an actual profile change clears the incompatible BLE bond. Normal reboot
  or reconnect with the same profile preserves the bond.
- The display must clearly show the selected profile and request re-pairing
  after a live profile change.

## 4. RP2350 to ESP32-C6 protocol

Introduce a versioned profile command in addition to the existing input-report
frame. Proposed eight-byte command:

```text
[0x46, 0x4D, profile_id, protocol_version, 0, 0, 0, xor]
```

ESP32-C6 must acknowledge the accepted profile before restarting its BLE HID
service. RP2350 sends the command on boot, Bluetooth transport entry and profile
change, and retries when no acknowledgement is received.

## 5. Profile behavior and mapping

### Xbox BLE

- B1/B2/B3/B4 -> A/B/X/Y
- L1/R1 -> LB/RB
- L2/R2 -> digital LT/RT
- L3/R3 -> stick clicks
- S1/S2 -> View/Menu
- A1/A2 -> Guide/Share or Capture

### PS5 BLE (PC)

- B1/B2/B3/B4 -> Cross/Circle/Square/Triangle
- L1/L2/R1/R2 and L3/R3 retain their matching controller meanings
- S1/S2 -> Create/Options
- A1/A2 -> PS/Touchpad click
- Unsupported report fields remain neutral.

### Generic BLE

Expose the standard direction, axes and normal Fightpad buttons through a
generic gamepad HID report.

### Keyboard BLE

Use the existing Fightpad12Slim board-default keyboard mapping:

- Directions -> arrow keys
- B1/B2/R2/L2 -> Left Shift/Z/X/V
- B3/B4/R1/L1 -> Left Ctrl/Left Alt/Space/C
- S1/S2 -> 5/1
- L3/R3 -> `=`/`-`
- A1/A2 -> 9/F2

For the first version this is a fixed BLE keyboard product mapping. Changes to
the USB keyboard map in Web Config do not automatically change BLE keyboard
mapping unless a future protocol revision transmits the complete key map.

The physical Turbo input remains an internal product function and is not
exported as an extra host button in these profiles.

## 6. BLE identity and production gate

- Development compatibility builds may use identities needed to test host
  recognition as Xbox or DualSense-style controllers.
- Production firmware must make the VID/PID and branding choice explicit.
  Shipping with Microsoft or Sony identifiers without authorization is not an
  assumed release configuration.
- Keep compatibility-test identity and production identity behind an explicit
  compile-time configuration or product variant.

## 7. Acceptance conditions

- Selecting a Bluetooth type never changes the configured USB type.
- The RP2350 screen, persisted setting and ESP32-C6 active HID profile agree.
- Each profile produces its documented button mapping on a supported PC host.
- Same-profile reboot reconnects without forced pairing.
- Profile change prompts a new 30-second pairing operation and does not leave a
  stale cached descriptor active on the host.
- Invalid UART commands cannot leave ESP32-C6 in an undefined HID mode.

