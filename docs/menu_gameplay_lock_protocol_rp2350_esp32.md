# RP2350B / ESP32-C6 Menu Gameplay-Lock Contract

## 1. Scope and target

This is the cross-repository contract for implementing the receiving side in:

```text
E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test
```

The RP2350B implementation lives in this GP2040-CE repository. Do not edit the
ESP32-C6 repository while changing the RP2350B side. An AI working in the C6
repository must treat this document as the wire and state-machine contract.

The goal is modal menu ownership:

- GP30/GP31/GP32 continue to operate the RP2350 menu.
- GP2..GP20 may still appear as real values in the UART payload.
- While gameplay lock is asserted, the ESP32-C6 must expose neutral BLE HID.
- After unlock, held controls must be drained before gameplay resumes.

## 2. Non-negotiable compatibility rules

1. Keep the `FP` frame type (`0x46 0x50`) and fixed length of eight bytes.
2. Keep the XOR checksum over Bytes0..6 in Byte7.
3. Keep the existing button bitmap in Bytes2..3.
4. Keep button bit15 reserved; do not use it as the lock flag.
5. Store gameplay lock only in Byte4 bit7 (`0x80`).
6. Keep the D-pad in Byte4 bits3..0. Bits6..4 remain reserved and zero.
7. Keep actual buttons, D-pad, LX, and LY in periodic Bluetooth `FP` reports
   while locked. The lock flag commands HID suppression; it does not erase the
   physical payload.
8. The explicit `FP` sent while switching away from Bluetooth stays the exact
   unlocked neutral frame; it must not carry the lock flag.
9. The lock bit is part of the complete eight-byte report comparison, so lock
   and unlock transitions are sent immediately rather than waiting for the
   10ms keepalive.

## 3. Wire format

### 3.1 `FP` input report (`0x50`)

```text
Byte 0: 0x46
Byte 1: 0x50
Byte 2: buttons bits 0..7
Byte 3: buttons bits 8..15
Byte 4: bit7 gameplay lock | bits3..0 D-pad
Byte 5: LX as signed int8 (-127..127)
Byte 6: LY as signed int8 (-127..127)
Byte 7: XOR(Byte0..Byte6)
```

Required decode:

```c
uint16_t buttons = (uint16_t)frame[2] |
                   ((uint16_t)frame[3] << 8);
bool gameplay_locked = (frame[4] & 0x80) != 0;
uint8_t dpad = frame[4] & 0x0F;
int8_t lx = (int8_t)frame[5];
int8_t ly = (int8_t)frame[6];
```

Never pass all of Byte4 to a D-pad/hat converter.

Button bits are unchanged:

| Bit | Input | Bit | Input |
|---:|---|---:|---|
| 0 | B1 | 8 | L3 |
| 1 | B2 | 9 | R3 |
| 2 | B3 | 10 | S1 |
| 3 | B4 | 11 | S2 |
| 4 | L1 | 12 | A1 |
| 5 | L2 | 13 | A2 / physical GP19 |
| 6 | R1 | 14 | Turbo control / physical GP20 |
| 7 | R2 | 15 | Reserved, zero |

D-pad low-nibble bits are Up=`0x01`, Down=`0x02`, Left=`0x04`, and
Right=`0x08`.

### 3.2 `FT` transport report (`0x54`)

```text
Byte 0: 0x46
Byte 1: 0x54
Byte 2: 0x01 Bluetooth selected, 0x00 Bluetooth not selected
Byte 3..6: 0x00
Byte 7: XOR(Byte0..Byte6)
```

Known frames:

```text
Bluetooth selected:      46 54 01 00 00 00 00 13
Bluetooth not selected:  46 54 00 00 00 00 00 12
```

`FT` is sent on change and approximately every 250ms. `FP` is sent on input or
lock change and otherwise approximately every 10ms while Bluetooth is selected.

## 4. RP2350B behavior

The RP2350 gameplay gate has `UNLOCKED`, `CAPTURED`, and
`DRAIN_UNTIL_RELEASE` phases. The exported lock predicate is true for both
`CAPTURED` and `DRAIN_UNTIL_RELEASE`.

- Long-pressing GP30 enters the menu and asserts lock in that Core0 loop.
- USB receives neutral game input while lock is true.
- Bluetooth `FP` retains the real processed input and sets Byte4 bit7.
- Closing the menu starts RP2350 drain. Lock remains true until raw and
  debounced GP2..GP20 are all released continuously for 30ms.
- GP30/GP31/GP32 are outside the gameplay mask and remain menu controls.

The receiver must still implement its own drain. That makes packet loss,
startup, transport changes, and version skew fail neutral instead of allowing a
held exit input through.

## 5. Required ESP32-C6 state machine

The C6 receiver owns three states:

