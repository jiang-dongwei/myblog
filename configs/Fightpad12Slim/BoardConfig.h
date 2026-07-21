/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2026 OpenStickCommunity (gp2040-ce.info)
 *
 * Fightpad 12 Slim (RP2350B) board configuration.
 * Pin layout sourced from PRD_Fightpad12Slim.md,
 * 22-FIGHTPAD_schematic.pdf, and fightpad12slim_rp2350b.xlsx.
 * Derived from configs/Haute42COSMOXMLite/BoardConfig.h; retargeted to RP2350B.
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "enums.pb.h"
#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "Fightpad 12 Slim (RP2350B)"

// Main pin mapping Configuration
//                          // GP2040 | Xinput | Switch  | PS3/4/5  | Dinput | Arcade |
#define GPIO_PIN_02 GpioAction::BUTTON_PRESS_UP     // UP     | UP     | UP      | UP       | UP     | UP     |
#define GPIO_PIN_03 GpioAction::BUTTON_PRESS_DOWN   // DOWN   | DOWN   | DOWN    | DOWN     | DOWN   | DOWN   |
#define GPIO_PIN_04 GpioAction::BUTTON_PRESS_LEFT   // LEFT   | LEFT   | LEFT    | LEFT     | LEFT   | LEFT   |
#define GPIO_PIN_05 GpioAction::BUTTON_PRESS_RIGHT  // RIGHT  | RIGHT  | RIGHT   | RIGHT    | RIGHT  | RIGHT  |
#define GPIO_PIN_06 GpioAction::BUTTON_PRESS_B1     // B1     | A      | B       | Cross    | 2      | K1     |
#define GPIO_PIN_07 GpioAction::BUTTON_PRESS_B2     // B2     | B      | A       | Circle   | 3      | K2     |
#define GPIO_PIN_08 GpioAction::BUTTON_PRESS_B3     // B3     | X      | Y       | Square   | 1      | P1     |
#define GPIO_PIN_09 GpioAction::BUTTON_PRESS_B4     // B4     | Y      | X       | Triangle | 4      | P2     |
#define GPIO_PIN_10 GpioAction::BUTTON_PRESS_L1     // L1     | LB     | L       | L1       | 5      | P4     |
#define GPIO_PIN_11 GpioAction::BUTTON_PRESS_L2     // L2     | LT     | ZL      | L2       | 7      | K4     |
#define GPIO_PIN_12 GpioAction::BUTTON_PRESS_R1     // R1     | RB     | R       | R1       | 6      | P3     |
#define GPIO_PIN_13 GpioAction::BUTTON_PRESS_R2     // R2     | RT     | ZR      | R2       | 8      | K3     |
#define GPIO_PIN_14 GpioAction::BUTTON_PRESS_L3     // L3     | LS     | LS      | L3       | 11     | LS     |
#define GPIO_PIN_15 GpioAction::BUTTON_PRESS_R3     // R3     | RS     | RS      | R3       | 12     | RS     |
#define GPIO_PIN_16 GpioAction::BUTTON_PRESS_S1     // S1     | Back   | Minus   | Select   | 9      | Coin   |
#define GPIO_PIN_17 GpioAction::BUTTON_PRESS_S2     // S2     | Start  | Plus    | Start    | 10     | Start  |
#define GPIO_PIN_18 GpioAction::BUTTON_PRESS_A1     // A1     | Guide  | Home    | PS       | 13     | ~      |
#define GPIO_PIN_19 GpioAction::BUTTON_PRESS_A2     // A2     | ~      | Capture | ~        | 14     | ~      |
#define GPIO_PIN_20 GpioAction::BUTTON_PRESS_TURBO  // TURBO

