# BLE Controller Profiles - Implementation Milestones

## M1. Freeze the cross-chip contract

- Publish `docs/ESP32C6_BLE_PROFILE_HANDOFF.md`.
- Freeze profile IDs, UART byte layouts, result codes and checksum vectors.
- Require both repositories to use protocol version 1.
- Do not begin independent protocol design in either repository.

Exit condition: both implementations can be reviewed against the same contract.

## M2. RP2350 configuration and menu

- Add a persisted `bluetoothProfile` field to
  `FightpadESP32ProxyOptions`, defaulting to Xbox BLE.
- Add a Bluetooth Type menu independent from USB Controller Type.
- Save in USB transport without rebooting or waking ESP32-C6.
- Display applying, pair-again and protocol-error states in Bluetooth transport.

Exit condition: menu persistence survives power cycling and USB mode remains
unchanged.

## M3. Bidirectional protocol

- RP2350 sends versioned mode commands with sequence IDs and bounded retry.
- ESP32-C6 validates, acknowledges and persists accepted commands.
- Both sides reject corrupt, stale or unsupported frames safely.
- Add fixed byte-level test vectors on both sides.

Exit condition: each side passes the same command/ACK vectors without BLE being
enabled.

## M4. ESP32-C6 profile framework and Generic regression

- Select identity, descriptor and encoder before `esp_hidd_dev_init()`.
- Add an early boot profile-sync window with NVS fallback.
- Implement safe pending bond-clear and post-restart pairing flags.
- First reproduce the current Generic BLE behavior through the new framework.

Exit condition: Generic BLE has no button, reconnect or pairing regression.

## M5. Add profiles incrementally

Implement and validate in this order:

1. Keyboard BLE
2. Xbox BLE
3. PS5 BLE (PC Experimental)

Exit condition: every profile passes its mapping table before the next profile
is introduced.

## M6. Whole-device integration

- Validate menu, RP2350 flash setting, C6 NVS setting and host-visible profile.
- Confirm only C6 restarts on a live profile change.
- Confirm a changed profile opens a 30-second pairing window.
- Confirm a same-profile power cycle reconnects without forced pairing.
- Exercise broken UART, bad checksum, invalid ID and version mismatch.
- Test Windows first; then cover Generic/Keyboard on macOS and Android.

Exit condition: the acceptance matrix in the handoff document is complete.

## M7. Production release gate

- Choose authorized production VID/PID and branding policy.
- Keep compatibility-test identities separate from release identities.
- Record matching RP2350 UF2 and ESP32-C6 image versions.
- Publish manufacturing flash and functional-test steps.

Exit condition: one unambiguous, traceable image pair is approved for mass
production.