```text
UNLOCKED  normal payload may reach BLE HID
LOCKED    RP2350 lock bit is set; BLE HID is neutral
DRAIN     BLE HID remains neutral until one unlocked neutral FP is accepted
```

Initialize in `DRAIN`, with neutral HID desired.

### 5.1 Valid `FP` transitions

Process a frame only after magic, type, length, and XOR validation.

1. If `gameplay_locked == true`:
   - enter `LOCKED` from any state;
   - immediately publish neutral HID if the current desired HID report is not
     already neutral;
   - never forward this frame's buttons, D-pad, or axes to BLE HID.
   - do not pass this frame's physical payload to `note_real_input()` or any
     equivalent input/activity path.
2. If `gameplay_locked == false` while state is `LOCKED`:
   - enter `DRAIN` before considering the physical payload.
3. In `DRAIN`:
   - keep BLE HID neutral;
   - accept drain completion only when the same valid unlocked frame has
     `buttons == 0`, `dpad == 0`, `lx == 0`, and `ly == 0`;
   - after accepting that neutral frame, enter `UNLOCKED`; the accepted frame
     itself remains a neutral HID result.
4. In `UNLOCKED` with an unlocked frame:
   - convert and submit the real payload normally;
   - this is the only gate state in which the payload may update the existing
     real-input/activity tracker.

Do not replay a payload remembered before or during lock. A user must release
and press again after drain to create a new gameplay action.

### 5.2 Reference receiver logic

```c
typedef enum {
    GAMEPLAY_UNLOCKED,
    GAMEPLAY_LOCKED,
    GAMEPLAY_DRAIN,
} gameplay_gate_t;

static gameplay_gate_t gate = GAMEPLAY_DRAIN;

static void handle_valid_fp(const fp_report_t *fp) {
    bool neutral = fp->buttons == 0 && fp->dpad == 0 &&
                   fp->lx == 0 && fp->ly == 0;

    if (fp->gameplay_locked) {
        gate = GAMEPLAY_LOCKED;
        clear_input_press_state();
        set_desired_hid_neutral();
        return;
    }

    if (gate == GAMEPLAY_LOCKED) {
        gate = GAMEPLAY_DRAIN;
        clear_input_press_state();
    }

    if (gate == GAMEPLAY_DRAIN) {
        set_desired_hid_neutral();
        if (neutral) {
            gate = GAMEPLAY_UNLOCKED;
            clear_input_press_state();
        }
        return;
    }

    note_real_input(input_activity_mask(fp->buttons, fp->dpad, fp->lx, fp->ly));
    submit_gameplay_hid(fp);
}
```

`set_desired_hid_neutral()` may deduplicate identical BLE transmissions, but
the receiver's stored desired HID state must become neutral immediately.

## 6. Existing real-input/activity handling

The C6 project already separates the desired HID report from its
`note_real_input()` activity tracker. Preserve that separation as follows:

- Only an unlocked `FP` processed while the gate is `UNLOCKED` may call
  `note_real_input(input_activity_mask(...))`.
- `LOCKED` and `DRAIN` payloads never count as real input/activity, even though
  their UART bytes still contain the physical buttons, D-pad, and axes.
- The transition into `LOCKED` is not an activity event.
- Clear the existing press-edge state when entering `LOCKED` or `DRAIN`, so a
  hidden held control cannot be remembered or replayed.
- The unlocked neutral frame that completes drain is consumed as neutral and
  does not count as activity. A later new press behaves normally.
- `FT`, `FB`, bad checksums, and arbitrary UART bytes remain non-input events.

Do not add a second sleep/activity policy for this feature. Keep the existing
axis dead zone and inactivity behavior for normal `UNLOCKED` gameplay only.

## 7. `FT`, existing stale timeout, and fail-neutral behavior

Keep the current ESP32-C6 timeout instead of introducing new FP/FT timers:

```c
#define UART_INPUT_STALE_MS 250
```

Required integration with the existing receiver:

- On a valid `FT` with Byte2=`0x00`, `set_transport_bt_enabled(false)` keeps its
  current link-handling behavior, publishes neutral HID, clears press-edge
  state, and also puts the gameplay gate in `DRAIN`.
- On a transition to valid `FT` Byte2=`0x01`, enter `DRAIN`. `FT` alone never
  unlocks gameplay; a valid unlocked neutral `FP` must complete drain.
- Repeated valid `FT` Byte2=`0x01` while already selected does not disturb the
  current `LOCKED`/`UNLOCKED`/`DRAIN` state.
- Extend the existing `neutralize_stale_uart_report()` path so a 250ms stale
  report restores neutral HID, clears press-edge state, and enters `DRAIN`.
  The first recovered active frame is therefore suppressed.
- This S24 change does not require a separate `FT` timeout.
- A corrupt or partial frame never changes the gameplay gate, never completes
  drain, and never refreshes the existing valid-report timestamp.

