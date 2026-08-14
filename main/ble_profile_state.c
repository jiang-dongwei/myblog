#include "ble_profile_state.h"

#include <string.h>

void ble_profile_state_default(ble_profile_persisted_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->format_version = BLE_PROFILE_STATE_FORMAT_VERSION;
    state->profile = FIGHTPAD_BLE_PROFILE_XBOX;
    /* A missing/invalid NVS record selects the requested factory profile, but
     * must not erase controller bonds or enter pairing automatically. Pairing
     * remains an explicit GPIO13/user action. */
    state->pending_flags = 0u;
}

bool ble_profile_state_valid(const ble_profile_persisted_state_t *state)
{
    return state != NULL &&
           state->format_version == BLE_PROFILE_STATE_FORMAT_VERSION &&
           fightpad_ble_profile_valid(state->profile) &&
           (state->pending_flags & ~BLE_PROFILE_PENDING_KNOWN_MASK) == 0u &&
           state->reserved == 0u;
}

bool ble_profile_state_can_migrate(uint8_t format_version)
{
    return format_version >= BLE_PROFILE_STATE_LEGACY_V1 &&
           format_version <= BLE_PROFILE_STATE_LEGACY_V5;
}

bool ble_profile_state_migrate_legacy(
    const ble_profile_persisted_state_t *legacy,
    ble_profile_persisted_state_t *migrated)
{
    if (legacy == NULL || migrated == NULL ||
        !ble_profile_state_can_migrate(legacy->format_version) ||
        !fightpad_ble_profile_valid(legacy->profile) ||
        (legacy->pending_flags & ~BLE_PROFILE_PENDING_KNOWN_MASK) != 0u ||
        legacy->reserved != 0u) {
        return false;
    }

    *migrated = *legacy;
    migrated->format_version = BLE_PROFILE_STATE_FORMAT_VERSION;
    /* A firmware-format migration is not a user-requested Profile change.
     * Preserve the selected Profile and suppress legacy pending actions so an
     * ordinary upgrade/reboot cannot unexpectedly clear bonds or auto-pair. */
    migrated->pending_flags = 0u;
    return true;
}

void ble_profile_decide_mode(
    const ble_profile_persisted_state_t *current_state,
    fightpad_ble_profile_t active_profile,
    ble_profile_phase_t phase,
    fightpad_mode_parse_result_t parse_result,
    const fightpad_ble_profile_mode_t *mode,
    ble_profile_mode_decision_t *decision)
{
    if (decision == NULL) {
        return;
    }

    memset(decision, 0, sizeof(*decision));
    ble_profile_state_default(&decision->next_state);
    if (current_state != NULL && ble_profile_state_valid(current_state)) {
        decision->next_state = *current_state;
    }
    decision->accepted_profile =
        (fightpad_ble_profile_t)decision->next_state.profile;

    if (mode == NULL || parse_result == FIGHTPAD_MODE_PARSE_BAD_FRAME) {
        return;
    }

    decision->respond = true;
    if (parse_result == FIGHTPAD_MODE_PARSE_UNSUPPORTED_VERSION) {
        decision->ack_result = BLE_PROFILE_ACK_UNSUPPORTED_VERSION;
        return;
    }

    fightpad_ble_profile_t target = mode->accepted_profile;
    const bool invalid_fallback =
        parse_result == FIGHTPAD_MODE_PARSE_INVALID_PROFILE;
    if (invalid_fallback) {
        target = FIGHTPAD_BLE_PROFILE_GENERIC;
        decision->ack_result = BLE_PROFILE_ACK_INVALID_FALLBACK;
    } else if (phase == BLE_PROFILE_PHASE_BOOT) {
        decision->ack_result = BLE_PROFILE_ACK_APPLYING_AT_BOOT;
    } else if (target == active_profile) {
        decision->ack_result = BLE_PROFILE_ACK_ACTIVE_UNCHANGED;
    } else {
        decision->ack_result = BLE_PROFILE_ACK_RESTARTING;
    }

    decision->accepted_profile = target;
    const bool force_repair =
        parse_result == FIGHTPAD_MODE_PARSE_OK &&
        (mode->flags & BLE_PROFILE_FLAG_FORCE_REPAIR) != 0u;
    const bool profile_changed = target != active_profile;

    if (profile_changed || force_repair) {
        decision->next_state.profile = (uint8_t)target;
        decision->next_state.pending_flags |=
            BLE_PROFILE_PENDING_CLEAR_BONDS |
            BLE_PROFILE_PENDING_PAIRING;
        decision->persist = true;
        decision->restart = phase == BLE_PROFILE_PHASE_RUNTIME;
        if (decision->restart && !invalid_fallback) {
            decision->ack_result = BLE_PROFILE_ACK_RESTARTING;
        }
    } else if (invalid_fallback) {
        /* Replace a malformed persisted/requested value with canonical
         * Generic even when Generic is already active. */
        decision->next_state.profile = FIGHTPAD_BLE_PROFILE_GENERIC;
        decision->persist = true;
    }
}
