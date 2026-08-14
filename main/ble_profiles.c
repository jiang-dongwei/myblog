#include "ble_profiles.h"

#include <string.h>

#define FIGHTPAD_VENDOR_ID 0x1209u
#define FIGHTPAD_PRODUCT_ID 0x2040u
#define XBOX_BLUETOOTH_VENDOR_ID 0x045Eu
#define XBOX_SERIES_1914_PRODUCT_ID 0x0B13u
#define XBOX_SERIES_1914_VERSION 0x0509u
#define XBOX_SERIES_1914_SERIAL_NUMBER "3039373130303637313034303231"
#define FIGHTPAD_DEVICE_NAME "FP12Slim-C6"
#define FIGHTPAD_MANUFACTURER_NAME "Fightpad Bringup"
#define FIGHTPAD_SERIAL_NUMBER "ESP32C6-BLE-HID-TEST"
#define GENERIC_NEUTRAL_HAT 0x08u

static const uint8_t generic_report_map[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
    0x85, BLE_PROFILE_REPORT_ID,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
    0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x03,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0xC0,
};

static const uint8_t xbox_report_map[] = {
    /* Xbox Series X|S model 1914 report map from the MIT-licensed
     * https://github.com/Mystfit/ESP32-BLE-CompositeHID project.
     * Report 1 is the 16-byte gamepad input. Report 3 is kept for Windows
     * driver matching, but vibration output is intentionally ignored. */
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x95,
    0x02, 0x75, 0x10, 0x81, 0x02, 0xC0, 0x09, 0x01, 0xA1, 0x00, 0x09, 0x32,
    0x09, 0x35, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x95, 0x02, 0x75,
    0x10, 0x81, 0x02, 0xC0, 0x05, 0x02, 0x09, 0xC5, 0x15, 0x00, 0x26, 0xFF,
    0x03, 0x95, 0x01, 0x75, 0x0A, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75,
    0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x02, 0x09, 0xC4, 0x15, 0x00, 0x26,
    0xFF, 0x03, 0x95, 0x01, 0x75, 0x0A, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00,
    0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x01, 0x09, 0x39, 0x15, 0x01,
    0x25, 0x08, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x66, 0x14, 0x00, 0x75, 0x04,
    0x95, 0x01, 0x81, 0x42, 0x75, 0x04, 0x95, 0x01, 0x15, 0x00, 0x25, 0x00,
    0x35, 0x00, 0x45, 0x00, 0x65, 0x00, 0x81, 0x03, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x0F, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0F, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x00, 0x75, 0x01, 0x95, 0x01, 0x81, 0x03, 0x05, 0x0C,
    0x0A, 0xB2, 0x00, 0x15, 0x00, 0x25, 0x01, 0x95, 0x01, 0x75, 0x01, 0x81,
    0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0x05,
    0x0F, 0x09, 0x21, 0x85, 0x03, 0xA1, 0x02, 0x09, 0x97, 0x15, 0x00, 0x25,
    0x01, 0x75,
    0x04, 0x95, 0x01, 0x91, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x04, 0x95,
    0x01, 0x91, 0x03, 0x09, 0x70, 0x15, 0x00, 0x25, 0x64, 0x75, 0x08, 0x95,
    0x04, 0x91, 0x02, 0x09, 0x50, 0x66, 0x01, 0x10, 0x55, 0x0E, 0x15, 0x00,
    0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0x09, 0xA7, 0x15,
    0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0x65, 0x00,
    0x55, 0x00, 0x09, 0x7C, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
    0x01, 0x91, 0x02, 0xC0, 0xC0,
};

_Static_assert(sizeof(xbox_report_map) == 283u,
               "Xbox Series X|S 1914 report map must remain byte-exact");
_Static_assert(BLE_PROFILE_MAX_REPORT_LEN >= 16u,
               "Xbox Series X|S input report buffer is too small");

static const uint8_t keyboard_report_map[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, BLE_PROFILE_REPORT_ID,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x19, 0x00, 0x29, 0x7F, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x80, 0x81, 0x02,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x75, 0x01,
    0x95, 0x05, 0x91, 0x02, 0x75, 0x03, 0x95, 0x01,
    0x91, 0x03, 0xC0,
};

