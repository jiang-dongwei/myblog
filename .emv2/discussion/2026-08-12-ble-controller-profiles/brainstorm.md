# BLE Controller Profiles - Solution Selection

## 1. Considered solutions

### A. One ESP32-C6 firmware with multiple boot-selected profiles

The C6 image contains Generic, Xbox, Keyboard and PS5-PC descriptors and report
encoders. NVS determines which profile is selected before BLE HID
initialization.

Advantages:

- One production firmware image
- Runtime selection through the Fightpad menu
- One upgrade and support path
- Clean separation between USB and BLE controller types

This is the selected solution.

### B. Separate ESP32-C6 firmware for every profile

This is simpler inside an individual image but introduces multiple production,
upgrade and service variants. It is rejected.

### C. One composite BLE device exposing all profile interfaces

This can create host-side ambiguity and does not reproduce distinct controller
identities cleanly. It is rejected.

## 2. Selected RP2350 design

- Add an independent `Bluetooth Type` physical-menu page.
- Add a persisted `bluetoothProfile` field to
  `FightpadESP32ProxyOptions`; do not reuse USB `InputMode`.
- The menu writes the setting and the ESP32 proxy owns UART delivery,
  acknowledgement, retry and status exposure.
- The display presents applying, re-pairing and failure states without rebooting
  RP2350.

## 3. Selected ESP32-C6 design

Keep one application image and introduce isolated profile modules rather than
adding all logic to the existing monolithic `main.c`:

- `ble_profiles.c/.h`: profile table, HID maps, identity metadata and report
  encoders
- `uart_protocol.c/.h`: common eight-byte protocol constants and helpers
- `main.c`: boot selection, BLE lifecycle, pairing, NVS and orchestration

ESP32-C6 reads the profile before `esp_hidd_dev_init()`. A live profile change
is persisted, acknowledged and applied through `esp_restart()`.

## 4. Selected mode protocol

All frames retain the existing eight-byte format and XOR checksum.

RP2350 to ESP32-C6 mode command:

```text
[0x46, 'M', version, profile, sequence, flags, 0, checksum]
```

ESP32-C6 to RP2350 acknowledgement:

```text
[0x46, 'A', version, accepted_profile, sequence, result, 0, checksum]
```

The sequence field rejects stale acknowledgements. Result codes distinguish an
unchanged active profile, an accepted change that will restart, invalid-profile
fallback and internal failure.

Existing report (`'P'`), transport (`'T'`), battery (`'B'`), firmware-info
(`'I'`) and BLE-status (`'S'`) frames remain intact.

## 5. Identity policy

Profile behavior and device identity are table driven. Development compatibility
identities and production identities remain explicit build/product choices.
Microsoft or Sony identifiers are not silently made the only shipping option.

