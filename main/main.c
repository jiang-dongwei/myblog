#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_bt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_hidd.h"
#include "esp_hid_common.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

#define DEVICE_NAME "FP12Slim-C6"
#define HID_REPORT_ID_GAMEPAD 1
#define HID_REPORT_MAP_INDEX 0
#define HID_GAMEPAD_REPORT_LEN 5
#define HID_NEUTRAL_HAT 0x08
#define HID_REPORT_INTERVAL_MS 5
#define HID_REPORT_KEEPALIVE_MS 50

#define UART_INPUT_PORT UART_NUM_0
#define UART_INPUT_BAUD 115200
#define UART_INPUT_TX_PIN 16
#define UART_INPUT_RX_PIN 17
#define UART_INPUT_RX_BUFFER_SIZE 512
#define UART_INPUT_FRAME_LEN 8
#define UART_INPUT_MAGIC0 0x46
#define UART_INPUT_MAGIC_REPORT 0x50
#define UART_INPUT_MAGIC_TRANSPORT 0x54
#define UART_INPUT_MAGIC_BATTERY 0x42
#define UART_INPUT_STALE_MS 250
#define UART_DONE_SIGNAL_TEXT "C6_DONE\n"
#define UART_DONE_SIGNAL_REPEAT 20
#define UART_DONE_SIGNAL_INTERVAL_MS 100

#define UART_INPUT_MAGIC_FW_INFO 0x49   /* 'I' */
#define FW_INFO_FLAG_SINGLE  0x00
#define FW_INFO_FLAG_FIRST   0xC0       /* bits 7-6 = 11 */
#define FW_INFO_FLAG_MIDDLE  0x80       /* bits 7-6 = 10 */
#define FW_INFO_FLAG_LAST    0x40       /* bits 7-6 = 01 */
#define FW_INFO_FLAG_MASK    0xC0
#define FW_INFO_SEQ_MASK     0x3F

#define PAIR_BUTTON_GPIO GPIO_NUM_13
#define PAIR_BUTTON_POLL_MS 10
#define PAIR_BUTTON_DEBOUNCE_MS 30
#define PAIRING_WINDOW_MS 60000
#define BRIDGE_FLASH_DIAGNOSTIC_BOOT_PAIRING 0
#define BRIDGE_FLASH_DIAGNOSTIC_IGNORE_TRANSPORT_WHILE_WINDOW 0
#define PAIRING_DIAGNOSTIC_DISABLE_ADV 0
#define PAIRING_DIAGNOSTIC_DISABLE_HID 0

/* CPU architecture name mapping for firmware info */
#if CONFIG_IDF_TARGET_ARCH_RISCV
#define FW_INFO_CPU_ARCH "RISC-V"
#elif CONFIG_IDF_TARGET_ARCH_XTENSA
#define FW_INFO_CPU_ARCH "Xtensa"
#else
#define FW_INFO_CPU_ARCH CONFIG_IDF_TARGET_ARCH
#endif

#define FW_INFO_BOARD_NAME "Fightpad12Slim_C6_BLE_HID"

#define UART_DPAD_UP 0x01
#define UART_DPAD_DOWN 0x02
#define UART_DPAD_LEFT 0x04
#define UART_DPAD_RIGHT 0x08

static const char *TAG = "FP12_C6_HID";

extern void ble_store_config_init(void);

static esp_hidd_dev_t *s_hid_dev;
static TaskHandle_t s_report_task;
static TaskHandle_t s_uart_task;
static TaskHandle_t s_done_signal_task;
static TaskHandle_t s_pair_button_task;
static uint8_t s_own_addr_type;
static bool s_addr_ready;
static bool s_addr_logged;
static bool s_uart_seen_frame;
static bool s_transport_mode_seen;
static bool s_transport_bt_enabled;
static bool s_hid_started;
static bool s_pairing_window_open;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static TickType_t s_last_uart_tick;
static TickType_t s_pairing_window_deadline;
static bool s_have_uart_report;
static uint8_t s_battery_level = 100;
static bool s_battery_usb_power_present;
static int s_pair_button_idle_level = -1;
static portMUX_TYPE s_report_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_transport_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_pairing_lock = portMUX_INITIALIZER_UNLOCKED;