static const uint8_t ps_report_map[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
    0x85, BLE_PROFILE_REPORT_ID,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0E, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x0E, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x02, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34,
    0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x04,
    0x81, 0x02, 0xC0,
};

static const ble_profile_definition_t definitions[] = {
    [FIGHTPAD_BLE_PROFILE_GENERIC] = {
        .profile = FIGHTPAD_BLE_PROFILE_GENERIC,
        .profile_label = "Generic",
        .device_name = FIGHTPAD_DEVICE_NAME,
        .manufacturer_name = FIGHTPAD_MANUFACTURER_NAME,
        .serial_number = FIGHTPAD_SERIAL_NUMBER,
        .vendor_id = FIGHTPAD_VENDOR_ID,
        .product_id = FIGHTPAD_PRODUCT_ID,
        .version = 0x0001u,
        .report_map = generic_report_map,
        .report_map_len = sizeof(generic_report_map),
        .input_report_id = BLE_PROFILE_REPORT_ID,
        .input_report_len = 5u,
    },
    [FIGHTPAD_BLE_PROFILE_XBOX] = {
        .profile = FIGHTPAD_BLE_PROFILE_XBOX,
        .profile_label = "Xbox Series X|S BLE",
        .device_name = FIGHTPAD_DEVICE_NAME,
        .manufacturer_name = FIGHTPAD_MANUFACTURER_NAME,
        .serial_number = XBOX_SERIES_1914_SERIAL_NUMBER,
        .vendor_id = XBOX_BLUETOOTH_VENDOR_ID,
        .product_id = XBOX_SERIES_1914_PRODUCT_ID,
        .version = XBOX_SERIES_1914_VERSION,
        .report_map = xbox_report_map,
        .report_map_len = sizeof(xbox_report_map),
        .input_report_id = BLE_PROFILE_REPORT_ID,
        .input_report_len = 16u,
    },
    [FIGHTPAD_BLE_PROFILE_KEYBOARD] = {
        .profile = FIGHTPAD_BLE_PROFILE_KEYBOARD,
        .profile_label = "Keyboard",
        .device_name = FIGHTPAD_DEVICE_NAME,
        .manufacturer_name = FIGHTPAD_MANUFACTURER_NAME,
        .serial_number = FIGHTPAD_SERIAL_NUMBER,
        .vendor_id = FIGHTPAD_VENDOR_ID,
        .product_id = FIGHTPAD_PRODUCT_ID,
        .version = 0x0001u,
        .report_map = keyboard_report_map,
        .report_map_len = sizeof(keyboard_report_map),
        .input_report_id = BLE_PROFILE_REPORT_ID,
        .input_report_len = 17u,
    },
    [FIGHTPAD_BLE_PROFILE_PS5_PC] = {
        .profile = FIGHTPAD_BLE_PROFILE_PS5_PC,
        .profile_label = "PlayStation Layout",
        .device_name = FIGHTPAD_DEVICE_NAME,
        .manufacturer_name = FIGHTPAD_MANUFACTURER_NAME,
        .serial_number = FIGHTPAD_SERIAL_NUMBER,
        .vendor_id = FIGHTPAD_VENDOR_ID,
        .product_id = FIGHTPAD_PRODUCT_ID,
        .version = 0x0001u,
        .report_map = ps_report_map,
        .report_map_len = sizeof(ps_report_map),
        .input_report_id = BLE_PROFILE_REPORT_ID,
        .input_report_len = 7u,
    },
};

const ble_profile_definition_t *ble_profile_get_definition(
    fightpad_ble_profile_t profile)
{
    if (!fightpad_ble_profile_valid((uint8_t)profile)) {
        profile = FIGHTPAD_BLE_PROFILE_GENERIC;
    }
    return &definitions[(uint8_t)profile];
}

static uint8_t hat_from_dpad(uint8_t dpad)
{
    bool up = (dpad & FIGHTPAD_DPAD_UP) != 0u;
    bool down = (dpad & FIGHTPAD_DPAD_DOWN) != 0u;
    bool left = (dpad & FIGHTPAD_DPAD_LEFT) != 0u;
    bool right = (dpad & FIGHTPAD_DPAD_RIGHT) != 0u;

    if (up && down) up = down = false;
    if (left && right) left = right = false;
    if (up && right) return 1u;
    if (down && right) return 3u;
    if (down && left) return 5u;
    if (up && left) return 7u;
    if (up) return 0u;
    if (right) return 2u;
    if (down) return 4u;
    if (left) return 6u;
    return GENERIC_NEUTRAL_HAT;
}

