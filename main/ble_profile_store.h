#ifndef FIGHTPAD_BLE_PROFILE_STORE_H
#define FIGHTPAD_BLE_PROFILE_STORE_H

#include "ble_profile_state.h"

#include "esp_err.h"

typedef enum {
    BLE_PROFILE_STORE_LOADED = 0,
    BLE_PROFILE_STORE_MIGRATED,
    BLE_PROFILE_STORE_DEFAULT_MISSING,
    BLE_PROFILE_STORE_DEFAULT_INVALID,
} ble_profile_store_load_result_t;

esp_err_t ble_profile_store_load(
    ble_profile_persisted_state_t *state,
    ble_profile_store_load_result_t *load_result);

esp_err_t ble_profile_store_save(
    const ble_profile_persisted_state_t *state);

#endif