static const uint8_t neutral_report[HID_GAMEPAD_REPORT_LEN] = {
    0x00, 0x00,     // buttons 1-16 released
    HID_NEUTRAL_HAT,// hat switch neutral/null, high nibble padding
    0x00, 0x00,     // X/Y centered
};

static uint8_t s_current_report[HID_GAMEPAD_REPORT_LEN] = {
    0x00, 0x00,
    HID_NEUTRAL_HAT,
    0x00, 0x00,
};

typedef struct {
    uint8_t data[UART_INPUT_FRAME_LEN];
    uint8_t pos;
} uart_frame_parser_t;

static const uint8_t gamepad_report_map[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x05,                    // Usage (Game Pad)
    0xA1, 0x01,                    // Collection (Application)
    0x85, HID_REPORT_ID_GAMEPAD,   //   Report ID

    0x05, 0x09,                    //   Usage Page (Button)
    0x19, 0x01,                    //   Usage Minimum (Button 1)
    0x29, 0x10,                    //   Usage Maximum (Button 16)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x10,                    //   Report Count (16)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    0x05, 0x01,                    //   Usage Page (Generic Desktop)
    0x09, 0x39,                    //   Usage (Hat Switch)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x07,                    //   Logical Maximum (7)
    0x35, 0x00,                    //   Physical Minimum (0)
    0x46, 0x3B, 0x01,              //   Physical Maximum (315)
    0x65, 0x14,                    //   Unit (English Rotation, degrees)
    0x75, 0x04,                    //   Report Size (4)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x42,                    //   Input (Data,Var,Abs,Null State)
    0x65, 0x00,                    //   Unit (None)

    0x75, 0x04,                    //   Report Size (4)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x03,                    //   Input (Const,Var,Abs)

    0x09, 0x30,                    //   Usage (X)
    0x09, 0x31,                    //   Usage (Y)
    0x15, 0x81,                    //   Logical Minimum (-127)
    0x25, 0x7F,                    //   Logical Maximum (127)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    0xC0                           // End Collection
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {
        .data = gamepad_report_map,
        .len = sizeof(gamepad_report_map),
    },
};

static esp_hid_device_config_t hid_config = {
    .vendor_id = 0x1209,
    .product_id = 0x2040,
    .version = 0x0001,
    .device_name = DEVICE_NAME,
    .manufacturer_name = "Fightpad Bringup",
    .serial_number = "ESP32C6-BLE-HID-TEST",
    .report_maps = ble_report_maps,
    .report_maps_len = 1,
};

static void start_advertising(void);
static void set_transport_bt_enabled(bool enabled);
static void trigger_pairing_mode(void);

static void open_pairing_window(void)
{
    TickType_t now = xTaskGetTickCount();

    taskENTER_CRITICAL(&s_pairing_lock);

    s_pairing_window_open = true;
    s_pairing_window_deadline = now + pdMS_TO_TICKS(PAIRING_WINDOW_MS);
    taskEXIT_CRITICAL(&s_pairing_lock);
}

static void close_pairing_window(void)
{
    taskENTER_CRITICAL(&s_pairing_lock);
    s_pairing_window_open = false;
    s_pairing_window_deadline = 0;
    taskEXIT_CRITICAL(&s_pairing_lock);
}

static bool pairing_window_active(void)
{
    bool active;
    TickType_t now = xTaskGetTickCount();

    taskENTER_CRITICAL(&s_pairing_lock);
    active = s_pairing_window_open && (int32_t)(s_pairing_window_deadline - now) > 0;
    if (!active) {
        s_pairing_window_open = false;
        s_pairing_window_deadline = 0;
    }
    taskEXIT_CRITICAL(&s_pairing_lock);

    return active;
}