static void set_keyboard_usage(uint8_t report[17], uint8_t usage)
{
    if (usage < 0x80u) {
        report[1u + usage / 8u] |= (uint8_t)(1u << (usage % 8u));
    }
}

static void encode_keyboard(const normalized_fightpad_state_t *state,
                            uint8_t report[17])
{
    const uint16_t b = state->buttons;
    bool up = (state->dpad & FIGHTPAD_DPAD_UP) != 0u;
    bool down = (state->dpad & FIGHTPAD_DPAD_DOWN) != 0u;
    bool left = (state->dpad & FIGHTPAD_DPAD_LEFT) != 0u;
    bool right = (state->dpad & FIGHTPAD_DPAD_RIGHT) != 0u;

    if (up && down) up = down = false;
    if (left && right) left = right = false;
    if (up) set_keyboard_usage(report, 0x52);
    if (down) set_keyboard_usage(report, 0x51);
    if (right) set_keyboard_usage(report, 0x4F);
    if (left) set_keyboard_usage(report, 0x50);

    if (b & FIGHTPAD_BUTTON_B1) report[0] |= 1u << 1;
    if (b & FIGHTPAD_BUTTON_B3) report[0] |= 1u << 0;
    if (b & FIGHTPAD_BUTTON_B4) report[0] |= 1u << 2;
    if (b & FIGHTPAD_BUTTON_B2) set_keyboard_usage(report, 0x1D);
    if (b & FIGHTPAD_BUTTON_R2) set_keyboard_usage(report, 0x1B);
    if (b & FIGHTPAD_BUTTON_L2) set_keyboard_usage(report, 0x19);
    if (b & FIGHTPAD_BUTTON_R1) set_keyboard_usage(report, 0x2C);
    if (b & FIGHTPAD_BUTTON_L1) set_keyboard_usage(report, 0x06);
    if (b & FIGHTPAD_BUTTON_S1) set_keyboard_usage(report, 0x22);
    if (b & FIGHTPAD_BUTTON_S2) set_keyboard_usage(report, 0x1E);
    if (b & FIGHTPAD_BUTTON_L3) set_keyboard_usage(report, 0x2E);
    if (b & FIGHTPAD_BUTTON_R3) set_keyboard_usage(report, 0x2D);
    if (b & FIGHTPAD_BUTTON_A1) set_keyboard_usage(report, 0x26);
    if (b & FIGHTPAD_BUTTON_A2) set_keyboard_usage(report, 0x3B);
}

static uint16_t map_xbox_buttons(uint16_t in)
{
    uint16_t out = 0;
    if (in & FIGHTPAD_BUTTON_B1) out |= 1u << 0;
    if (in & FIGHTPAD_BUTTON_B2) out |= 1u << 1;
    if (in & FIGHTPAD_BUTTON_B3) out |= 1u << 3;
    if (in & FIGHTPAD_BUTTON_B4) out |= 1u << 4;
    if (in & FIGHTPAD_BUTTON_L1) out |= 1u << 6;
    if (in & FIGHTPAD_BUTTON_R1) out |= 1u << 7;
    if (in & FIGHTPAD_BUTTON_S1) out |= 1u << 10;
    if (in & FIGHTPAD_BUTTON_S2) out |= 1u << 11;
    if (in & FIGHTPAD_BUTTON_A1) out |= 1u << 12;
    if (in & FIGHTPAD_BUTTON_L3) out |= 1u << 13;
    if (in & FIGHTPAD_BUTTON_R3) out |= 1u << 14;
    /* A2 and Turbo intentionally match USB XInput: they are not exported. */
    return out;
}

static uint8_t xbox_hat_from_dpad(uint8_t dpad)
{
    const uint8_t generic_hat = hat_from_dpad(dpad);
    return generic_hat == GENERIC_NEUTRAL_HAT ? 0u : (uint8_t)(generic_hat + 1u);
}

static uint16_t xbox_axis_from_i8(int8_t value)
{
    /* The 1914 report uses unsigned 16-bit axes centered at 0x8000. */
    return (uint16_t)((int32_t)value * 256 + 0x8000);
}

