# ESP32-C6 -> RP2350B Bluetooth Status Protocol

## UART

- RP2350B UART0: GP44 TX, GP45 RX
- ESP32-C6 UART0: GPIO17 RX, GPIO16 TX
- 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control
- Direction: ESP32-C6 to RP2350B notification; RP2350B sends no reply

## Frame

Every status notification is one 8-byte frame:

| Byte | Value | Meaning |
|---:|---:|---|
| 0 | `0x46` | Shared ESP32 frame magic (`F`) |
| 1 | `0x53` | Bluetooth status frame type (`S`) |
| 2 | `status` | Connection status |
| 3..6 | `0x00` | Reserved |
| 7 | XOR | XOR of bytes 0 through 6 |

Status values:

| Value | OLED | Temporary GP40 Base output |
|---:|---|---|
| `0x00` | `Disconnected` for 1000ms | Black for 1000ms |
| `0x01` | `Connecting...` until the next status | Blue Chase |
| `0x02` | `Connected` for 1000ms | Solid blue for 1000ms |
| `0x03` | `Pairing...` until the next status | Blue Chase |

Frames with a bad XOR or a status value outside `0x00..0x03` are ignored.
The status frame shares the existing UART byte stream with `0x46 0x49`
firmware-information frames. The RP2350B receiver synchronizes both frame
types and dispatches them only after the complete frame passes XOR validation.

## Examples

```text
Disconnected: 46 53 00 00 00 00 00 15
Connecting:    46 53 01 00 00 00 00 14
Connected:     46 53 02 00 00 00 00 17
Pairing:       46 53 03 00 00 00 00 16
```

The ESP32-C6 sends once when its state changes. `Connecting...` and `Pairing...`
therefore have no RP2350-side timeout; they remain active until another valid
status arrives. A terminal `Disconnected` or `Connected` result expires after
1000ms and the OLED/Base output then returns to the state that existed before
the notification.

At startup, the ESP32-C6 sends firmware-information frames, then repeats the
plain-text ASCII line `C6_DONE\n`, then sends the initial Bluetooth status.
`C6_DONE` is not an 8-byte binary frame and has no runtime action here. The
RP2350 receiver safely ignores it while looking for a `0x46 0x49` or
`0x46 0x53` frame start, so the following status frame remains synchronized.

The temporary lighting indication may wake GP40 while the saved menu state is
`All OFF`. The BQ27220 `SOC <= 7%` cutoff remains higher priority and keeps both
LED chains and their boost supply off.