static uint8_t hat_from_dpad(uint8_t dpad)
{
    bool up = (dpad & UART_DPAD_UP) != 0;
    bool down = (dpad & UART_DPAD_DOWN) != 0;
    bool left = (dpad & UART_DPAD_LEFT) != 0;
    bool right = (dpad & UART_DPAD_RIGHT) != 0;

    if (up && down) {
        up = false;
        down = false;
    }
    if (left && right) {
        left = false;
        right = false;
    }

    if (up && right) {
        return 1;
    }
    if (down && right) {
        return 3;
    }
    if (down && left) {
        return 5;
    }
    if (up && left) {
        return 7;
    }
    if (up) {
        return 0;
    }
    if (right) {
        return 2;
    }
    if (down) {
        return 4;
    }
    if (left) {
        return 6;
    }

    return HID_NEUTRAL_HAT;
}

static void compose_hid_report(uint16_t buttons, uint8_t dpad, int8_t x, int8_t y,
                               uint8_t report[HID_GAMEPAD_REPORT_LEN])
{
    report[0] = buttons & 0xFF;
    report[1] = (buttons >> 8) & 0xFF;
    report[2] = hat_from_dpad(dpad) & 0x0F;
    report[3] = (uint8_t)x;
    report[4] = (uint8_t)y;
}

static bool set_current_report(const uint8_t report[HID_GAMEPAD_REPORT_LEN])
{
    bool changed;

    taskENTER_CRITICAL(&s_report_lock);
    changed = memcmp(s_current_report, report, HID_GAMEPAD_REPORT_LEN) != 0;
    memcpy(s_current_report, report, HID_GAMEPAD_REPORT_LEN);
    s_last_uart_tick = xTaskGetTickCount();
    s_have_uart_report = true;
    taskEXIT_CRITICAL(&s_report_lock);

    return changed;
}

static void copy_current_report(uint8_t report[HID_GAMEPAD_REPORT_LEN])
{
    taskENTER_CRITICAL(&s_report_lock);
    memcpy(report, s_current_report, HID_GAMEPAD_REPORT_LEN);
    taskEXIT_CRITICAL(&s_report_lock);
}

static bool transport_bt_enabled(void)
{
    bool enabled;

    taskENTER_CRITICAL(&s_transport_lock);
    enabled = s_transport_bt_enabled;
    taskEXIT_CRITICAL(&s_transport_lock);

    return enabled;
}

static bool neutralize_stale_uart_report(void)
{
    bool changed = false;
    TickType_t now = xTaskGetTickCount();

    taskENTER_CRITICAL(&s_report_lock);
    if (s_have_uart_report &&
        (now - s_last_uart_tick) > pdMS_TO_TICKS(UART_INPUT_STALE_MS)) {
        changed = memcmp(s_current_report, neutral_report, HID_GAMEPAD_REPORT_LEN) != 0;
        memcpy(s_current_report, neutral_report, HID_GAMEPAD_REPORT_LEN);
        s_have_uart_report = false;
    }
    taskEXIT_CRITICAL(&s_report_lock);

    return changed;
}

static void set_transport_bt_enabled(bool enabled)
{
    bool changed;

    taskENTER_CRITICAL(&s_transport_lock);
    changed = !s_transport_mode_seen || s_transport_bt_enabled != enabled;
    s_transport_mode_seen = true;
    s_transport_bt_enabled = enabled;
    taskEXIT_CRITICAL(&s_transport_lock);

    if (changed) {
        ESP_LOGI(TAG, "transport mode: %s", enabled ? "bluetooth" : "usb");
    }

    if (enabled) {
        if (s_hid_started) {
            start_advertising();
        }
        return;
    }

    set_current_report(neutral_report);

    if (!s_hid_started) {
        return;
    }

    if (ble_gap_adv_active()) {
        int rc = ble_gap_adv_stop();
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_gap_adv_stop failed rc=%d", rc);
        }
    }

    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_gap_terminate failed rc=%d", rc);
        }
    }
}

