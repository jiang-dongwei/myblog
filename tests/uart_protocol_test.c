#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uart_protocol.h"

static void test_handoff_vectors(void)
{
    static const uint8_t mode_frame[FIGHTPAD_UART_FRAME_LEN] = {
        0x46, 0x4D, 0x01, 0x01, 0x2A, 0x01, 0x00, 0x20,
    };
    static const uint8_t restart_ack[FIGHTPAD_UART_FRAME_LEN] = {
        0x46, 0x41, 0x01, 0x01, 0x2A, 0x01, 0x00, 0x2C,
    };
    static const uint8_t unchanged_ack[FIGHTPAD_UART_FRAME_LEN] = {
        0x46, 0x41, 0x01, 0x01, 0x2A, 0x00, 0x00, 0x2D,
    };
    fightpad_ble_profile_mode_t mode = {0};
    uint8_t ack[FIGHTPAD_UART_FRAME_LEN];

    assert(fightpad_uart_parse_profile_mode(mode_frame, &mode) ==
           FIGHTPAD_MODE_PARSE_OK);
    assert(mode.accepted_profile == FIGHTPAD_BLE_PROFILE_XBOX);
    assert(mode.sequence == 0x2A);
    assert(mode.flags == BLE_PROFILE_FLAG_APPLY_NOW);

    fightpad_uart_build_profile_ack(FIGHTPAD_BLE_PROFILE_XBOX, 0x2A,
                                    BLE_PROFILE_ACK_RESTARTING, ack);
    assert(memcmp(ack, restart_ack, sizeof(ack)) == 0);
    fightpad_uart_build_profile_ack(FIGHTPAD_BLE_PROFILE_XBOX, 0x2A,
                                    BLE_PROFILE_ACK_ACTIVE_UNCHANGED, ack);
    assert(memcmp(ack, unchanged_ack, sizeof(ack)) == 0);
}

static void test_validation_and_status(void)
{
    uint8_t frame[FIGHTPAD_UART_FRAME_LEN] = {
        0x46, 0x4D, 0x01, 0xFF, 0x12, 0x00, 0x00, 0x00,
    };
    fightpad_ble_profile_mode_t mode = {0};
    frame[7] = fightpad_uart_checksum(frame);
    assert(fightpad_uart_parse_profile_mode(frame, &mode) ==
           FIGHTPAD_MODE_PARSE_INVALID_PROFILE);
    assert(mode.accepted_profile == FIGHTPAD_BLE_PROFILE_GENERIC);

    fightpad_uart_build_ble_status(0x03, frame);
    static const uint8_t expected[FIGHTPAD_UART_FRAME_LEN] = {
        0x46, 0x53, 0x03, 0x00, 0x00, 0x00, 0x00, 0x16,
    };
    assert(memcmp(frame, expected, sizeof(frame)) == 0);
    assert(fightpad_uart_frame_checksum_valid(frame));
}

int main(void)
{
    test_handoff_vectors();
    test_validation_and_status();
    puts("uart_protocol_test: PASS");
    return 0;
}
