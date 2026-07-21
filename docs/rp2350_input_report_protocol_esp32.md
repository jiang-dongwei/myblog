# RP2350B -> ESP32-C6 Input Report and Sleep Detection Protocol

> Target reader: the AI or developer implementing the ESP32-C6 firmware.
>
> Scope: this document describes the protocol that is already implemented by
> the Fightpad12Slim RP2350B firmware. It also defines how the ESP32-C6 should
> derive user inactivity from input reports without changing the RP2350B side.

## 1. Important conclusion

While Bluetooth transport is selected, the RP2350B sends an `FP` input report:

- immediately when the processed input state changes;
- otherwise every 10ms as a keepalive, approximately 100 reports per second.

Neutral reports therefore continue while nobody is touching the controller.
The ESP32-C6 must **not** treat receipt of any UART frame as user activity. It
must parse the `FP` payload and update its inactivity timer only while a button,
D-pad direction, or non-neutral axis is active.

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
Byte 4: D-pad bit mask           low four bits
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

### 4.2 D-pad bitmap

Only the low four bits of Byte4 are used:

| Bit | Mask | Direction |
|---:|---:|---|
| 0 | `0x01` | Up |
| 1 | `0x02` | Down |
| 2 | `0x04` | Left |
| 3 | `0x08` | Right |

Directions may be combined, for example Up+Right is `0x09`. This is a bitmap,
not a USB HID hat-switch value.

### 4.3 Neutral report

The complete neutral frame is:

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

## 5. Send timing and transport behavior

### 5.1 Bluetooth transport selected

- A changed report is sent immediately, even if less than 10ms has passed.
- An unchanged report is resent once 10ms has elapsed since the last report.
- Holding a button therefore produces repeated active reports at about 100Hz.
- Releasing all controls produces repeated neutral reports at about 100Hz.

### 5.2 Switching away from Bluetooth

When the RP2350B changes to USB transport, it sends one neutral `FP` report if
Bluetooth input reporting had been active, then stops periodic `FP` reports.
The separate `FT` transport frame continues to identify the selected mode.

### 5.3 Other periodic UART traffic

The UART is not silent when the controller is idle:

- `FP` input keepalive: approximately every 10ms in Bluetooth mode;
- `FT` transport state: approximately every 250ms;
- `FB` battery state: approximately every 1000ms when battery data is valid.

An ESP32 implementation must dispatch by Byte1 and use only valid `FP` payloads
for input-activity decisions.

## 6. ESP32 application-level sleep state machine

Use this logic when "sleep" means an application state in which BLE/HID work is
reduced or stopped but the UART receive task remains capable of parsing frames.

Define a configurable timeout, for example:

```c
#define INPUT_SLEEP_TIMEOUT_MS 60000
#define INPUT_AXIS_WAKE_DEADZONE 8
```

An input report is active when:

```c
active = buttons != 0 ||
         dpad != 0 ||
         abs(lx) > INPUT_AXIS_WAKE_DEADZONE ||
         abs(ly) > INPUT_AXIS_WAKE_DEADZONE;
```

Recommended behavior:

1. Accept only a complete `FP` frame with a valid XOR checksum.
2. Decode buttons, D-pad, LX, and LY.
3. If `active` is true:
   - set `last_input_activity_ms = now`;
   - if the application is sleeping, leave sleep immediately;
   - process this same report as the first wake-up HID report so the initial
     button press is not discarded.
4. If `active` is false:
   - do not refresh `last_input_activity_ms`;
   - enter application sleep once the configured timeout expires.
5. Continue treating every active report as activity while a control is held,
   so the application cannot sleep during a long press.
6. A checksum error, battery frame, transport frame, firmware text, or Bluetooth
   status frame must not count as input activity.

### 6.1 Reference pseudocode

```c
static uint32_t last_input_activity_ms;
static bool app_sleeping;

static bool xor_valid(const uint8_t frame[8]) {
    uint8_t checksum = 0;
    for (int i = 0; i < 7; i++) checksum ^= frame[i];
    return checksum == frame[7];
}

static void handle_input_frame(const uint8_t frame[8], uint32_t now_ms) {
    if (frame[0] != 0x46 || frame[1] != 0x50 || !xor_valid(frame)) {
        return;
    }

    uint16_t buttons = (uint16_t)frame[2] |
                       ((uint16_t)frame[3] << 8);
    uint8_t dpad = frame[4] & 0x0F;
    int8_t lx = (int8_t)frame[5];
    int8_t ly = (int8_t)frame[6];

    bool active = buttons != 0 ||
                  dpad != 0 ||
                  abs((int)lx) > INPUT_AXIS_WAKE_DEADZONE ||
                  abs((int)ly) > INPUT_AXIS_WAKE_DEADZONE;

    if (active) {
        last_input_activity_ms = now_ms;
        if (app_sleeping) {
            exit_application_sleep();
            app_sleeping = false;
        }
        submit_hid_report(buttons, dpad, lx, ly);
        return;
    }

    submit_hid_report(buttons, dpad, lx, ly);

    if (!app_sleeping &&
        (uint32_t)(now_ms - last_input_activity_ms) >= INPUT_SLEEP_TIMEOUT_MS) {
        enter_application_sleep();
        app_sleeping = true;
    }
}
```

Initialize `last_input_activity_ms` when the input/UART service starts so the
device does not enter sleep immediately after boot.

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
Neutral:             46 50 00 00 00 00 00 16
B1 pressed:          46 50 01 00 00 00 00 17
Up pressed:          46 50 00 00 01 00 00 17
B1 + Up pressed:     46 50 01 00 01 00 00 16
LX fully positive:   46 50 00 00 00 7F 00 69
```

For each test, verify the XOR before checking activity. Repeated neutral frames
must not postpone sleep; repeated active frames must keep the application awake.