static void trigger_pairing_mode(void)
{
    if (s_transport_mode_seen && !transport_bt_enabled()) {
        ESP_LOGI(TAG, "pair button ignored: transport mode disables bluetooth");
        close_pairing_window();
        return;
    }

    if (!s_transport_mode_seen) {
        ESP_LOGI(TAG, "pair button: transport mode not seen yet, defaulting to bluetooth");
        set_transport_bt_enabled(true);
    }

    if (pairing_window_active()) {
        ESP_LOGI(TAG, "pair button event: pairing window already open, ensuring advertising");
        if (transport_bt_enabled() && s_hid_started) {
            start_advertising();
        }
        return;
    }

    ESP_LOGI(TAG, "pair button pressed: opening %d ms pairing window", PAIRING_WINDOW_MS);

    open_pairing_window();

    if (ble_gap_adv_active()) {
        int rc = ble_gap_adv_stop();
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_gap_adv_stop failed rc=%d", rc);
        }
    }

    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_gap_terminate failed rc=%d", rc);
        }
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }

    int rc = ble_store_clear();
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_store_clear failed rc=%d", rc);
    }

    if (transport_bt_enabled() && s_hid_started) {
        start_advertising();
    }
}

static void pair_button_task(void *arg)
{
    bool stable_pressed = false;
    bool last_raw_pressed = false;
    TickType_t last_change_tick = xTaskGetTickCount();

    if (s_pair_button_idle_level < 0) {
        s_pair_button_idle_level = gpio_get_level(PAIR_BUTTON_GPIO);
        ESP_LOGI(TAG, "pair button idle level=%d", s_pair_button_idle_level);
    }

    while (1) {
        bool raw_pressed = gpio_get_level(PAIR_BUTTON_GPIO) != s_pair_button_idle_level;
        TickType_t now = xTaskGetTickCount();

        if (raw_pressed != last_raw_pressed) {
            last_raw_pressed = raw_pressed;
            last_change_tick = now;
        }

        if ((now - last_change_tick) >= pdMS_TO_TICKS(PAIR_BUTTON_DEBOUNCE_MS) && raw_pressed != stable_pressed) {
            stable_pressed = raw_pressed;
            ESP_LOGI(TAG, "pair button debounced: %s", stable_pressed ? "pressed" : "released");
            trigger_pairing_mode();
        }

        if (pairing_window_active() &&
            transport_bt_enabled() &&
            s_hid_started &&
            s_conn_handle == BLE_HS_CONN_HANDLE_NONE &&
            !ble_gap_adv_active()) {
            start_advertising();
        }

        if (!pairing_window_active() && ble_gap_adv_active()) {
            ESP_LOGI(TAG, "pairing window expired: stopping advertising");
            int rc = ble_gap_adv_stop();
            if (rc != 0) {
                ESP_LOGW(TAG, "ble_gap_adv_stop failed rc=%d", rc);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(PAIR_BUTTON_POLL_MS));
    }
}

static esp_err_t send_hid_report(const uint8_t report[HID_GAMEPAD_REPORT_LEN])
{
    return esp_hidd_dev_input_set(
        s_hid_dev,
        HID_REPORT_MAP_INDEX,
        HID_REPORT_ID_GAMEPAD,
        (uint8_t *)report,
        HID_GAMEPAD_REPORT_LEN
    );
}

static void update_hid_battery_level(uint8_t level)
{
    s_battery_level = level;

    if (s_hid_dev == NULL) {
        return;
    }

    esp_err_t err = esp_hidd_dev_battery_set(s_hid_dev, s_battery_level);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "battery update skipped: %s", esp_err_to_name(err));
    }
}

static uint8_t uart_frame_checksum(const uint8_t frame[UART_INPUT_FRAME_LEN])
{
    uint8_t checksum = 0;

    for (uint8_t i = 0; i < UART_INPUT_FRAME_LEN - 1; i++) {
        checksum ^= frame[i];
    }

    return checksum;
}

