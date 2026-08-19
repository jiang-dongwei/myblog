# GP2040 Configuration for the Fightpad 12 Slim (RP2350B)

GP2040-CE build target for the **Fightpad 12 Slim** PCB, based on the Raspberry Pi **RP2350B** (QFN-80, 48 GPIOs). The current diagnostic firmware scope is the wired controller path, 19 buttons, OLED button viewer, the GP22 button LED chain, PIO-USB expansion, the GP23 status LED, GP30-GP32 raw ambient-control input dots on the OLED, and a runtime UART feed to the ESP32-C6 BLE HID firmware.

The latest hardware references are `22-FIGHTPAD_schematic.pdf` and `fightpad12slim_rp2350b.xlsx` in the workspace root. They also describe follow-up hardware for ESP32-C6 update flow, a transport switch, and battery/power management; those remain reserved unless noted below.

## Build

From the repo root:

```bash
PICO_SDK_PATH=/path/to/pico-sdk \
GP2040_BOARDCONFIG=Fightpad12Slim \
SKIP_WEBBUILD=TRUE \
  cmake -B build -DCMAKE_BUILD_TYPE=Release

GP2040_BOARDCONFIG=Fightpad12Slim \
  cmake --build build --config Release --parallel
```

Output: `build/GP2040-CE_<version>_Fightpad12Slim.uf2`.

## Flash

Enter RP2350 BOOTSEL mode using the board recovery path/test pad, then copy the `.uf2` to the mounted RP2350 mass-storage volume.

## Boot Hotkeys

| Hold while plugging USB | Effect |
| --- | --- |
| `S2` (START) | Enter bootloader / configured GP2040-CE boot behavior |
| `S1` + `S2` | Enter GP2040-CE Web Configurator mode |
| `R1` (RB) | Boot into Xbox One input mode (`DEFAULT_INPUT_MODE_R1`) |
| `B4` (Y / Triangle) | Boot into PS5 input mode (`DEFAULT_INPUT_MODE_B4`, USB auth) |

## Pin Mapping