// Setting GPIO pins to assigned by add-on / out-of-scope hardware.
// These are declared so the GPIO scan engine does NOT mis-read them as buttons.
// In-scope add-ons (firmware acts on them):
//   GP00/GP01 -> I2C0 (OLED)
//   GP22      -> WS2812 per-key RGB chain (LED1)
//   GP23      -> on-board status LED
//   GP24      -> 5V boost enable for LED rail bring-up
//   GP28/GP29 -> PIO-USB expansion host
//   GP30..32  -> ambient-light controls
//   GP40      -> WS2812 ambient/border RGB chain (LED_1)
// Reserved / follow-up hardware:
//   GP21      -> VBUS-present status reported in the ESP32 battery frame
//   GP25..27  -> BQ27220 battery gauge: SCL/SDA/GPOUT
//   GP33      -> USB/BT HID transport switch
//   GP34/GP35 -> ESP32-C6 optional EN/boot control
//   GP41      -> VBAT divider (RP2350B ADC1), retained as raw ESP32-frame diagnostics
//   GP42/GP43 -> BQ27220 4-second battery telemetry UART1 TX/RX
//   GP44/GP45 -> ESP32-C6 UART0 TX/RX bridge pins
//   GP46/GP47 -> currently free, but locked in v1 until high-GPIO mapping is audited
#define GPIO_PIN_00 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_01 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_21 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_22 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_23 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_24 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_25 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_26 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_27 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_28 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_29 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_30 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_31 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_32 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_33 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_34 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_35 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_36 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_37 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_38 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_39 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_40 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_41 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_42 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_43 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_44 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_45 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_46 GpioAction::ASSIGNED_TO_ADDON
#define GPIO_PIN_47 GpioAction::ASSIGNED_TO_ADDON

// Keyboard HID Mapping Configuration
// Verbatim copy from configs/Haute42COSMOXMLite/BoardConfig.h
// (PRD section 5.1.b: "Keep the keyboard-mapping block identical to the Haute42COSMOXMLite reference unless there is a reason to diverge.")
//                                            // GP2040 | Xinput | Switch  | PS3/4/5  | Dinput | Arcade |
#define KEY_DPAD_UP     HID_KEY_ARROW_UP      // UP     | UP     | UP      | UP       | UP     | UP     |
#define KEY_DPAD_DOWN   HID_KEY_ARROW_DOWN    // DOWN   | DOWN   | DOWN    | DOWN     | DOWN   | DOWN   |
#define KEY_DPAD_RIGHT  HID_KEY_ARROW_RIGHT   // RIGHT  | RIGHT  | RIGHT   | RIGHT    | RIGHT  | RIGHT  |
#define KEY_DPAD_LEFT   HID_KEY_ARROW_LEFT    // LEFT   | LEFT   | LEFT    | LEFT     | LEFT   | LEFT   |
#define KEY_BUTTON_B1   HID_KEY_SHIFT_LEFT    // B1     | A      | B       | Cross    | 2      | K1     |
#define KEY_BUTTON_B2   HID_KEY_Z             // B2     | B      | A       | Circle   | 3      | K2     |
#define KEY_BUTTON_R2   HID_KEY_X             // R2     | RT     | ZR      | R2       | 8      | K3     |
#define KEY_BUTTON_L2   HID_KEY_V             // L2     | LT     | ZL      | L2       | 7      | K4     |
#define KEY_BUTTON_B3   HID_KEY_CONTROL_LEFT  // B3     | X      | Y       | Square   | 1      | P1     |
#define KEY_BUTTON_B4   HID_KEY_ALT_LEFT      // B4     | Y      | X       | Triangle | 4      | P2     |
#define KEY_BUTTON_R1   HID_KEY_SPACE         // R1     | RB     | R       | R1       | 6      | P3     |
#define KEY_BUTTON_L1   HID_KEY_C             // L1     | LB     | L       | L1       | 5      | P4     |
#define KEY_BUTTON_S1   HID_KEY_5             // S1     | Back   | Minus   | Select   | 9      | Coin   |
#define KEY_BUTTON_S2   HID_KEY_1             // S2     | Start  | Plus    | Start    | 10     | Start  |
#define KEY_BUTTON_L3   HID_KEY_EQUAL         // L3     | LS     | LS      | L3       | 11     | LS     |
#define KEY_BUTTON_R3   HID_KEY_MINUS         // R3     | RS     | RS      | R3       | 12     | RS     |
#define KEY_BUTTON_A1   HID_KEY_9             // A1     | Guide  | Home    | PS       | 13     | ~      |
#define KEY_BUTTON_A2   HID_KEY_F2            // A2     | ~      | Capture | ~        | 14     | ~      |
#define KEY_BUTTON_FN   -1                    // Hotkey Function                                        |