static void handle_uart_frame(const uint8_t frame[UART_INPUT_FRAME_LEN])
{
    uint8_t expected_checksum = uart_frame_checksum(frame);
    if (frame[UART_INPUT_FRAME_LEN - 1] != expected_checksum) {
        ESP_LOGD(TAG, "UART frame checksum mismatch got=0x%02x expected=0x%02x",
                 frame[UART_INPUT_FRAME_LEN - 1], expected_checksum);
        return;
    }

    if (frame[1] == UART_INPUT_MAGIC_TRANSPORT) {
#if BRIDGE_FLASH_DIAGNOSTIC_IGNORE_TRANSPORT_WHILE_WINDOW
        if (pairing_window_active()) {
            ESP_LOGI(TAG, "diagnostic: ignoring transport frame during pairing window");
            return;
        }
#endif
        set_transport_bt_enabled(frame[2] != 0);
        return;
    }

    if (frame[1] == UART_INPUT_MAGIC_BATTERY) {
        s_battery_usb_power_present = frame[3] != 0;
        update_hid_battery_level(frame[2]);
        ESP_LOGD(TAG, "battery frame level=%u usb=%u raw=0x%02x%02x",
                 frame[2],
                 frame[3],
                 frame[5],
                 frame[4]);
        return;
    }

    if (frame[1] != UART_INPUT_MAGIC_REPORT) {
        return;
    }

    if (!transport_bt_enabled()) {
        set_current_report(neutral_report);
        return;
    }

    uint16_t buttons = frame[2] | ((uint16_t)frame[3] << 8);
    uint8_t dpad = frame[4];
    int8_t x = (int8_t)frame[5];
    int8_t y = (int8_t)frame[6];
    uint8_t report[HID_GAMEPAD_REPORT_LEN];

    compose_hid_report(buttons, dpad, x, y, report);
    bool changed = set_current_report(report);

    if (!s_uart_seen_frame) {
        ESP_LOGI(TAG, "UART input frame received");
        s_uart_seen_frame = true;
    }
    if (changed) {
        ESP_LOGD(TAG, "UART report buttons=0x%04x dpad=0x%02x x=%d y=%d",
                 buttons, dpad, x, y);
    }
}

static void parse_uart_byte(uart_frame_parser_t *parser, uint8_t value)
{
    if (parser->pos == 0) {
        if (value == UART_INPUT_MAGIC0) {
            parser->data[parser->pos++] = value;
        }
        return;
    }

    if (parser->pos == 1) {
        if (value == UART_INPUT_MAGIC_REPORT ||
            value == UART_INPUT_MAGIC_TRANSPORT ||
            value == UART_INPUT_MAGIC_BATTERY) {
            parser->data[parser->pos++] = value;
        } else if (value == UART_INPUT_MAGIC0) {
            parser->data[0] = value;
            parser->pos = 1;
        } else {
            parser->pos = 0;
        }
        return;
    }

    parser->data[parser->pos++] = value;
    if (parser->pos >= UART_INPUT_FRAME_LEN) {
        handle_uart_frame(parser->data);
        parser->pos = 0;
    }
}

static void uart_input_task(void *arg)
{
    uint8_t buffer[64];
    uart_frame_parser_t parser = {0};

    while (true) {
        int len = uart_read_bytes(
            UART_INPUT_PORT,
            buffer,
            sizeof(buffer),
            pdMS_TO_TICKS(20)
        );

        for (int i = 0; i < len; i++) {
            parse_uart_byte(&parser, buffer[i]);
        }
    }
}

