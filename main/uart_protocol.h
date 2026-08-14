#ifndef FIGHTPAD_UART_PROTOCOL_H
#define FIGHTPAD_UART_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define FIGHTPAD_UART_FRAME_LEN 8u
#define FIGHTPAD_UART_MAGIC 0x46u

#define FIGHTPAD_UART_TYPE_INPUT_REPORT 0x50u
#define FIGHTPAD_UART_TYPE_TRANSPORT 0x54u
#define FIGHTPAD_UART_TYPE_BATTERY 0x42u
#define FIGHTPAD_UART_TYPE_FW_INFO 0x49u
#define FIGHTPAD_UART_TYPE_BLE_STATUS 0x53u
#define FIGHTPAD_UART_TYPE_PROFILE_MODE 0x4Du
#define FIGHTPAD_UART_TYPE_PROFILE_ACK 0x41u

#define FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION 0x01u

#define BLE_PROFILE_FLAG_APPLY_NOW 0x01u
#define BLE_PROFILE_FLAG_FORCE_REPAIR 0x02u
#define BLE_PROFILE_FLAG_KNOWN_MASK \
    (BLE_PROFILE_FLAG_APPLY_NOW | BLE_PROFILE_FLAG_FORCE_REPAIR)

typedef enum {
    FIGHTPAD_BLE_PROFILE_GENERIC = 0,
    FIGHTPAD_BLE_PROFILE_XBOX = 1,
    FIGHTPAD_BLE_PROFILE_KEYBOARD = 2,
    FIGHTPAD_BLE_PROFILE_PS5_PC = 3,
} fightpad_ble_profile_t;

typedef enum {
    BLE_PROFILE_ACK_ACTIVE_UNCHANGED = 0,
    BLE_PROFILE_ACK_RESTARTING = 1,
    BLE_PROFILE_ACK_APPLYING_AT_BOOT = 2,
    BLE_PROFILE_ACK_INVALID_FALLBACK = 3,
    BLE_PROFILE_ACK_UNSUPPORTED_VERSION = 4,
    BLE_PROFILE_ACK_INTERNAL_ERROR = 5,
} fightpad_ble_profile_ack_t;

typedef struct {
    uint8_t version;
    uint8_t requested_profile;
    fightpad_ble_profile_t accepted_profile;
    uint8_t sequence;
    uint8_t flags;
    uint8_t reserved;
} fightpad_ble_profile_mode_t;

typedef enum {
    FIGHTPAD_MODE_PARSE_OK = 0,
    FIGHTPAD_MODE_PARSE_BAD_FRAME,
    FIGHTPAD_MODE_PARSE_UNSUPPORTED_VERSION,
    FIGHTPAD_MODE_PARSE_INVALID_PROFILE,
} fightpad_mode_parse_result_t;

uint8_t fightpad_uart_checksum(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN]);

bool fightpad_uart_frame_checksum_valid(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN]);

bool fightpad_uart_input_type_allowed(uint8_t type);
bool fightpad_ble_profile_valid(uint8_t profile);

fightpad_mode_parse_result_t fightpad_uart_parse_profile_mode(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN],
    fightpad_ble_profile_mode_t *mode);

void fightpad_uart_build_profile_ack(
    fightpad_ble_profile_t accepted_profile,
    uint8_t sequence,
    fightpad_ble_profile_ack_t result,
    uint8_t frame[FIGHTPAD_UART_FRAME_LEN]);

void fightpad_uart_build_ble_status(
    uint8_t status,
    uint8_t frame[FIGHTPAD_UART_FRAME_LEN]);

#endif