// USB passthrough (PIO-USB host on expansion D+/D-)
#define USB_PERIPHERAL_ENABLED 1
#define USB_PERIPHERAL_PIN_DPLUS 28
#define USB_PERIPHERAL_PIN_ORDER 0

#define DEFAULT_INPUT_MODE_R1 INPUT_MODE_XBONE
#define DEFAULT_INPUT_MODE_B4 INPUT_MODE_PS5
#define DEFAULT_PS5AUTHENTICATION_TYPE INPUT_MODE_AUTH_TYPE_USB

// Turbo
#define TURBO_ENABLED 1

// WS2812 RGB chain
#define BOARD_LEDS_PIN 22
#define LED_BRIGHTNESS_MAXIMUM 200
#define LEDS_BRIGHTNESS 200
#define LED_BRIGHTNESS_STEPS 5
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_PIXEL 1
#define LEDS_BASE_ANIMATION_INDEX 1
#define LEDS_BUTTON_COLOR_INDEX 1
#define LEDS_DPAD_LEFT   0
#define LEDS_DPAD_DOWN   1
#define LEDS_DPAD_RIGHT  2
#define LEDS_BUTTON_B3   3
#define LEDS_BUTTON_B4   4
#define LEDS_BUTTON_R1   5
#define LEDS_BUTTON_L1   6
#define LEDS_BUTTON_L2   7
#define LEDS_BUTTON_R2   8
#define LEDS_BUTTON_B2   9
#define LEDS_BUTTON_B1   10
#define LEDS_DPAD_UP     11
#define FIGHTPAD12SLIM_BUTTON_LEDS_COUNT 12
#define FIGHTPAD12SLIM_BUTTON_LEDS_FORCE_BOARD_DEFAULTS 1
#define FIGHTPAD12SLIM_AMBIENT_OWNS_GP22 1

// Ambient WS2812 RGB chain on GP40, mirrored to the GP22 LED chain for matched top/bottom effects.
#define FIGHTPAD12SLIM_AMBIENT_ENABLED 1
#define FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN 40
#define FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT 19
#define FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT 12
#define FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN 30      // DIP switch: enable/disable ambient LEDs
#define FIGHTPAD12SLIM_AMBIENT_PREV_PIN 31       // DIP switch: previous effect
#define FIGHTPAD12SLIM_AMBIENT_NEXT_PIN 32       // DIP switch: next effect
#define FIGHTPAD12SLIM_AMBIENT_DIP_SELECTOR_MODE 0
#define FIGHTPAD12SLIM_AMBIENT_CONTROLS_ACTIVE_LOW 1
#define FIGHTPAD12SLIM_AMBIENT_PIO pio2
#define FIGHTPAD12SLIM_AMBIENT_PIO_SM 0
#define FIGHTPAD12SLIM_AMBIENT_GP22_PIO_SM 1
#define FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE 16
#define FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS 0.25f
#define FIGHTPAD12SLIM_AMBIENT_BOOT_ENABLE 1
#define FIGHTPAD12SLIM_AMBIENT_DRIVE_BOOST_EN 1
#define FIGHTPAD12SLIM_AMBIENT_BOOST_EXTERNAL_PULLUP 0
#define FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL 1
#define FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF 1
#define FIGHTPAD12SLIM_AMBIENT_BOOST_STARTUP_DELAY_MS 5
#define FIGHTPAD12SLIM_AMBIENT_BOOST_SHUTDOWN_DELAY_US 1000
#define SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED 0 // Hide the level-0 entry; set to 1 to restore the four-page debug menu.
#define FIGHTPAD12SLIM_AMBIENT_POWER_ONLY_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_GP22_LOOPBACK_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_CONTROL_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_GPIO40_INIT_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_PIO_SETUP_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_ZERO_SHOW_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_FORCE_SOLID_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_PIXEL_SCAN_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_MS 500
#define FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN 40
#define FIGHTPAD12SLIM_AMBIENT_STARTUP_BLINK_DIAGNOSTIC 0
#define FIGHTPAD12SLIM_AMBIENT_RENDER_TOGGLE_DIAGNOSTIC 0