static esp_err_t init_input_uart(void)
{
    if (s_uart_task != NULL) {
        return ESP_OK;
    }

    uart_config_t uart_config = {
        .baud_rate = UART_INPUT_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(
        UART_INPUT_PORT,
        UART_INPUT_RX_BUFFER_SIZE,
        0,
        0,
        NULL,
        0
    );
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(err, TAG, "UART driver install failed");
    }

    ESP_RETURN_ON_ERROR(uart_param_config(UART_INPUT_PORT, &uart_config),
                        TAG, "UART param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(UART_INPUT_PORT,
                                     UART_INPUT_TX_PIN,
                                     UART_INPUT_RX_PIN,
                                     UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE),
                        TAG, "UART pin config failed");

    xTaskCreate(uart_input_task, "uart_input_task", 3072, NULL, 6, &s_uart_task);
    ESP_LOGI(TAG, "UART input ready on U0 GPIO%d/GPIO%d at %d baud",
             UART_INPUT_TX_PIN, UART_INPUT_RX_PIN, UART_INPUT_BAUD);

    return ESP_OK;
}

static void uart_done_signal_task(void *arg)
{
    const char *signal = UART_DONE_SIGNAL_TEXT;
    const size_t signal_len = strlen(signal);

    for (int i = 0; i < UART_DONE_SIGNAL_REPEAT; i++) {
        uart_write_bytes(UART_INPUT_PORT, signal, signal_len);
        vTaskDelay(pdMS_TO_TICKS(UART_DONE_SIGNAL_INTERVAL_MS));
    }

    s_done_signal_task = NULL;
    vTaskDelete(NULL);
}

static void start_done_signal_task(void)
{
    if (s_done_signal_task != NULL) {
        return;
    }

    xTaskCreate(
        uart_done_signal_task,
        "uart_done_signal_task",
        2048,
        NULL,
        4,
        &s_done_signal_task
    );
}

/* ── Firmware info frames ─────────────────────────────────── */

static int fw_info_build_payload(char *buf, size_t buf_size)
{
    const char *idf_ver = esp_get_idf_version();
    /* Skip leading 'v' if present (e.g. "v5.5.1" -> "5.5.1") */
    const char *sdk_ver = (idf_ver[0] == 'v' || idf_ver[0] == 'V')
                              ? idf_ver + 1
                              : idf_ver;

    return snprintf(buf, buf_size,
                    "SDK=%s\n"
                    "Plat=" CONFIG_IDF_TARGET "\n"
                    "Board=" FW_INFO_BOARD_NAME "\n"
                    "CPU=" FW_INFO_CPU_ARCH "\n",
                    sdk_ver);
}

static void send_fw_info_frames(const char *payload, int len)
{
    int total_frames = (len + 3) / 4;  /* 4 data bytes per frame */
    uint8_t frame[UART_INPUT_FRAME_LEN];

    for (int seq = 0; seq < total_frames; seq++) {
        memset(frame, 0, UART_INPUT_FRAME_LEN);
        frame[0] = UART_INPUT_MAGIC0;
        frame[1] = UART_INPUT_MAGIC_FW_INFO;

        /* Set flag (bits 7-6) + sequence (bits 5-0) in byte 2 */
        if (total_frames == 1) {
            frame[2] = FW_INFO_FLAG_SINGLE;
        } else if (seq == 0) {
            frame[2] = FW_INFO_FLAG_FIRST;
        } else if (seq == total_frames - 1) {
            frame[2] = FW_INFO_FLAG_LAST | (seq & FW_INFO_SEQ_MASK);
        } else {
            frame[2] = FW_INFO_FLAG_MIDDLE | (seq & FW_INFO_SEQ_MASK);
        }

        /* Copy up to 4 bytes of payload */
        int offset = seq * 4;
        int remaining = len - offset;
        int copy_len = remaining < 4 ? remaining : 4;
        memcpy(&frame[3], &payload[offset], copy_len);

        /* XOR checksum over bytes 0-6 */
        frame[7] = uart_frame_checksum(frame);

        uart_write_bytes(UART_INPUT_PORT, (const char *)frame, UART_INPUT_FRAME_LEN);
    }
}

