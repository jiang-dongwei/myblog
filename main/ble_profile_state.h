#ifndef FIGHTPAD_BLE_PROFILE_STATE_H
#define FIGHTPAD_BLE_PROFILE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "uart_protocol.h"

#define BLE_PROFILE_STATE_FORMAT_VERSION 6u
#define BLE_PROFILE_STATE_LEGACY_V1 1u
#define BLE_PROFILE_STATE_LEGACY_V2 2u
#define BLE_PROFILE_STATE_LEGACY_V3 3u
#define BLE_PROFILE_STATE_LEGACY_V4 4u
#define BLE_PROFILE_STATE_LEGACY_V5 5u

#define BLE_PROFILE_PENDING_CLEAR_BONDS 0x01u
#define BLE_PROFILE_PENDING_PAIRING 0x02u
#define BLE_PROFILE_PENDING_KNOWN_MASK \
    (BLE_PROFILE_PENDING_CLEAR_BONDS | BLE_PROFILE_PENDING_PAIRING)

typedef struct {
    uint8_t format_version;
    uint8_t profile;
    uint8_t pending_flags;
    uint8_t reserved;
} ble_profile_persisted_state_t;

typedef enum {
    BLE_PROFILE_PHASE_BOOT = 0,
    BLE_PROFILE_PHASE_RUNTIME,
} ble_profile_phase_t;

typedef struct {
    bool respond;
    bool persist;
    bool restart;
    fightpad_ble_profile_t accepted_profile;
    fightpad_ble_profile_ack_t ack_result;
    ble_profile_persisted_state_t next_state;
} ble_profile_mode_decision_t;

void ble_profile_state_default(ble_profile_persisted_state_t *state);
bool ble_profile_state_valid(const ble_profile_persisted_state_t *state);
bool ble_profile_state_can_migrate(uint8_t format_version);
bool ble_profile_state_migrate_legacy(
    const ble_profile_persisted_state_t *legacy,
    ble_profile_persisted_state_t *migrated);

void ble_profile_decide_mode(
    const ble_profile_persisted_state_t *current_state,
    fightpad_ble_profile_t active_profile,
    ble_profile_phase_t phase,
    fightpad_mode_parse_result_t parse_result,
    const fightpad_ble_profile_mode_t *mode,
    ble_profile_mode_decision_t *decision);

#endif
