idf_component_register(
    SRCS "ble_profiles.c" "ble_profile_store.c" "ble_profile_state.c" "uart_protocol.c" "main.c" "gameplay_gate.c"
    INCLUDE_DIRS "."
    REQUIRES esp_hid bt driver
    PRIV_REQUIRES nvs_flash driver
)