static void report_task(void *arg)
{
    uint8_t report[HID_GAMEPAD_REPORT_LEN];
    uint8_t last_sent[HID_GAMEPAD_REPORT_LEN] = {0};
    bool have_last_sent = false;
    TickType_t last_sent_tick = 0;

    while (true) {
        if (neutralize_stale_uart_report()) {
            ESP_LOGW(TAG, "UART input timeout; neutral report restored");
        }

        copy_current_report(report);

        if (s_hid_dev != NULL && esp_hidd_dev_connected(s_hid_dev)) {
            TickType_t now = xTaskGetTickCount();
            bool changed = !have_last_sent ||
                           memcmp(report, last_sent, HID_GAMEPAD_REPORT_LEN) != 0;
            bool keepalive = have_last_sent &&
                             (now - last_sent_tick) >= pdMS_TO_TICKS(HID_REPORT_KEEPALIVE_MS);

            if (changed || keepalive) {
                esp_err_t err = send_hid_report(report);
                if (err == ESP_OK) {
                    memcpy(last_sent, report, HID_GAMEPAD_REPORT_LEN);
                    have_last_sent = true;
                    last_sent_tick = now;
                } else {
                    ESP_LOGD(TAG, "HID report send skipped: %s", esp_err_to_name(err));
                }
            }
        } else {
            have_last_sent = false;
        }

        vTaskDelay(pdMS_TO_TICKS(HID_REPORT_INTERVAL_MS));
    }
}

static void ensure_report_task_running(void)
{
    if (s_report_task != NULL) {
        return;
    }

    xTaskCreate(report_task, "hid_report_task", 3072, NULL, 5, &s_report_task);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE connect %s status=%d",
                 event->connect.status == 0 ? "ok" : "failed",
                 event->connect.status);
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            close_pairing_window();
            if (!transport_bt_enabled()) {
                ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
        } else {
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnect reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "BLE connection updated status=%d", event->conn_update.status);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertising complete reason=%d", event->adv_complete.reason);
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "BLE encryption changed status=%d", event->enc_change.status);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "BLE subscribe conn=%d attr=%d notify=%d indicate=%d",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify,
                 event->subscribe.cur_indicate);
        ensure_report_task_running();
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    default:
        return 0;
    }
}

static bool ensure_ble_address(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed rc=%d", rc);
        return false;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed rc=%d", rc);
        return false;
    }

    s_addr_ready = true;

    if (!s_addr_logged) {
        uint8_t addr_val[6] = {0};
        rc = ble_hs_id_copy_addr(s_own_addr_type, addr_val, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_hs_id_copy_addr failed rc=%d", rc);
            return true;
        }

        ESP_LOGI(TAG, "BLE address %02x:%02x:%02x:%02x:%02x:%02x",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
        s_addr_logged = true;
    }

    return true;
}

static void start_advertising(void)
{
#if PAIRING_DIAGNOSTIC_DISABLE_ADV
    ESP_LOGI(TAG, "diagnostic: advertising disabled in firmware");
    return;
#endif

    if (!s_hid_started || !transport_bt_enabled() || !pairing_window_active()) {
        return;
    }

    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    if (ble_gap_adv_active()) {
        return;
    }

    if (!s_addr_ready && !ensure_ble_address()) {
        return;
    }

    ble_uuid16_t hid_service_uuid = BLE_UUID16_INIT(0x1812);
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = ESP_HID_APPEARANCE_GAMEPAD;
    fields.appearance_is_present = 1;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.uuids16 = &hid_service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(200);

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "advertising as %s", DEVICE_NAME);
}