| GPIO | Function | GP2040-CE assignment | Notes |
| --- | --- | --- | --- |
| GP00 | I2C0 SDA - OLED | `ASSIGNED_TO_ADDON` + `I2C0_PIN_SDA = 0` | Display |
| GP01 | I2C0 SCL - OLED | `ASSIGNED_TO_ADDON` + `I2C0_PIN_SCL = 1` | Display |
| GP02 | UP | `BUTTON_PRESS_UP` | Button |
| GP03 | DOWN | `BUTTON_PRESS_DOWN` | Button |
| GP04 | LEFT | `BUTTON_PRESS_LEFT` | Button |
| GP05 | RIGHT | `BUTTON_PRESS_RIGHT` | Button |
| GP06 | A / Cross | `BUTTON_PRESS_B1` | Button |
| GP07 | B / Circle | `BUTTON_PRESS_B2` | Button |
| GP08 | X / Square | `BUTTON_PRESS_B3` | Button |
| GP09 | Y / Triangle | `BUTTON_PRESS_B4` | Button |
| GP10 | L1 | `BUTTON_PRESS_L1` | Button |
| GP11 | L2 | `BUTTON_PRESS_L2` | Button |
| GP12 | R1 | `BUTTON_PRESS_R1` | Button |
| GP13 | R2 | `BUTTON_PRESS_R2` | Button |
| GP14 | L3 | `BUTTON_PRESS_L3` | Button |
| GP15 | R3 | `BUTTON_PRESS_R3` | Button |
| GP16 | SELECT | `BUTTON_PRESS_S1` | Button |
| GP17 | START | `BUTTON_PRESS_S2` | Button |
| GP18 | HOME | `BUTTON_PRESS_A1` | Button |
| GP19 | BACK / Share | `BUTTON_PRESS_A2` | Button |
| GP20 | TURBO | `BUTTON_PRESS_TURBO` | Button |
| GP21 | VBUS-present sense | `ASSIGNED_TO_ADDON` | Reported in the ESP32 battery frame as external-power status |
| GP22 | Button WS2812 data | `ASSIGNED_TO_ADDON` + `BOARD_LEDS_PIN = 22` | Workbook says 12 LEDs |
| GP23 | Power / status LED | `BOARD_LED_PIN = 23`, `BOARD_LED_ENABLED = 1` | Confirm on hardware |
| GP24 | 5V RGB-rail boost enable | `ASSIGNED_TO_ADDON` + ambient LED addon | High while RGB output is active; low after a final black frame when RGB is off |
| GP25 | BQ27220 software-I2C SCL | `ASSIGNED_TO_ADDON` + BQ27220 | Battery gauge clock |
| GP26 | BQ27220 software-I2C SDA | `ASSIGNED_TO_ADDON` + BQ27220 | Battery gauge data |
| GP27 | BQ27220 GPOUT | `ASSIGNED_TO_ADDON` + BQ27220 | Battery gauge status input |
| GP28 | USB expansion D+ | `ASSIGNED_TO_ADDON` + `USB_PERIPHERAL_PIN_DPLUS = 28` | PIO-USB host |
| GP29 | USB expansion D- | `ASSIGNED_TO_ADDON` | Implicit pair to GP28 |
| GP30 | Ambient LED on/off | `ASSIGNED_TO_ADDON` + diagnostic input | Upper-left OLED dot; active-low |
| GP31 | Ambient effect previous | `ASSIGNED_TO_ADDON` + diagnostic input | Upper-left OLED dot; active-low |
| GP32 | Ambient effect next | `ASSIGNED_TO_ADDON` + diagnostic input | Upper-left OLED dot; active-low |
| GP33 | HID transport switch | `ASSIGNED_TO_ADDON` + `FIGHTPAD12SLIM_TRANSPORT_SEL_PIN = 33` | Low = USB-HID, high = BT-HID; runtime changes require 30 ms stable input |
| GP34 | ESP32-C6 EN / RESET | `ASSIGNED_TO_ADDON` | Low in USB mode; high in BT mode to enable the ESP32-C6 |
| GP35 | ESP32-C6 boot strap | `ASSIGNED_TO_ADDON` | Reserved; released by runtime firmware so the boot button can pull it low |
| GP36 | UART1 TX to ESP32-C6 RXD | `ASSIGNED_TO_ADDON` | Follow-up BT HCI |
| GP37 | UART1 RX from ESP32-C6 TXD | `ASSIGNED_TO_ADDON` | Follow-up BT HCI |
| GP38 | UART1 CTS from ESP32-C6 RTS | `ASSIGNED_TO_ADDON` | Follow-up BT HCI |
| GP39 | UART1 RTS to ESP32-C6 CTS | `ASSIGNED_TO_ADDON` | Follow-up BT HCI |
| GP40 | Ambient WS2812 data | `ASSIGNED_TO_ADDON` + Fightpad ambient LED addon | Active 19-LED chain on PIO2/SM0 |
| GP41 | Battery voltage sense | `ASSIGNED_TO_ADDON` | Sampled by the ESP32 proxy battery path through the schematic 100k/100k divider |
| GP42-GP43 | Battery telemetry UART1 TX/RX | `ASSIGNED_TO_ADDON` + BQ27220 log | 115200 8N1; RP2350 AUX UART function |
| GP44 | UART0 TX to ESP32-C6 RXD | `ASSIGNED_TO_ADDON` + ESP32 proxy | Runtime BT-HID input frame output |
| GP45 | UART0 RX from ESP32-C6 TXD | `ASSIGNED_TO_ADDON` + ESP32 proxy | Reserved for ESP logs/replies |
| GP46-GP47 | Free / ADC-capable | `ASSIGNED_TO_ADDON` | Locked in v1 until high-GPIO mapping is audited |

## OLED Button Viewer

`BoardConfig.h` uses `BUTTON_LAYOUT_BOARD_DEFINED_A/B` and defines `DEFAULT_BOARD_LAYOUT_A/B` so the OLED can draw the Fightpad 12 Slim's physical button circles. The viewer elements are `GP_ELEMENT_PIN_BUTTON` entries bound directly to GPIO pins; this keeps the first hardware test simple because each circle should fill when its physical switch pulls that GPIO low.