// HID transport select. GP33 comes from the SPDT switch:
// high = ESP32-C6 BT HID enabled, low = ESP32-C6 BT HID disabled. This is
// intentionally independent of VBUS/power-source sensing.
#define FIGHTPAD12SLIM_TRANSPORT_SEL_PIN 33
#define FIGHTPAD12SLIM_TRANSPORT_USB_LEVEL 1
#define FIGHTPAD12SLIM_TRANSPORT_BT_LEVEL 1

// ESP32-C6 BT-HID runtime link: RP2350 UART0 on GP44/GP45 sends compact
// gamepad input frames to the ESP32-C6 when GP33 selects BT. ESP32-C6 sends
// 0x46/0x49 firmware-info and 0x46/0x53 Bluetooth-status frames back. USB CDC
// stays disabled so WebConfig RNDIS keeps its original USB class behavior.
#define FIGHTPAD12SLIM_ESP32_PROXY_ENABLED 1
#define FIGHTPAD12SLIM_ESP32_PROXY_UART uart0
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_BLOCK 0
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_BAUD 115200
#define FIGHTPAD12SLIM_ESP32_PROXY_RESET_PIN -1
#define FIGHTPAD12SLIM_ESP32_PROXY_BOOT_PIN -1
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_TX_PIN 44
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_RX_PIN 45
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_CTS_PIN -1
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_RTS_PIN -1
#define FIGHTPAD12SLIM_ESP32_PROXY_USE_FLOW_CONTROL 0
#define FIGHTPAD12SLIM_ESP32_PROXY_AUTO_DTR_RTS 0
#define FIGHTPAD12SLIM_ESP32_PROXY_FORCE_BOARD_DEFAULTS 1
#define FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED 0
#define FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_ENABLED 1
#define FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_INTERVAL_MS 10
#define FIGHTPAD12SLIM_ESP32_BT_STATUS_RESULT_MS 1000
#define FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN 23