static void write_le16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static uint16_t map_ps_buttons(uint16_t in)
{
    uint16_t out = 0;
    if (in & FIGHTPAD_BUTTON_B1) out |= 1u << 0;
    if (in & FIGHTPAD_BUTTON_B2) out |= 1u << 1;
    if (in & FIGHTPAD_BUTTON_B3) out |= 1u << 2;
    if (in & FIGHTPAD_BUTTON_B4) out |= 1u << 3;
    if (in & FIGHTPAD_BUTTON_L1) out |= 1u << 4;
    if (in & FIGHTPAD_BUTTON_R1) out |= 1u << 5;
    if (in & FIGHTPAD_BUTTON_L2) out |= 1u << 6;
    if (in & FIGHTPAD_BUTTON_R2) out |= 1u << 7;
    if (in & FIGHTPAD_BUTTON_S1) out |= 1u << 8;
    if (in & FIGHTPAD_BUTTON_S2) out |= 1u << 9;
    if (in & FIGHTPAD_BUTTON_L3) out |= 1u << 10;
    if (in & FIGHTPAD_BUTTON_R3) out |= 1u << 11;
    if (in & FIGHTPAD_BUTTON_A1) out |= 1u << 12;
    if (in & FIGHTPAD_BUTTON_A2) out |= 1u << 13;
    return out;
}

bool ble_profile_encode_report(
    fightpad_ble_profile_t profile,
    const normalized_fightpad_state_t *state,
    uint8_t *out,
    size_t out_capacity,
    size_t *out_length)
{
    if (state == NULL || out == NULL || out_length == NULL) {
        return false;
    }

    const ble_profile_definition_t *definition =
        ble_profile_get_definition(profile);
    if (out_capacity < definition->input_report_len) {
        return false;
    }
    memset(out, 0, definition->input_report_len);

    switch (definition->profile) {
    case FIGHTPAD_BLE_PROFILE_GENERIC: {
        const uint16_t buttons = state->buttons &
            (uint16_t)~FIGHTPAD_BUTTON_TURBO;
        out[0] = (uint8_t)buttons;
        out[1] = (uint8_t)(buttons >> 8);
        out[2] = hat_from_dpad(state->dpad) & 0x0Fu;
        out[3] = (uint8_t)state->x;
        out[4] = (uint8_t)state->y;
        break;
    }
    case FIGHTPAD_BLE_PROFILE_XBOX: {
        const uint16_t buttons = map_xbox_buttons(state->buttons);
        write_le16(&out[0], xbox_axis_from_i8(state->x));
        write_le16(&out[2], xbox_axis_from_i8(state->y));
        /* UART has no right-stick values, so Z/Rz remain centered. */
        write_le16(&out[4], 0x8000u);
        write_le16(&out[6], 0x8000u);
        write_le16(&out[8], (state->buttons & FIGHTPAD_BUTTON_L2) ? 0x03FFu : 0u);
        write_le16(&out[10], (state->buttons & FIGHTPAD_BUTTON_R2) ? 0x03FFu : 0u);
        out[12] = xbox_hat_from_dpad(state->dpad);
        write_le16(&out[13], buttons);
        /* Model 1914 share button byte. RP2350 A2/Turbo stay unexported. */
        out[15] = 0u;
        break;
    }
    case FIGHTPAD_BLE_PROFILE_KEYBOARD:
        encode_keyboard(state, out);
        break;
    case FIGHTPAD_BLE_PROFILE_PS5_PC: {
        const uint16_t buttons = map_ps_buttons(state->buttons);
        out[0] = (uint8_t)buttons;
        out[1] = (uint8_t)(buttons >> 8);
        out[2] = hat_from_dpad(state->dpad) & 0x0Fu;
        out[3] = (uint8_t)state->x;
        out[4] = (uint8_t)state->y;
        out[5] = 0u;
        out[6] = 0u;
        break;
    }
    }

    *out_length = definition->input_report_len;
    return true;
}

bool ble_profile_neutral_report(
    fightpad_ble_profile_t profile,
    uint8_t *out,
    size_t out_capacity,
    size_t *out_length)
{
    const normalized_fightpad_state_t neutral = {0};
    return ble_profile_encode_report(profile, &neutral,
                                     out, out_capacity, out_length);
}
