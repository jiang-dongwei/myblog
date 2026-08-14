#ifndef FIGHTPAD_BLE_PROFILES_H
#define FIGHTPAD_BLE_PROFILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uart_protocol.h"

#define BLE_PROFILE_REPORT_MAP_INDEX 0u
#define BLE_PROFILE_REPORT_ID 1u
#define BLE_PROFILE_MAX_REPORT_LEN 17u

#define FIGHTPAD_BUTTON_B1 (1u << 0)
#define FIGHTPAD_BUTTON_B2 (1u << 1)
#define FIGHTPAD_BUTTON_B3 (1u << 2)
#define FIGHTPAD_BUTTON_B4 (1u << 3)
#define FIGHTPAD_BUTTON_L1 (1u << 4)
#define FIGHTPAD_BUTTON_L2 (1u << 5)
#define FIGHTPAD_BUTTON_R1 (1u << 6)
#define FIGHTPAD_BUTTON_R2 (1u << 7)
#define FIGHTPAD_BUTTON_L3 (1u << 8)
#define FIGHTPAD_BUTTON_R3 (1u << 9)
#define FIGHTPAD_BUTTON_S1 (1u << 10)
#define FIGHTPAD_BUTTON_S2 (1u << 11)
#define FIGHTPAD_BUTTON_A1 (1u << 12)
#define FIGHTPAD_BUTTON_A2 (1u << 13)
#define FIGHTPAD_BUTTON_TURBO (1u << 14)

#define FIGHTPAD_DPAD_UP 0x01u
#define FIGHTPAD_DPAD_DOWN 0x02u
#define FIGHTPAD_DPAD_LEFT 0x04u
#define FIGHTPAD_DPAD_RIGHT 0x08u

typedef struct {
    uint16_t buttons;
    uint8_t dpad;
    int8_t x;
    int8_t y;
} normalized_fightpad_state_t;

typedef struct {
    fightpad_ble_profile_t profile;
    const char *profile_label;
    const char *device_name;
    const char *manufacturer_name;
    const char *serial_number;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t version;
    const uint8_t *report_map;
    size_t report_map_len;
    uint8_t input_report_id;
    size_t input_report_len;
} ble_profile_definition_t;

const ble_profile_definition_t *ble_profile_get_definition(
    fightpad_ble_profile_t profile);

bool ble_profile_encode_report(
    fightpad_ble_profile_t profile,
    const normalized_fightpad_state_t *state,
    uint8_t *out,
    size_t out_capacity,
    size_t *out_length);

bool ble_profile_neutral_report(
    fightpad_ble_profile_t profile,
    uint8_t *out,
    size_t out_capacity,
    size_t *out_length);

#endif