// BQ27220 battery gauge (software I2C: GP25=SCL, GP26=SDA, GP27=GPOUT)
#define FIGHTPAD12SLIM_BQ27220_ENABLED 1
#define FIGHTPAD12SLIM_BQ27220_SCL_PIN 25
#define FIGHTPAD12SLIM_BQ27220_SDA_PIN 26
#define FIGHTPAD12SLIM_BQ27220_GPOUT_PIN 27
#define FIGHTPAD12SLIM_BQ27220_BOOT_DELAY_MS 2000
#define FIGHTPAD12SLIM_BQ27220_POLL_INTERVAL_MS 2000
#define FIGHTPAD12SLIM_BQ27220_I2C_DELAY_US 5
#define FIGHTPAD12SLIM_BQ27220_LIGHTS_OFF_PERCENT 7
#define FIGHTPAD12SLIM_BQ27220_DIAGNOSTIC_DISPLAY 0
#define FIGHTPAD12SLIM_BQ27220_DATA_MEMORY_DIAGNOSTIC_DISPLAY 0
#define FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM 1
#define FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH 650
#define FIGHTPAD12SLIM_BQ27220_DESIGN_ENERGY_MWH 2405
#define FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV 3700
#define FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA 200
#define FIGHTPAD12SLIM_BQ27220_TAPER_VOLTAGE_MV 50
#define FIGHTPAD12SLIM_BQ27220_BATTERY_LOW_PERCENT_X100 700
#define FIGHTPAD12SLIM_BQ27220_INDEPENDENT_CHARGER 1
#define FIGHTPAD12SLIM_BQ27220_EDV_CMP 0
#define FIGHTPAD12SLIM_BQ27220_CSYNC 1
#define FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV 2750
#define FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV 4200
#define FIGHTPAD12SLIM_BQ27220_EDV0_MV 2750
#define FIGHTPAD12SLIM_BQ27220_EDV1_MV 3000
#define FIGHTPAD12SLIM_BQ27220_EDV2_MV 3300
#define FIGHTPAD12SLIM_BQ27220_SENSE_RESISTOR_MILLIOHMS 10
#define FIGHTPAD12SLIM_BQ27220_CALIBRATION_CURRENT_MA 318
#define FIGHTPAD12SLIM_BQ27220_CURRENT_CALIBRATED 0
#define FIGHTPAD12SLIM_BQ27220_LOG_UART_ENABLED 1
#define FIGHTPAD12SLIM_BQ27220_LOG_UART uart1
#define FIGHTPAD12SLIM_BQ27220_LOG_UART_BAUD 115200
#define FIGHTPAD12SLIM_BQ27220_LOG_INTERVAL_MS 4000
#define FIGHTPAD12SLIM_BQ27220_LOG_UART_BYTE_TIMEOUT_US 2000
#define FIGHTPAD12SLIM_BQ27220_LOG_UART_TX_PIN 42
#define FIGHTPAD12SLIM_BQ27220_LOG_UART_RX_PIN 43
// OLED (I2C0)
#define FIGHTPAD12SLIM_OLED_IDLE_SLEEP_ENABLED 1
#define FIGHTPAD12SLIM_OLED_IDLE_SLEEP_TIMEOUT_MS 60000
#define HAS_I2C_DISPLAY 1
#define I2C0_ENABLED 1
#define I2C0_PIN_SDA 0
#define I2C0_PIN_SCL 1
#define BUTTON_LAYOUT BUTTON_LAYOUT_BOARD_DEFINED_A
#define BUTTON_LAYOUT_RIGHT BUTTON_LAYOUT_BOARD_DEFINED_B
#define SPLASH_MODE SPLASH_MODE_STATIC
#define SPLASH_DURATION 3000

// OLED button viewer layout. These elements bind to raw GPIO pins so bring-up can
// prove the physical switch matrix even before higher-level button semantics change.
#define DEFAULT_BOARD_LAYOUT_A {\
    {GP_ELEMENT_PIN_BUTTON, {24,  32, 4, 4, 1, 1, 4,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {38,  32, 4, 4, 1, 1, 3,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {52,  36, 4, 4, 1, 1, 5,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {50,  52, 4, 4, 1, 1, 2,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {58,  14, 2, 2, 1, 1, 14,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {66,  14, 2, 2, 1, 1, 15,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {74,  14, 2, 2, 1, 1, 17,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {82,  14, 2, 2, 1, 1, 16,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {90,  14, 2, 2, 1, 1, 18,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {98,  14, 2, 2, 1, 1, 19,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {106, 14, 2, 2, 1, 1, 20,   GP_SHAPE_ELLIPSE}}\
}

#define DEFAULT_BOARD_LAYOUT_B {\
    {GP_ELEMENT_PIN_BUTTON, {62,  31, 4, 4, 1, 1, 8,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {74,  26, 4, 4, 1, 1, 9,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {86,  26, 4, 4, 1, 1, 12,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {98,  31, 4, 4, 1, 1, 10,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {58,  45, 4, 4, 1, 1, 6,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {70,  40, 4, 4, 1, 1, 7,    GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {84,  40, 4, 4, 1, 1, 13,   GP_SHAPE_ELLIPSE}},\
    {GP_ELEMENT_PIN_BUTTON, {98,  45, 4, 4, 1, 1, 11,   GP_SHAPE_ELLIPSE}}\
}

// Keyboard Host enabled by default (auth passthrough on PIO-USB expansion port)
#define KEYBOARD_HOST_ENABLED 1

// On-board status LED on GP23
#define BOARD_LED_PIN 23
#define BOARD_LED_ENABLED 1
#define BOARD_LED_TYPE ON_BOARD_LED_MODE_MODE_INDICATOR

#endif
