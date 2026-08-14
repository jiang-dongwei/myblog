#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ble_profiles.h"

static bool contains_bytes(const uint8_t *data, size_t data_len,
                           const uint8_t *wanted, size_t wanted_len)
{
    if (wanted_len == 0u || wanted_len > data_len) {
        return false;
    }
    for (size_t i = 0; i <= data_len - wanted_len; ++i) {
        if (memcmp(&data[i], wanted, wanted_len) == 0) {
            return true;
        }
    }
    return false;
}

static size_t descriptor_main_item_bits(
    const ble_profile_definition_t *definition,
    uint8_t wanted_item,
    uint8_t wanted_report_id)
{
    size_t offset = 0;
    size_t bits = 0;
    uint32_t report_size = 0;
    uint32_t report_count = 0;
    uint8_t report_id = 0;

    while (offset < definition->report_map_len) {
        uint8_t prefix = definition->report_map[offset++];
        assert(prefix != 0xFEu);
        size_t length = prefix & 3u;
        if (length == 3u) length = 4u;
        assert(offset + length <= definition->report_map_len);
        uint32_t value = 0;
        for (size_t i = 0; i < length; ++i) {
            value |= (uint32_t)definition->report_map[offset + i] << (8u * i);
        }
        switch (prefix & 0xFCu) {
        case 0x74u: report_size = value; break;
        case 0x94u: report_count = value; break;
        case 0x84u: report_id = (uint8_t)value; break;
        case 0x80u:
        case 0x90u:
            if ((prefix & 0xFCu) == wanted_item &&
                report_id == wanted_report_id) {
                bits += report_size * report_count;
            }
            break;
        default: break;
        }
        offset += length;
    }
    return bits;
}

static void test_profile_definitions(void)
{
    for (uint8_t id = FIGHTPAD_BLE_PROFILE_GENERIC;
         id <= FIGHTPAD_BLE_PROFILE_PS5_PC; ++id) {
        const ble_profile_definition_t *definition =
            ble_profile_get_definition((fightpad_ble_profile_t)id);
        assert(descriptor_main_item_bits(definition, 0x80u,
                                         definition->input_report_id) ==
               definition->input_report_len * 8u);
    }

    const ble_profile_definition_t *xbox =
        ble_profile_get_definition(FIGHTPAD_BLE_PROFILE_XBOX);
    assert(strcmp(xbox->device_name, "FP12Slim-C6") == 0);
    assert(strcmp(xbox->manufacturer_name, "Fightpad Bringup") == 0);
    assert(strcmp(xbox->serial_number, "3039373130303637313034303231") == 0);
    assert(xbox->vendor_id == 0x045Eu);
    assert(xbox->product_id == 0x0B13u);
    assert(xbox->version == 0x0509u);
    assert(xbox->input_report_len == 16u);
    assert(xbox->report_map_len == 283u);
    assert(descriptor_main_item_bits(xbox, 0x80u, 1u) == 128u);
    assert(descriptor_main_item_bits(xbox, 0x80u, 2u) == 0u);
    assert(descriptor_main_item_bits(xbox, 0x90u, 3u) == 64u);
    assert(descriptor_main_item_bits(xbox, 0x80u, 4u) == 0u);

    static const uint8_t buttons_1_to_15[] = {
        0x19, 0x01, 0x29, 0x0F, 0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x0F, 0x81, 0x02,
    };
    static const uint8_t axes_x_y_16_bit[] = {
        0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x27, 0xFF,
        0xFF, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02,
    };
    static const uint8_t trigger_10_bit[] = {
        0x09, 0xC5, 0x15, 0x00, 0x26, 0xFF, 0x03,
        0x95, 0x01, 0x75, 0x0A, 0x81, 0x02,
    };
    static const uint8_t rumble_output_report[] = {
        0x05, 0x0F, 0x09, 0x21, 0x85, 0x03,
    };
    static const uint8_t share_button_usage[] = {
        0x05, 0x0C, 0x0A, 0xB2, 0x00,
    };
    assert(contains_bytes(xbox->report_map, xbox->report_map_len,
                          buttons_1_to_15, sizeof(buttons_1_to_15)));
    assert(contains_bytes(xbox->report_map, xbox->report_map_len,
                          axes_x_y_16_bit, sizeof(axes_x_y_16_bit)));
    assert(contains_bytes(xbox->report_map, xbox->report_map_len,
                          trigger_10_bit, sizeof(trigger_10_bit)));
    assert(contains_bytes(xbox->report_map, xbox->report_map_len,
                          rumble_output_report, sizeof(rumble_output_report)));
    assert(contains_bytes(xbox->report_map, xbox->report_map_len,
                          share_button_usage, sizeof(share_button_usage)));

    for (uint8_t id = FIGHTPAD_BLE_PROFILE_GENERIC;
         id <= FIGHTPAD_BLE_PROFILE_PS5_PC; ++id) {
        const ble_profile_definition_t *definition =
            ble_profile_get_definition((fightpad_ble_profile_t)id);
        assert(strcmp(definition->device_name, "FP12Slim-C6") == 0);
        /* Legacy advertising payload: Flags(3) + Appearance(4) + HID UUID(4)
         * + Complete Name(2 + name bytes) must fit in 31 bytes. */
        assert(3u + 4u + 4u + 2u + strlen(definition->device_name) <= 31u);
        if (id != FIGHTPAD_BLE_PROFILE_XBOX) {
            assert(definition->vendor_id == 0x1209u);
            assert(definition->product_id == 0x2040u);
        }
    }
}

