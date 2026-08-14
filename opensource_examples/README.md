# ESP32-C6 BLE gamepad reference projects

These repositories were cloned as isolated references. No existing firmware
source under `main/` was changed while collecting them.

## 1. ESP32-C6 hardware project: generic BLE gamepad

- Local directory: `esp32c6-tandy-ble-gamepad`
- Upstream: <https://github.com/CoCo-Synthesis/Tandy-Deluxe-Bluetooth-Joystick>
- Pinned clone commit: `29406a749a67eec72817a1aa026b2028046d98ac`
- Framework: Arduino-ESP32
- Target documented by the project: DFRobot FireBeetle 2 ESP32-C6
- BLE library: `lemmingDev/ESP32-BLE-Gamepad` + NimBLE-Arduino
- Output type: generic HID gamepad, not Xbox/PlayStation/Switch emulation

The sketch sets VID `0xE502` and PID `0xABCD`. It is useful for checking whether
an ESP32-C6 can enumerate as a generic BLE gamepad, but it is not expected to
make a browser identify the device as an Xbox controller.

Risk: NimBLE-Arduino issue 1146 reports immediate disconnects on the same
FireBeetle 2 ESP32-C6 with Arduino core 3.3.8 and NimBLE-Arduino 2.5.0:
<https://github.com/h2zero/NimBLE-Arduino/issues/1146>

## 2. Xbox and DualSense BLE emulation library

- Local directory: `esp32-ble-compositehid-xbox-dualsense`
- Upstream: <https://github.com/Mystfit/ESP32-BLE-CompositeHID>
- Pinned clone commit: `06d93eab499181afaa3e26f96ecee67233c01303`
- Framework: Arduino-ESP32
- BLE library: NimBLE-Arduino + Callback
- Modes: generic gamepad, Xbox One S, Xbox Series X, DualSense Edge

The Xbox implementation is not a name-only change. It uses HID gamepad report
descriptors and configures Microsoft VID `0x045E` with PID `0x02FD` (Xbox One S)
or PID `0x0B13` (Xbox Series X). The relevant ready-made sketch is:

`examples/XInputExamples/XboxXInputController/XboxXInputController.ino`

The DualSense implementation configures Sony VID `0x054C` and PID `0x0DF2`.

Risk: upstream CI compiles only the generic `esp32:esp32:esp32` Arduino target;
it does not currently prove ESP32-C6 compatibility. Treat C6 support as an
experiment until it compiles and passes pairing, reconnect, and Windows
Gamepad API tests on real hardware.

## Native ESP-IDF finding

ESP-IDF release/v5.2 and current Espressif examples do not contain an official
BLE gamepad/joystick device example. Their BLE HID device examples implement a
media remote, keyboard, or mouse.

Joypad OS contains a substantial Xbox BLE HID implementation, including Xbox
reports, advertising, pairing, and rumble parsing:
<https://github.com/joypad-ai/joypad-os/tree/main/src/bt/ble_output>

However, its complete ESP-IDF application currently targets ESP32-S3 and also
builds USB-OTG paths unavailable on ESP32-C6. It is source material for a port,
not a ready-to-flash C6 baseline.

## Recommended test order

1. For a quick generic C6 BLE gamepad test, build the Tandy project.
2. For the actual browser/Windows Xbox identification goal, build the
   `XboxOneSControllerDeviceConfiguration` example from CompositeHID first.
3. Before each descriptor/VID/PID retest, remove the old Bluetooth pairing in
   Windows and fully restart the browser so cached HID metadata is not reused.

No Arduino CLI, Arduino ESP32 board package, or PlatformIO executable was found
in the standard local install paths during this audit, so neither Arduino
project was compiled in this pass.
