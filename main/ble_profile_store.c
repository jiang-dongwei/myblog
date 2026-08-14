#include "ble_profile_store.h"

#include <stddef.h>

#include "nvs.h"

#define BLE_PROFILE_NVS_NAMESPACE "fp_ble_prof"
#define BLE_PROFILE_NVS_KEY "state"

esp_err_t ble_profile_store_save(
    const ble_profile_persisted_state_t *state)
{
    if (!ble_profile_state_valid(state)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BLE_PROFILE_NVS_NAMESPACE,
                             NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, BLE_PROFILE_NVS_KEY,
                       state, sizeof(*state));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t ble_profile_store_load(
    ble_profile_persisted_state_t *state,
    ble_profile_store_load_result_t *load_result)
{
    if (state == NULL || load_result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BLE_PROFILE_NVS_NAMESPACE,
                             NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ble_profile_state_default(state);
        *load_result = BLE_PROFILE_STORE_DEFAULT_MISSING;
        return ble_profile_store_save(state);
    }
    if (err != ESP_OK) {
        return err;
    }

    size_t size = sizeof(*state);
    err = nvs_get_blob(handle, BLE_PROFILE_NVS_KEY, state, &size);
    nvs_close(handle);

    if (err == ESP_OK && size == sizeof(*state) &&
        ble_profile_state_can_migrate(state->format_version)) {
        ble_profile_persisted_state_t migrated;
        if (ble_profile_state_migrate_legacy(state, &migrated)) {
            *state = migrated;
            *load_result = BLE_PROFILE_STORE_MIGRATED;
            return ble_profile_store_save(state);
        }
    }

    if (err == ESP_ERR_NVS_NOT_FOUND ||
        err == ESP_ERR_NVS_INVALID_LENGTH ||
        (err == ESP_OK &&
         (size != sizeof(*state) || !ble_profile_state_valid(state)))) {
        ble_profile_state_default(state);
        *load_result = err == ESP_ERR_NVS_NOT_FOUND
                           ? BLE_PROFILE_STORE_DEFAULT_MISSING
                           : BLE_PROFILE_STORE_DEFAULT_INVALID;
        return ble_profile_store_save(state);
    }
    if (err != ESP_OK) {
        return err;
    }

    *load_result = BLE_PROFILE_STORE_LOADED;
    return ESP_OK;
}