static void test_xbox_usb_layout_mapping(void)
{
    normalized_fightpad_state_t input = {
        .buttons = FIGHTPAD_BUTTON_B1 | FIGHTPAD_BUTTON_B2 |
                   FIGHTPAD_BUTTON_B3 | FIGHTPAD_BUTTON_B4 |
                   FIGHTPAD_BUTTON_L1 | FIGHTPAD_BUTTON_L2 |
                   FIGHTPAD_BUTTON_R1 | FIGHTPAD_BUTTON_R2 |
                   FIGHTPAD_BUTTON_L3 | FIGHTPAD_BUTTON_R3 |
                   FIGHTPAD_BUTTON_S1 | FIGHTPAD_BUTTON_S2 |
                   FIGHTPAD_BUTTON_A1 | FIGHTPAD_BUTTON_A2 |
                   FIGHTPAD_BUTTON_TURBO,
        .dpad = FIGHTPAD_DPAD_DOWN | FIGHTPAD_DPAD_LEFT,
        .x = -12,
        .y = 34,
    };
    uint8_t report[BLE_PROFILE_MAX_REPORT_LEN];
    size_t length = 0;
    assert(ble_profile_encode_report(FIGHTPAD_BLE_PROFILE_XBOX,
                                     &input, report, sizeof(report), &length));
    static const uint8_t expected[] = {
        0x00, 0x74, 0x00, 0xA2, 0x00, 0x80, 0x00, 0x80,
        0xFF, 0x03, 0xFF, 0x03, 0x06, 0xDB, 0x7C, 0x00,
    };
    assert(length == sizeof(expected));
    assert(memcmp(report, expected, sizeof(expected)) == 0);

    input.buttons = FIGHTPAD_BUTTON_A2 | FIGHTPAD_BUTTON_TURBO;
    input.dpad = 0;
    input.x = input.y = 0;
    assert(ble_profile_encode_report(FIGHTPAD_BLE_PROFILE_XBOX,
                                     &input, report, sizeof(report), &length));
    static const uint8_t neutral[] = {
        0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    assert(memcmp(report, neutral, sizeof(neutral)) == 0);
}

int main(void)
{
    test_profile_definitions();
    test_xbox_usb_layout_mapping();
    puts("ble_profiles_test: PASS");
    return 0;
}