## Ambient LED Controls

The Fightpad-specific ambient LED addon owns both WS2812 outputs: the 19-LED GP40 ambient chain on PIO2/SM0 and the 12-LED GP22 button chain on PIO2/SM1. It is also the sole writer of the active-high GP24 RGB-rail boost enable. `Turn Lights Off` first sends black to both chains, waits 1 ms for the PIO data and WS2812 latch interval, then drives GP24 low. Its independent persisted master switch preserves the selected effect, colors, Button Flash, brightness, and GP30 normal-effect state; the same row becomes `Turn Lights On` and restores those values. The master switch also suppresses temporary Bluetooth status output, while GP30 alone continues to gate only normal effects. Restoring raises GP24, waits 5 ms for the rail to settle, and sends the first restored frame. GP30-GP32 remain the scrollwheel/menu controls; the old ambient control diagnostic paths are disabled.

## ESP32-C6 BLE HID Feed

`FightpadESP32ProxyAddon` is enabled for the Fightpad build without adding a USB CDC interface. It initializes RP2350 UART0 on GP44 TX / GP45 RX at 115200 8N1 and talks to the ESP32-C6 firmware in `esp32c6_ble_hid_gamepad_test` with compact 8-byte frames:

- `FP`: buttons low/high, D-pad bitmask, signed X/Y axes, and an XOR checksum. Input frames are sent every 10 ms, or sooner when the input state changes, only while GP33 selects BT-HID.
- `FT`: runtime BT transport enable state from GP33.
- `FB`: battery status from RP2350, carrying percent, VBUS-present state, raw ADC sample, and checksum. Percent comes from the same BQ27220 SOC snapshot shown on the OLED; GP21 and GP41 remain supplemental power/ADC diagnostics. The ESP32-C6 uses the percent field for its BLE HID battery level characteristic.

GP33 is the runtime transport switch. Low selects USB-HID and stops the ESP input feed after sending one neutral frame. High selects BT-HID and software-disconnects the RP2350 TinyUSB device, so a USB cable may still provide power/charging without exposing a second neutral controller to the host. A runtime USB-to-BT transition first gives the existing neutral-report path up to 20 ms to release stale buttons, then removes the USB device; switching back to USB reconnects and re-enumerates the configured USB controller. Web Config always keeps TinyUSB connected, and the BOOTSEL ROM path is unchanged. This polarity follows the assembled switch's observed physical positions. The boot state is accepted immediately; later switch changes must remain stable for 30 ms before the USB report path, ESP input feed, GP34 enable output, and USB attachment state change modes. During the 3-second OLED splash, Bluetooth status frames are still received and may wake the display, but their temporary text overlay is suppressed so the startup logo remains uninterrupted.

The runtime build drives GP34 as the ESP32-C6 enable signal and leaves GP35 for the physical ESP32-C6 BOOT button. The configurable proxy reset/boot pins remain disabled, so CDC DTR/RTS handling does not take ownership of either net.

## BQ27220 Battery Snapshot UART

The BQ27220 addon samples the gauge every 2 seconds and sends one compact line on RP2350 UART1 at 115200 8N1 every 4 seconds. The line contains only `SOC`, voltage, instantaneous current, and full-charge capacity, for example `SOC:59% V:3868mV I:-305mA FCC:650mAh`. An invalid field is reported as `NA` instead of reusing stale data.

Connect GP42 (RP2350 TX) to the RX input of a 3.3 V TTL USB-UART adapter and connect grounds. GP43 is initialized as RP2350 RX but the current diagnostic has no receive command parser, so it may be left disconnected. Do not connect these pins directly to RS-232 levels or a 5 V UART.