static void host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID device started");
        s_hid_started = true;
        update_hid_battery_level(s_battery_level);
        start_advertising();
        break;

    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID connected status=%s", esp_err_to_name(param->connect.status));
        update_hid_battery_level(s_battery_level);
        ensure_report_task_running();
        break;

    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID disconnected reason=%d", param->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        start_advertising();
        break;

    case ESP_HIDD_PROTOCOL_MODE_EVENT:
        ESP_LOGI(TAG, "HID protocol mode=%u", param->protocol_mode.protocol_mode);
        break;

    case ESP_HIDD_CONTROL_EVENT:
        ESP_LOGI(TAG, "HID control=%u", param->control.control);
        break;

    case ESP_HIDD_OUTPUT_EVENT:
        ESP_LOGI(TAG, "HID output report id=%u len=%u",
                 param->output.report_id,
                 param->output.length);
        break;

    case ESP_HIDD_STOP_EVENT:
        ESP_LOGI(TAG, "HID device stopped");
        s_hid_started = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        break;

    default:
        break;
    }
}

static void configure_security(void)
{
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
}

static esp_err_t init_nimble_controller(void)
{
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "classic BT mem release skipped: %s", esp_err_to_name(ret));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&bt_cfg), TAG, "controller init failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BLE), TAG, "controller enable failed");
    ESP_RETURN_ON_ERROR(esp_nimble_init(), TAG, "nimble init failed");

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "app_main: start");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "app_main: nvs ready");

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG, "app_main: event loop ready");

    ESP_ERROR_CHECK(init_nimble_controller());
    ESP_LOGI(TAG, "app_main: nimble controller ready");

    gpio_config_t pair_button_config = {
        .pin_bit_mask = 1ULL << PAIR_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pair_button_config));
    s_pair_button_idle_level = gpio_get_level(PAIR_BUTTON_GPIO);
    ESP_LOGI(TAG, "app_main: pair button gpio ready on GPIO%d idle=%d",
             PAIR_BUTTON_GPIO,
             s_pair_button_idle_level);

    ESP_ERROR_CHECK(init_input_uart());
    ESP_LOGI(TAG, "app_main: uart ready");

    /* Send firmware info frames before C6_DONE signal */
    {
        char payload[128];
        int len = fw_info_build_payload(payload, sizeof(payload));
        if (len > 0 && len < (int)sizeof(payload)) {
            send_fw_info_frames(payload, len);
            ESP_LOGI(TAG, "FW info sent: %d bytes in %d frames",
                     len, (len + 3) / 4);
        } else {
            ESP_LOGW(TAG, "FW info payload build failed len=%d", len);
        }
    }

    start_done_signal_task();
    ESP_LOGI(TAG, "app_main: done signal task started");

    ble_svc_gap_device_name_set(DEVICE_NAME);
    configure_security();
    ESP_LOGI(TAG, "app_main: ble config ready for %s", DEVICE_NAME);

#if BRIDGE_FLASH_DIAGNOSTIC_BOOT_PAIRING
    ESP_LOGI(TAG, "diagnostic: opening boot pairing window for bridge flash verification");
    open_pairing_window();
    set_transport_bt_enabled(true);
#endif

#if PAIRING_DIAGNOSTIC_DISABLE_HID
    ESP_LOGI(TAG, "diagnostic: HID/BLE app start disabled");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    ESP_ERROR_CHECK(esp_hidd_dev_init(
        &hid_config,
        ESP_HID_TRANSPORT_BLE,
        hidd_event_callback,
        &s_hid_dev
    ));
    ESP_LOGI(TAG, "app_main: hid init requested");

    ble_store_config_init();
    ESP_LOGI(TAG, "app_main: ble store ready");
    ESP_ERROR_CHECK(esp_nimble_enable(host_task));
    ESP_LOGI(TAG, "app_main: nimble host enabled");

    xTaskCreate(pair_button_task, "pair_button_task", 2048, NULL, 5, &s_pair_button_task);

    /* Boot: default transport to BT if RP2350 hasn't set it yet, then open
     * pairing window so BLE is discoverable + modem sleep has an RF schedule */
    if (!s_transport_mode_seen) {
        set_transport_bt_enabled(true);
    }
    open_pairing_window();

    ESP_LOGI(TAG, "app_main: pair button task started");
}
