# RP2350B -> ESP32-C6 Input Report, Gameplay Lock, and Sleep Protocol

> Target reader: the AI or developer implementing the ESP32-C6 firmware.
>
> Scope: this document describes the Fightpad12Slim RP2350B input-report wire
> format, including the menu gameplay-lock flag, and the activity information
> available to the ESP32-C6. The exact cross-repository lock state-machine
> contract is in `docs/menu_gameplay_lock_protocol_rp2350_esp32.md`.

## 1. Important conclusion

While Bluetooth transport is selected, the RP2350B sends an `FP` input report:

- immediately when the processed input state changes;
- otherwise every 10ms as a keepalive, approximately 100 reports per second.

When the RP2350 menu owns gameplay input, the RP2350B keeps the real
buttons/D-pad/axes in the `FP` payload and sets Byte4 bit7 (`0x80`). An updated
ESP32-C6 must immediately publish neutral BLE HID while this flag is set and
must drain held controls before forwarding input again. The flag is deliberately
not encoded in button bit15.

Neutral reports continue while nobody is touching the controller. The
ESP32-C6 must **not** treat receipt of any UART frame as user activity. Only an
unlocked payload accepted in the `UNLOCKED` gate state may feed the existing
real-input/activity tracker. Locked and drain-state payloads never do.

## 2. UART connection

| RP2350B | Direction | ESP32-C6 |
|---|---:|---|
| GP44 / UART0 TX | -> | GPIO17 / UART0 RX |
| GP45 / UART0 RX | <- | GPIO16 / UART0 TX |
| GND | - | GND |

- 115200 baud
- 8 data bits, no parity, 1 stop bit
- no RTS/CTS flow control
- fixed 8-byte binary frames

## 3. Common frame format

```text
Byte 0: 0x46
Byte 1: frame type
Byte 2..6: type-specific payload
Byte 7: Byte0 XOR Byte1 XOR ... XOR Byte6
```

Discard a candidate frame when its checksum is wrong. The receiver should then
resume searching for a `0x46` frame start.

Frames relevant to ESP32-C6 receive logic:

| Type | ASCII | Direction | Meaning |
|---:|:---:|---|---|
| `0x50` | `P` | RP2350 -> ESP32 | Input report |
| `0x54` | `T` | RP2350 -> ESP32 | USB/Bluetooth transport selection |
| `0x42` | `B` | RP2350 -> ESP32 | Battery information |

## 4. Input report frame: `0x50` (`FP`)

```text
Byte 0: 0x46                     frame magic ('F')
Byte 1: 0x50                     input report type ('P')
Byte 2: buttons bits 0..7        little-endian low byte
Byte 3: buttons bits 8..15       little-endian high byte
Byte 4: flags and D-pad          bit7=gameplay lock, low nibble=D-pad
Byte 5: LX                       signed int8, -127..127
Byte 6: LY                       signed int8, -127..127
Byte 7: XOR(Byte0..Byte6)
```

Byte 5 and Byte 6 are transmitted as the two's-complement byte representation
of an `int8_t`. Cast them back to `int8_t` before testing the axis dead zone.

### 4.1 Button bitmap

```text
buttons = Byte2 | (Byte3 << 8)
```

| Bit | Mask | RP2350 input |
|---:|---:|---|
| 0 | `0x0001` | B1 |
| 1 | `0x0002` | B2 |
| 2 | `0x0004` | B3 |
| 3 | `0x0008` | B4 |
| 4 | `0x0010` | L1 |
| 5 | `0x0020` | L2 |
| 6 | `0x0040` | R1 |
| 7 | `0x0080` | R2 |
| 8 | `0x0100` | L3 |
| 9 | `0x0200` | R3 |
| 10 | `0x0400` | S1 |
| 11 | `0x0800` | S2 |
| 12 | `0x1000` | A1 |
| 13 | `0x2000` | A2 |
| 14 | `0x4000` | Turbo control input |
| 15 | `0x8000` | Reserved, currently zero |