The first line follows the configured BQ27220 boot delay, then output uses `FIGHTPAD12SLIM_BQ27220_LOG_INTERVAL_MS=4000` while gauge polling remains at `FIGHTPAD12SLIM_BQ27220_POLL_INTERVAL_MS=2000`. UART writes have a bounded per-byte wait; a timeout aborts the current line and forces UART1 to be initialized again before the next period instead of blocking the battery task indefinitely.

## OLED Battery Gauge and Idle Sleep

The BUTTONS page keeps the normal button viewer and shows the BQ27220 numeric SOC immediately to the left of the four-cell battery icon in the upper-right corner. It does not overlay voltage, current, or FCC diagnostics. The four Battery Info debug pages remain compiled, but their level-0 entry is hidden by `SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED=0`; set it to `1` to restore the entry for gauge debugging.

Core0 records activity from every debounced GP2-GP20 gameplay key and every raw edge on GP30, GP31, or GP32. When no such activity has been seen for 60 seconds, the Core1 display addon powers off the SSD1306 and skips further frame rendering. Pressing any gameplay key or operating GP30/GP31/GP32 powers the OLED back on immediately without consuming the input; Bluetooth status transitions retain their existing wake behavior.

## BQ27220 Low-Battery LED Cutoff

After a valid BQ27220 SOC sample reaches 7% or lower, the Fightpad LED owner forces both WS2812 frames to black: all 12 button LEDs on GP22 and all 19 ambient LEDs on GP40. The cutoff is a render-only override, so RGB menu colors, effects, Key Flash settings, and stored configuration are not changed. A later valid SOC sample above 7% automatically restores the currently selected effects.

The cutoff state is published atomically from the Core1 BQ27220 sampler to the Core0 LED renderer. A failed SOC read retains the previous cutoff state, preventing a transient I2C error from relighting a low battery. Before the first valid SOC sample after boot, the firmware does not guess the battery level, so a unit that boots already at or below 7% can remain lit until that first sample completes.

This feature uses the same final-black-frame sequence and then disables the GP24 LED-rail boost supply. When a valid SOC sample rises above the cutoff, GP24 is enabled and the current effect resumes automatically. The separate GP23 status LED is not affected.

## Known Limitations

- `Fightpad12Slim.cmake` uses the local `fightpad12slim.h` board header for RP2350B, GP23 status LED, and W25Q128JVSI 16 MiB flash metadata. Confirm with `picotool info` on hardware.
- The `22-FIGHTPAD_20260625-schematic_new.pdf` schematic shows GPIO24/`5V_EN` driving only the FP6276 `VCC_5V` boost rail used by the RGB chains; RP2350 and ESP32 use the independent 3.3V rail. Hardware validation should still confirm that GP22/GP40 stay low while the 5V rail is off and that no board revision routes another load from `VCC_5V`.
- The workbook Pinout sheet now assigns `VBAT_SENSE_PIN = 41`, labels GP41 as ADC1, and describes VBAT routed to GP41. Stale workbook text still marks GP27 as ADC1 and mentions GP41 BT pairing. The schematic/RP2350B pinout is treated as authoritative: GP41 / ADC1 is retained as raw diagnostic telemetry, while BQ27220 SOC is the trusted percentage source.
- `LEDS_BUTTON_*` and ambient LED ordering are intentionally not finalized. The workbook says 18 ambient LEDs, while the schematic labels extend through `LED_19`; confirm physical count and chain order on hardware.
- ESP32-C6 update flow through RP2350 is still follow-up work. BLE battery UI now uses the BQ27220 SOC shared with the OLED; GP41 ADC sampling and GP21 VBUS detection are forwarded only as supplemental runtime telemetry.
- The battery snapshot logger owns UART1. The planned GP36-GP39 ESP32-C6 BT HCI link cannot be enabled at the same time without moving one function to another UART or PIO implementation.

## References

- PRD: `PRD_Fightpad12Slim.md` in the workspace root.
- Latest schematic: `22-FIGHTPAD_schematic.pdf`.
- Latest pin workbook: `fightpad12slim_rp2350b.xlsx`.
- Reference targets: `configs/Haute42COSMOXMLite/` and `configs/SparkFunProMicroRP2350/`.