The RP2350 sends this exact unlocked neutral `FP` once when switching away from
Bluetooth:

```text
46 50 00 00 00 00 00 16
```

It is useful redundancy, but the valid `FT` transport state remains the
authority for whether Bluetooth gameplay forwarding is allowed.

## 8. GP19 and GP20 edge cases

### GP19

GP19 is both the RP2350 menu BACK input and gameplay button A2/button bit13.
On the menu's MAIN page, the press that exits the menu must never appear as a
Bluetooth game action. Normally the RP2350 keeps bit7 set through its 30ms
release drain. The C6 `DRAIN` state is still mandatory: if an unlocked GP19/A2
frame is received after lock clears, keep HID neutral until an unlocked neutral
frame arrives.

### GP20

GP20 is the Turbo control input and uses button bit14. It is not the protocol
lock bit. While Byte4 bit7 is set, a GP20 bit in the physical payload is
raw payload data only: it must not reach BLE HID, update input/activity, or
mutate a C6-side gameplay state. Button bit15 remains reserved.

## 9. Compatibility

- Old RP2350 firmware sends Byte4 bit7 as zero. The updated C6 starts in
  `DRAIN`, consumes the periodic unlocked neutral `FP`, then behaves normally.
- The current old C6 receiver's hat conversion ignores Byte4 high bits, so the
  new flag does not create a phantom direction. It still does not implement
  gameplay suppression and must be updated before claiming menu lock support.
- A receiver that interprets all of Byte4 as a D-pad value is incorrect even if
  it appeared to work before this extension.
- No parser may infer lock from button bit15, a neutral payload, GP19, GP20, or
  the absence of input. Only Byte4 bit7 is the lock signal.

## 10. Known test frames

```text
Unlocked neutral:       46 50 00 00 00 00 00 16
Locked neutral:         46 50 00 00 80 00 00 96
Unlocked B1:            46 50 01 00 00 00 00 17
Locked B1:              46 50 01 00 80 00 00 97
Locked GP19/A2:         46 50 00 20 80 00 00 B6
Unlocked GP19/A2:       46 50 00 20 00 00 00 36
Locked GP20/Turbo:      46 50 00 40 80 00 00 D6
```

Required sequence tests in the ESP32-C6 repository:

1. Boot -> `FT=BT` -> unlocked neutral: `DRAIN -> UNLOCKED`, HID neutral.
2. Unlocked B1: B1 reaches HID.
3. Locked B1: transition to `LOCKED`, immediate neutral HID, B1 suppressed.
4. Repeated locked active/neutral frames: HID remains neutral and the frames do
   not update the real-input/activity timer.
5. Lock clears with GP19/A2 held: enter/remain `DRAIN`; A2 never reaches HID.
6. An unlocked neutral frame completes drain; only a later new press is sent.
7. Locked GP20 never reaches HID and never uses button bit15.
8. Existing 250ms UART input staleness forces neutral and drain.
9. `FT=USB` forces neutral; a later `FT=BT` still requires drain completion.
10. A bad-checksum locked or neutral frame cannot lock, unlock, or drain.
11. Verify Byte4 lock transitions cause immediate RP report transmission even
    when less than 10ms has elapsed since the previous report.

## 11. ESP32-C6 implementation checklist

Current receiving-side landmarks in `main/main.c`:

- `UART_INPUT_STALE_MS` is already `250`.
- `neutral_report` is the authoritative five-byte neutral BLE HID report.
- `handle_uart_frame()` validates and dispatches `FP` and `FT`; split
  `frame[4]` into lock bit and D-pad low nibble here.
- `set_current_report()` updates the desired report and valid-report timestamp.
- `neutralize_stale_uart_report()` is the existing fail-neutral timeout path.
- `input_activity_mask()` plus `note_real_input()` are the existing activity
  path and must only receive `UNLOCKED` gameplay.
- `report_task()` sends the stored desired report and can remain the final BLE
  transmitter once every writer obeys the gate.

- [ ] Find the one UART parser that validates and dispatches `FP` and `FT`.
- [ ] Decode Byte4 bit7 separately from the D-pad low nibble.
- [ ] Add one authoritative `UNLOCKED`/`LOCKED`/`DRAIN` state.
- [ ] Route every BLE HID submit through that state.
- [ ] Publish neutral HID immediately on lock, transport loss, or stale input.
- [ ] Require an exact unlocked neutral `FP` before leaving `DRAIN`.
- [ ] Call the existing real-input tracker only for `UNLOCKED` gameplay.
- [ ] Extend the existing 250ms stale path to force `DRAIN`; do not add an FT timer.
- [ ] Preserve the current frame length, checksum, button bits, and timing.
- [ ] Add all sequence tests from Section 10.
- [ ] Confirm GP19 exit and GP20 Turbo never leak to BLE HID.
