#include "uart_protocol.h"

#include <stddef.h>
#include <string.h>

uint8_t fightpad_uart_checksum(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN])
{
    uint8_t checksum = 0;

    if (frame == NULL) {
        return 0;
    }
    for (size_t i = 0; i < FIGHTPAD_UART_FRAME_LEN - 1u; ++i) {
        checksum ^= frame[i];
    }
    return checksum;
}

bool fightpad_uart_frame_checksum_valid(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN])
{
    return frame != NULL &&
           frame[FIGHTPAD_UART_FRAME_LEN - 1u] ==
               fightpad_uart_checksum(frame);
}

bool fightpad_uart_input_type_allowed(uint8_t type)
{
    switch (type) {
    case FIGHTPAD_UART_TYPE_INPUT_REPORT:
    case FIGHTPAD_UART_TYPE_TRANSPORT:
    case FIGHTPAD_UART_TYPE_BATTERY:
    case FIGHTPAD_UART_TYPE_PROFILE_MODE:
    case FIGHTPAD_UART_TYPE_PROFILE_ACK:
        return true;
    default:
        return false;
    }
}

bool fightpad_ble_profile_valid(uint8_t profile)
{
    return profile <= FIGHTPAD_BLE_PROFILE_PS5_PC;
}

fightpad_mode_parse_result_t fightpad_uart_parse_profile_mode(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN],
    fightpad_ble_profile_mode_t *mode)
{
    if (frame == NULL || mode == NULL ||
        frame[0] != FIGHTPAD_UART_MAGIC ||
        frame[1] != FIGHTPAD_UART_TYPE_PROFILE_MODE ||
        !fightpad_uart_frame_checksum_valid(frame)) {
        return FIGHTPAD_MODE_PARSE_BAD_FRAME;
    }

    mode->version = frame[2];
    mode->requested_profile = frame[3];
    mode->accepted_profile = fightpad_ble_profile_valid(frame[3])
                                 ? (fightpad_ble_profile_t)frame[3]
                                 : FIGHTPAD_BLE_PROFILE_GENERIC;
    mode->sequence = frame[4];
    mode->flags = frame[5];
    mode->reserved = frame[6];

    if (mode->version != FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION) {
        return FIGHTPAD_MODE_PARSE_UNSUPPORTED_VERSION;
    }
    if (!fightpad_ble_profile_valid(mode->requested_profile)) {
        return FIGHTPAD_MODE_PARSE_INVALID_PROFILE;
    }
    return FIGHTPAD_MODE_PARSE_OK;
}

void fightpad_uart_build_profile_ack(
    fightpad_ble_profile_t accepted_profile,
    uint8_t sequence,
    fightpad_ble_profile_ack_t result,
    uint8_t frame[FIGHTPAD_UART_FRAME_LEN])
{
    if (frame == NULL) {
        return;
    }
    if (!fightpad_ble_profile_valid((uint8_t)accepted_profile)) {
        accepted_profile = FIGHTPAD_BLE_PROFILE_GENERIC;
    }
    if ((uint8_t)result > BLE_PROFILE_ACK_INTERNAL_ERROR) {
        result = BLE_PROFILE_ACK_INTERNAL_ERROR;
    }

    memset(frame, 0, FIGHTPAD_UART_FRAME_LEN);
    frame[0] = FIGHTPAD_UART_MAGIC;
    frame[1] = FIGHTPAD_UART_TYPE_PROFILE_ACK;
    frame[2] = FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION;
    frame[3] = (uint8_t)accepted_profile;
    frame[4] = sequence;
    frame[5] = (uint8_t)result;
    frame[7] = fightpad_uart_checksum(frame);
}

void fightpad_uart_build_ble_status(
    uint8_t status,
    uint8_t frame[FIGHTPAD_UART_FRAME_LEN])
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, FIGHTPAD_UART_FRAME_LEN);
    frame[0] = FIGHTPAD_UART_MAGIC;
    frame[1] = FIGHTPAD_UART_TYPE_BLE_STATUS;
    frame[2] = status;
    frame[7] = fightpad_uart_checksum(frame);
}
