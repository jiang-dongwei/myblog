#include <assert.h>
#include <stdio.h>

#include "ble_profile_state.h"

static fightpad_ble_profile_mode_t xbox_mode(uint8_t flags)
{
    fightpad_ble_profile_mode_t mode = {
        .version = FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION,
        .requested_profile = FIGHTPAD_BLE_PROFILE_XBOX,
        .accepted_profile = FIGHTPAD_BLE_PROFILE_XBOX,
        .sequence = 0x2A,
        .flags = flags,
    };
    return mode;
}

static void test_defaults_and_migration(void)
{
    ble_profile_persisted_state_t state;
    ble_profile_state_default(&state);
    assert(ble_profile_state_valid(&state));
    assert(state.profile == FIGHTPAD_BLE_PROFILE_XBOX);
    assert(state.pending_flags == 0u);
    assert(!ble_profile_state_can_migrate(0u));
    assert(!ble_profile_state_can_migrate(BLE_PROFILE_STATE_FORMAT_VERSION));

    for (uint8_t legacy_version = BLE_PROFILE_STATE_LEGACY_V1;
         legacy_version <= BLE_PROFILE_STATE_LEGACY_V5; ++legacy_version) {
        ble_profile_persisted_state_t legacy = {
            .format_version = legacy_version,
            .profile = FIGHTPAD_BLE_PROFILE_PS5_PC,
        };
        assert(ble_profile_state_can_migrate(legacy_version));
        ble_profile_persisted_state_t migrated;
        assert(ble_profile_state_migrate_legacy(&legacy, &migrated));
        assert(migrated.format_version == BLE_PROFILE_STATE_FORMAT_VERSION);
        assert(migrated.profile == FIGHTPAD_BLE_PROFILE_PS5_PC);
        assert(migrated.pending_flags == 0u);
    }
}

static void test_boot_and_runtime_decisions(void)
{
    ble_profile_persisted_state_t state;
    ble_profile_mode_decision_t decision;
    fightpad_ble_profile_mode_t mode = xbox_mode(BLE_PROFILE_FLAG_APPLY_NOW);
    ble_profile_state_default(&state);

    ble_profile_decide_mode(&state, FIGHTPAD_BLE_PROFILE_GENERIC,
                            BLE_PROFILE_PHASE_BOOT,
                            FIGHTPAD_MODE_PARSE_OK, &mode, &decision);
    assert(decision.respond && decision.persist && !decision.restart);
    assert(decision.ack_result == BLE_PROFILE_ACK_APPLYING_AT_BOOT);
    assert(decision.next_state.profile == FIGHTPAD_BLE_PROFILE_XBOX);
    assert(decision.next_state.pending_flags ==
           (BLE_PROFILE_PENDING_CLEAR_BONDS |
            BLE_PROFILE_PENDING_PAIRING));

    state = decision.next_state;
    state.pending_flags = 0;
    ble_profile_decide_mode(&state, FIGHTPAD_BLE_PROFILE_XBOX,
                            BLE_PROFILE_PHASE_RUNTIME,
                            FIGHTPAD_MODE_PARSE_OK, &mode, &decision);
    assert(decision.respond && !decision.persist && !decision.restart);
    assert(decision.ack_result == BLE_PROFILE_ACK_ACTIVE_UNCHANGED);

    mode.flags |= BLE_PROFILE_FLAG_FORCE_REPAIR;
    ble_profile_decide_mode(&state, FIGHTPAD_BLE_PROFILE_XBOX,
                            BLE_PROFILE_PHASE_RUNTIME,
                            FIGHTPAD_MODE_PARSE_OK, &mode, &decision);
    assert(decision.persist && decision.restart);
    assert(decision.next_state.pending_flags ==
           (BLE_PROFILE_PENDING_CLEAR_BONDS |
            BLE_PROFILE_PENDING_PAIRING));
}

int main(void)
{
    test_defaults_and_migration();
    test_boot_and_runtime_decisions();
    puts("ble_profile_state_test: PASS");
    return 0;
}