### 4.2 Byte4 flags and D-pad bitmap

Byte4 is split into two independent fields:

| Bit | Mask | Meaning |
|---:|---:|---|
| 7 | `0x80` | Gameplay input is owned by the RP2350 menu |
| 6..4 | `0x70` | Reserved, currently zero |
| 3..0 | `0x0F` | D-pad bitmap |

Decode it as:

```c
bool gameplay_locked = (frame[4] & 0x80) != 0;
uint8_t dpad = frame[4] & 0x0F;
```

The D-pad low-nibble mapping is:

| Bit | Mask | Direction |
|---:|---:|---|
| 0 | `0x01` | Up |
| 1 | `0x02` | Down |
| 2 | `0x04` | Left |
| 3 | `0x08` | Right |

Directions may be combined, for example Up+Right is `0x09`. This is a bitmap,
not a USB HID hat-switch value. Never pass the complete Byte4 value to D-pad
conversion code because bit7 is not a direction.

### 4.3 Neutral reports

The normal unlocked neutral frame, also used as the explicit final report when
switching away from Bluetooth transport, is:

```text
46 50 00 00 00 00 00 16
```

Its decoded values are:

```text
buttons = 0x0000
dpad    = 0x00
lx      = 0
ly      = 0
```

A periodic input report may carry a neutral physical payload while gameplay is
locked. That frame is distinct because Byte4 bit7 is set:

```text
46 50 00 00 80 00 00 96
```

Both frames describe neutral physical controls, but only the second commands
the ESP32-C6 to remain in its locked HID state.

## 5. Send timing and transport behavior

### 5.1 Bluetooth transport selected

- A changed report is sent immediately, even if less than 10ms has passed.
- An unchanged report is resent once 10ms has elapsed since the last report.
- A gameplay-lock transition changes Byte4 and therefore participates in the
  same whole-frame comparison; lock and unlock are sent immediately.
- Holding a button therefore produces repeated active reports at about 100Hz.
- Releasing all controls produces repeated neutral reports at about 100Hz.

### 5.2 Switching away from Bluetooth

When the RP2350B changes to USB transport, it sends one neutral `FP` report if
Bluetooth input reporting had been active, then stops periodic `FP` reports.
This explicit report remains exactly `46 50 00 00 00 00 00 16`; it does not
inherit the gameplay-lock flag. The separate `FT` transport frame continues to
identify the selected mode.

### 5.3 Other periodic UART traffic

The UART is not silent when the controller is idle:

- `FP` input keepalive: approximately every 10ms in Bluetooth mode;
- `FT` transport state: approximately every 250ms;
- `FB` battery state: approximately every 1000ms when battery data is valid.

An ESP32 implementation must dispatch by Byte1 and use only valid `FP` payloads
for input-activity decisions.

## 6. ESP32 gameplay gate and application activity

The BLE HID side must have three states: `UNLOCKED`, `LOCKED`, and `DRAIN`.
Start in `DRAIN`. A valid `FP` with Byte4 bit7 set enters `LOCKED` and publishes
neutral HID. Clearing bit7 moves `LOCKED -> DRAIN`; do not forward a held exit
button. `DRAIN` returns to `UNLOCKED` only after a valid unlocked report has an
exactly neutral physical payload. See the shared contract for the required
`FT` and stale-link behavior.

Gameplay gating and activity detection are separate decisions. Only normal
gameplay accepted while `UNLOCKED` may call the existing activity path.
`LOCKED` and `DRAIN` frames do not update activity, regardless of the physical
values retained in their UART payload, and the lock transition is not an input
event. Clear the prior press-edge state when entering either suppressed state.

Define a configurable timeout and activity dead zone, for example:

```c
#define INPUT_SLEEP_TIMEOUT_MS 60000
#define INPUT_AXIS_WAKE_DEADZONE 8
```

### 6.1 Reference pseudocode

