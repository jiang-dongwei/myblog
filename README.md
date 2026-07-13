# ESP32-C6 BLE HID Gamepad Test

This is the standalone ESP32-C6 Bluetooth bring-up firmware for the Fightpad 12
Slim board.

The firmware boots as a BLE HID gamepad and accepts a small binary input frame
over the ESP32-C6 UART0 programming pins. This keeps the first working BT-HID
path simple: the RP2350 side can send a compact button state frame, and the C6
turns it into a BLE gamepad report.

## Behavior

- BLE device name: `FP12Slim-C6`
- HID type: generic gamepad
- HID report: 16 buttons, one hat switch, X/Y axes
- Runtime report: UART-fed gamepad state, with neutral fallback on timeout
- Pairing: BLE bonding with Just Works pairing, no passkey required
- UART input: ESP32-C6 UART0, GPIO16 TX / GPIO17 RX, 115200 8N1

## UART Input Frame

The ESP32-C6 accepts 8-byte frames:

| Byte | Value |
| ---: | --- |
| 0 | `0x46` (`F`) |
| 1 | `0x50` (`P`) |
| 2 | Buttons low byte |
| 3 | Buttons high byte |
| 4 | D-pad bitmask: bit0 Up, bit1 Down, bit2 Left, bit3 Right |
| 5 | X axis, signed int8 |
| 6 | Y axis, signed int8 |
| 7 | XOR checksum of bytes 0 through 6 |

Button bits map directly to HID buttons 1 through 16. The first 15 intended
Fightpad bits are `B1`, `B2`, `B3`, `B4`, `L1`, `L2`, `R1`, `R2`, `L3`, `R3`,
`S1`, `S2`, `A1`, `A2`, and `TURBO`; bit 15 is reserved.

If valid UART frames stop for 250 ms, the firmware restores a neutral report so
buttons cannot remain stuck.

## Build

Docker-only build (no WSL, no host-local ESP-IDF):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\docker-build-esp32c6.ps1
```

## Flash

For the current board, put the ESP32-C6 into download mode before flashing:

1. Hold or drive ESP32-C6 `GPIO9` low during reset.
2. Reset/release `CHIP_PU`.
3. Flash over the ESP32-C6 UART programming path.

Example command once the correct ESP32-C6 serial port is known:

```powershell
idf.py -p COMx flash monitor
```

After boot, scan Bluetooth devices from a phone or computer and pair with
`FP12Slim-C6`.
