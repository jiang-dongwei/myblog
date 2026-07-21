idf_component_register(
    SRCS "main.c" "gameplay_gate.c"
    INCLUDE_DIRS "."
    REQUIRES esp_hid bt driver
    PRIV_REQUIRES nvs_flash driver
)
