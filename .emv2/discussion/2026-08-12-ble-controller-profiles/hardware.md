# BLE Controller Profiles - Hardware and Cross-Chip Boundary

## 1. Confirmed architecture

The product contains two independently executing controllers:

- RP2350 runs GP2040-CE, owns the physical menu, display, gameplay state and
  persisted product selection.
- ESP32-C6 owns the Bluetooth Low Energy stack and presents the HID device to
  the host.
- RP2350 and ESP32-C6 exchange transport state, controller input and status over
  a full-duplex UART link.

Changing only the RP2350 USB `InputMode` cannot change the BLE controller type.
Both firmware projects must implement the same versioned interface.

## 2. Existing ESP32-C6 constraints

The inspected ESP32-C6 firmware currently has one statically selected BLE HID
configuration:

- Device name: `FP12Slim-C6`
- VID/PID: `0x1209:0x2040`
- One generic gamepad HID report descriptor
- Five-byte gamepad input report
- The configuration is passed to `esp_hidd_dev_init()` during application boot

Therefore profile identity, descriptor and report encoder must be selected
before BLE HID initialization. Attempting to replace a live report map in place
would add unnecessary NimBLE/HID lifecycle risk.

## 3. Confirmed profile-change lifecycle

For an actual Bluetooth profile change:

1. RP2350 persists the requested profile.
2. RP2350 sends a versioned profile command over UART.
3. ESP32-C6 validates and persists the accepted profile in NVS.
4. ESP32-C6 acknowledges the accepted profile to RP2350.
5. ESP32-C6 removes bonds that are incompatible with the changed GATT/HID
   identity.
6. ESP32-C6 performs `esp_restart()`.
7. On boot, ESP32-C6 reads the profile before `esp_hidd_dev_init()` and selects
   the matching identity, report descriptor and report encoder.
8. ESP32-C6 opens a 30-second pairing window and reports status to RP2350.

Only ESP32-C6 restarts. RP2350 remains running so the display can show progress
and a re-pairing instruction.

Normal boot or reconnect with the same profile must not clear the bond or force
pairing.

## 4. Safety and recovery

- ESP32-C6 acknowledges before restart so RP2350 can distinguish acceptance
  from a broken UART link.
- The mode command includes a protocol version and checksum.
- Unknown versions or profile IDs fall back to Generic BLE and report an error
  or fallback status.
- RP2350 retries a missing command acknowledgement with a bounded timeout; it
  does not continuously reset ESP32-C6.
- ESP32-C6 NVS provides a bootable local fallback, while RP2350 remains the
  product-level source of truth.

## 5. Cross-repository collaboration

Implementation must be driven by one shared interface contract. The final
discussion output will include `docs/ESP32C6_BLE_PROFILE_HANDOFF.md` containing:

- Protocol constants and byte layout
- State and restart sequence
- Complete mapping tables
- Error behavior and compatibility rules
- Test vectors and integration acceptance checks

The RP2350 and ESP32-C6 implementations may be produced by separate agents, but
neither side may independently change this contract. Any protocol change must
increment its version and update both projects.