```c
typedef enum { GATE_DRAIN, GATE_UNLOCKED, GATE_LOCKED } gameplay_gate_t;

static gameplay_gate_t gate = GATE_DRAIN;
static uint32_t last_input_activity_ms;
static bool app_sleeping;

static bool xor_valid(const uint8_t frame[8]) {
    uint8_t checksum = 0;
    for (int i = 0; i < 7; i++) checksum ^= frame[i];
    return checksum == frame[7];
}

static void handle_input_frame(const uint8_t frame[8], uint32_t now_ms) {
    if (frame[0] != 0x46 || frame[1] != 0x50 || !xor_valid(frame)) return;

    uint16_t buttons = (uint16_t)frame[2] |
                       ((uint16_t)frame[3] << 8);
    bool gameplay_locked = (frame[4] & 0x80) != 0;
    uint8_t dpad = frame[4] & 0x0F;
    int8_t lx = (int8_t)frame[5];
    int8_t ly = (int8_t)frame[6];

    bool physical_neutral = buttons == 0 && dpad == 0 && lx == 0 && ly == 0;
    if (gameplay_locked) {
        gate = GATE_LOCKED;
        clear_input_press_state();
        submit_neutral_hid_report();
    } else {
        if (gate == GATE_LOCKED) {
            gate = GATE_DRAIN;
            clear_input_press_state();
        }

        if (gate == GATE_DRAIN) {
            submit_neutral_hid_report();
            if (physical_neutral) {
                gate = GATE_UNLOCKED;
                clear_input_press_state();
            }
        } else {
            bool active = buttons != 0 || dpad != 0 ||
                          abs((int)lx) > INPUT_AXIS_WAKE_DEADZONE ||
                          abs((int)ly) > INPUT_AXIS_WAKE_DEADZONE;
            note_real_input(input_activity_mask(buttons, dpad, lx, ly));
            submit_hid_report(buttons, dpad, lx, ly);
            update_existing_idle_policy(active, now_ms);
        }
    }
}
```

The helper names above describe integration with the current C6 activity path;
do not create a second idle policy. A bad checksum, `FB`, `FT`, firmware text,
or Bluetooth-status frame is not physical input activity. `FT` still changes
transport state as defined in the shared contract; it must not be counted as a
button event.

## 7. Hardware light/deep sleep warning

The current RP2350B protocol is not compatible with waking the ESP32-C6 from
hardware sleep on **any UART RX activity**, because neutral `FP`, `FT`, and `FB`
frames continue to create RX edges while the user is idle. Such a wake source
would wake the ESP32 repeatedly without a button press.

For real MCU light/deep sleep, use one of these separately designed mechanisms:

- keep UART reception available in an application-level idle state;
- add a dedicated button-activity wake GPIO from RP2350B to ESP32-C6; or
- add a sleep handshake that makes RP2350B stop periodic UART traffic and send
  only an explicit wake event when a real input becomes active.

Do not implement hardware UART wake from this document without first changing
that transport behavior.

## 8. Test frames

```text
Unlocked neutral:       46 50 00 00 00 00 00 16
Locked neutral:         46 50 00 00 80 00 00 96
B1 pressed:             46 50 01 00 00 00 00 17
Locked B1:              46 50 01 00 80 00 00 97
Up pressed:             46 50 00 00 01 00 00 17
B1 + Up pressed:        46 50 01 00 01 00 00 16
Locked GP19/A2:         46 50 00 20 80 00 00 B6
Unlocked GP19/A2:       46 50 00 20 00 00 00 36
Locked GP20/Turbo:      46 50 00 40 80 00 00 D6
LX fully positive:      46 50 00 00 00 7F 00 69
```

For each test, verify the XOR before checking activity. Repeated unlocked active
frames keep the existing application activity behavior; locked and drain-state
frames never update it. Verify that every locked frame produces neutral BLE
HID, and that an unlocked held GP19/A2 frame remains neutral during `DRAIN`
until an unlocked neutral frame has been accepted.
