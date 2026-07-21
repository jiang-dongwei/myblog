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

#include "gameplay_gate.h"

#define DEVICE_NAME "FP12Slim-C6"
#define HID_REPORT_ID_GAMEPAD 1
#define HID_REPORT_MAP_INDEX 0
#define HID_GAMEPAD_REPORT_LEN 5
#define HID_NEUTRAL_HAT 0x08
#define HID_REPORT_INTERVAL_MS 10
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
#define UART_MAGIC_BLE_STATUS 0x53      /* 'S' — BLE connection status */
#define BLE_STATUS_DISCONNECTED 0x00    /* 未连接 */
#define BLE_STATUS_CONNECTING   0x01    /* 连接中 */
#define BLE_STATUS_CONNECTED    0x02    /* 已连接 */
#define BLE_STATUS_PAIRING      0x03    /* 配对模式 */
#define ADV_ACTIVE_ITVL_MIN_MS   30      /* 有真实按键活动时快广播 */
#define ADV_ACTIVE_ITVL_MAX_MS   50
#define ADV_IDLE_ITVL_MS         200     /* 无真实按键活动时固定慢广播 */
#define REAL_INPUT_IDLE_MS       60000
#define DIRECTED_ADV_TIMEOUT_MS  1280
#define ADV_RETRY_MS             100
#define ADV_RESOURCE_RETRY_MS    1000
#define UART_AXIS_DEADZONE       8
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
static uint8_t s_ble_status = 0xFF;  /* deliberately invalid init value — forces first send */
static TickType_t s_last_real_input_tick;
static uint32_t s_previous_input_mask;
static bool s_link_connected;
static bool s_hid_connected;
static bool s_terminate_requested;
static bool s_pairing_status_requested;
static uint32_t s_directed_request_seq;
static gameplay_gate_t s_gameplay_gate = GAMEPLAY_GATE_DRAIN;
static ble_addr_t s_peer_addr;              /* last connected peer identity address */
static bool s_have_peer_addr;
static portMUX_TYPE s_report_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_transport_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_pairing_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_ble_state_lock = portMUX_INITIALIZER_UNLOCKED;

typedef enum {
    ADV_MODE_OFF = 0,
    ADV_MODE_FAST,
    ADV_MODE_SLOW,
    ADV_MODE_DIRECTED_HIGH,
    ADV_MODE_DIRECTED_LOW,
} adv_mode_t;

typedef struct {
    bool link_connected;
    bool hid_connected;
    bool hid_started;
    bool terminate_requested;
    uint16_t conn_handle;
    uint32_t directed_request_seq;
    bool have_peer_addr;
    ble_addr_t peer_addr;
} ble_state_snapshot_t;

/* Advertising is started and stopped only by pair_button_task(). */
static adv_mode_t s_adv_mode = ADV_MODE_OFF;
static uint32_t s_directed_handled_seq;
static TickType_t s_adv_retry_after_tick;

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

/* ble_gap may reuse advertising fields during controller connection reattempts,
 * so referenced data must have static lifetime. */
static ble_uuid16_t s_hid_service_uuid = BLE_UUID16_INIT(0x1812);

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

static int start_advertising(bool fast);
static int start_directed_advertising(const ble_addr_t *peer_addr, bool high_duty);
static void manage_advertising(TickType_t now, bool pairing_active);
static void set_transport_bt_enabled(bool enabled);
static void send_ble_status_frame(uint8_t status);
static void trigger_pairing_mode(void);

static void open_pairing_window(bool show_pairing_status)
{
    TickType_t now = xTaskGetTickCount();

    taskENTER_CRITICAL(&s_pairing_lock);

    s_pairing_window_open = true;
    s_pairing_window_deadline = now + pdMS_TO_TICKS(PAIRING_WINDOW_MS);
    if (show_pairing_status) {
        s_pairing_status_requested = true;
    }
    taskEXIT_CRITICAL(&s_pairing_lock);
}

static void close_pairing_window(void)
{
    taskENTER_CRITICAL(&s_pairing_lock);
    s_pairing_window_open = false;
    s_pairing_window_deadline = 0;
    s_pairing_status_requested = false;
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
        s_pairing_status_requested = false;
    }
    taskEXIT_CRITICAL(&s_pairing_lock);

    return active;
}

static bool pairing_status_active(void)
{
    bool active;

    taskENTER_CRITICAL(&s_pairing_lock);
    active = s_pairing_window_open && s_pairing_status_requested;
    taskEXIT_CRITICAL(&s_pairing_lock);

    return active;
}

static ble_state_snapshot_t copy_ble_state(void)
{
    ble_state_snapshot_t state;

    taskENTER_CRITICAL(&s_ble_state_lock);
    state.link_connected = s_link_connected;
    state.hid_connected = s_hid_connected;
    state.hid_started = s_hid_started;
    state.terminate_requested = s_terminate_requested;
    state.conn_handle = s_conn_handle;
    state.directed_request_seq = s_directed_request_seq;
    state.have_peer_addr = s_have_peer_addr;
    state.peer_addr = s_peer_addr;
    taskEXIT_CRITICAL(&s_ble_state_lock);

    return state;
}

static bool ble_state_connection_active(const ble_state_snapshot_t *state)
{
    return state->link_connected ||
           state->hid_connected ||
           state->conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

static void request_link_termination(void)
{
    taskENTER_CRITICAL(&s_ble_state_lock);
    s_terminate_requested = true;
    taskEXIT_CRITICAL(&s_ble_state_lock);
}

static void clear_link_termination_request(void)
{
    taskENTER_CRITICAL(&s_ble_state_lock);
    s_terminate_requested = false;
    taskEXIT_CRITICAL(&s_ble_state_lock);
}

static void reset_real_input_state(TickType_t idle_anchor)
{
    taskENTER_CRITICAL(&s_report_lock);
    s_previous_input_mask = 0;
    s_last_real_input_tick = idle_anchor;
    taskEXIT_CRITICAL(&s_report_lock);
}

static TickType_t last_real_input_tick(void)
{
    TickType_t tick;

    taskENTER_CRITICAL(&s_report_lock);
    tick = s_last_real_input_tick;
    taskEXIT_CRITICAL(&s_report_lock);

    return tick;
}

static uint32_t input_activity_mask(uint16_t buttons, uint8_t dpad, int8_t x, int8_t y)
{
    uint32_t mask = buttons | ((uint32_t)(dpad & 0x0F) << 16);

    if (x <= -UART_AXIS_DEADZONE) {
        mask |= 1UL << 20;
    }
    if (x >= UART_AXIS_DEADZONE) {
        mask |= 1UL << 21;
    }
    if (y <= -UART_AXIS_DEADZONE) {
        mask |= 1UL << 22;
    }
    if (y >= UART_AXIS_DEADZONE) {
        mask |= 1UL << 23;
    }

    return mask;
}

static bool peer_identity_addr_valid(const ble_addr_t *addr)
{
    static const uint8_t zero_addr[6] = {0};
    bool valid_type = addr->type == BLE_ADDR_PUBLIC ||
                      addr->type == BLE_ADDR_RANDOM ||
                      addr->type == BLE_ADDR_PUBLIC_ID ||
                      addr->type == BLE_ADDR_RANDOM_ID;

    return valid_type && memcmp(addr->val, zero_addr, sizeof(zero_addr)) != 0;
}

static void note_real_input(uint32_t input_mask)
{
    TickType_t now = xTaskGetTickCount();
    bool press_edge;

    taskENTER_CRITICAL(&s_report_lock);
    press_edge = (input_mask & ~s_previous_input_mask) != 0;
    s_previous_input_mask = input_mask;
    if (input_mask != 0) {
        /* Repeated active frames keep a long press out of slow advertising. */
        s_last_real_input_tick = now;
    }
    taskEXIT_CRITICAL(&s_report_lock);

    if (!press_edge) {
        return;
    }

    taskENTER_CRITICAL(&s_ble_state_lock);
    if (!s_link_connected) {
        ++s_directed_request_seq;
    }
    taskEXIT_CRITICAL(&s_ble_state_lock);
}

static void update_ble_status_output(void)
{
    ble_state_snapshot_t state = copy_ble_state();
    bool connection_active = ble_state_connection_active(&state);
    bool high_duty_directed = s_adv_mode == ADV_MODE_DIRECTED_HIGH &&
                              ble_gap_adv_active();
    uint8_t status;

    if (state.hid_connected) {
        status = BLE_STATUS_CONNECTED;
    } else if (pairing_status_active()) {
        status = BLE_STATUS_PAIRING;
    } else if (connection_active || high_duty_directed) {
        status = BLE_STATUS_CONNECTING;
    } else {
        status = BLE_STATUS_DISCONNECTED;
    }

    send_ble_status_frame(status);
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

static const char *gameplay_gate_name(gameplay_gate_t gate)
{
    switch (gate) {
    case GAMEPLAY_GATE_UNLOCKED:
        return "UNLOCKED";
    case GAMEPLAY_GATE_LOCKED:
        return "LOCKED";
    case GAMEPLAY_GATE_DRAIN:
        return "DRAIN";
    default:
        return "?";
    }
}

static void log_gameplay_gate_transition(gameplay_gate_t previous,
                                         gameplay_gate_t current)
{
    if (previous != current) {
        ESP_LOGI(TAG, "gameplay gate: %s -> %s",
                 gameplay_gate_name(previous), gameplay_gate_name(current));
    }
}

/* Force the desired HID state to neutral and enter DRAIN atomically with the
 * press-edge reset.  Valid blocked FP frames still refresh the existing UART
 * report timestamp; FT and local state changes do not. */
static bool force_gameplay_drain(bool valid_fp_received)
{
    gameplay_gate_t previous;
    gameplay_gate_t current;
    bool report_changed;

    taskENTER_CRITICAL(&s_report_lock);
    previous = s_gameplay_gate;
    gameplay_gate_force_drain(&s_gameplay_gate);
    current = s_gameplay_gate;
    report_changed = memcmp(s_current_report, neutral_report,
                            HID_GAMEPAD_REPORT_LEN) != 0;
    memcpy(s_current_report, neutral_report, HID_GAMEPAD_REPORT_LEN);
    s_previous_input_mask = 0;
    if (valid_fp_received) {
        s_last_uart_tick = xTaskGetTickCount();
        s_have_uart_report = true;
    }
    taskEXIT_CRITICAL(&s_report_lock);

    log_gameplay_gate_transition(previous, current);
    return report_changed;
}

typedef struct {
    gameplay_gate_action_t action;
    bool report_changed;
} gameplay_gate_result_t;

static gameplay_gate_result_t process_valid_fp_gate(bool gameplay_locked,
                                                    bool payload_neutral)
{
    gameplay_gate_result_t result = {
        .action = GAMEPLAY_GATE_SUPPRESS,
        .report_changed = false,
    };
    gameplay_gate_t previous;
    gameplay_gate_t current;

    taskENTER_CRITICAL(&s_report_lock);
    previous = s_gameplay_gate;
    result.action = gameplay_gate_accept_fp(&s_gameplay_gate,
                                             gameplay_locked,
                                             payload_neutral);
    current = s_gameplay_gate;

    /* Every valid FP, including locked/draining reports, keeps the existing
     * UART-report watchdog alive.  This happens before releasing the lock so
     * the stale task cannot race a newly accepted UNLOCKED report. */
    s_last_uart_tick = xTaskGetTickCount();
    s_have_uart_report = true;

    if (result.action == GAMEPLAY_GATE_SUPPRESS) {
        result.report_changed = memcmp(s_current_report, neutral_report,
                                       HID_GAMEPAD_REPORT_LEN) != 0;
        memcpy(s_current_report, neutral_report, HID_GAMEPAD_REPORT_LEN);
        s_previous_input_mask = 0;
    }
    taskEXIT_CRITICAL(&s_report_lock);

    log_gameplay_gate_transition(previous, current);
    return result;
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

static void copy_transport_state(bool *seen, bool *enabled)
{
    taskENTER_CRITICAL(&s_transport_lock);
    if (seen != NULL) {
        *seen = s_transport_mode_seen;
    }
    if (enabled != NULL) {
        *enabled = s_transport_bt_enabled;
    }
    taskEXIT_CRITICAL(&s_transport_lock);
}

static bool neutralize_stale_uart_report(void)
{
    bool timed_out = false;
    TickType_t now = xTaskGetTickCount();
    gameplay_gate_t previous = GAMEPLAY_GATE_DRAIN;
    gameplay_gate_t current = GAMEPLAY_GATE_DRAIN;

    taskENTER_CRITICAL(&s_report_lock);
    if (s_have_uart_report &&
        (now - s_last_uart_tick) > pdMS_TO_TICKS(UART_INPUT_STALE_MS)) {
        timed_out = true;
        previous = s_gameplay_gate;
        gameplay_gate_force_drain(&s_gameplay_gate);
        current = s_gameplay_gate;
        memcpy(s_current_report, neutral_report, HID_GAMEPAD_REPORT_LEN);
        s_have_uart_report = false;
        s_previous_input_mask = 0;
    }
    taskEXIT_CRITICAL(&s_report_lock);

    if (timed_out) {
        log_gameplay_gate_transition(previous, current);
    }
    return timed_out;
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
        if (changed) {
            force_gameplay_drain(false);
            reset_real_input_state(xTaskGetTickCount());
        }
        return;
    }

    force_gameplay_drain(false);
    close_pairing_window();

    request_link_termination();
}

static void trigger_pairing_mode(void)
{
    bool mode_seen;
    bool bt_enabled;
    copy_transport_state(&mode_seen, &bt_enabled);

    if (mode_seen && !bt_enabled) {
        ESP_LOGI(TAG, "pair button ignored: transport mode disables bluetooth");
        close_pairing_window();
        return;
    }

    if (!mode_seen) {
        ESP_LOGI(TAG, "pair button: transport mode not seen yet, defaulting to bluetooth");
        set_transport_bt_enabled(true);
    }

    ESP_LOGI(TAG, "pair button pressed: opening/restarting %d ms pairing window",
             PAIRING_WINDOW_MS);
    open_pairing_window(true);

    /* GAP disconnect remains the single authority for the link handle. */
    request_link_termination();

    /* Do NOT call ble_store_clear() here. The phone may still have the
     * old bond and will try to reconnect with it. If we wipe our side
     * first, the encryption handshake fails outright (CONNECT status≠0)
     * instead of triggering BLE_GAP_EVENT_REPEAT_PAIRING, which is what
     * actually allows a clean re-pairing. Let NimBLE handle bond cleanup
     * automatically via the REPEAT_PAIRING path. */
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
            if (stable_pressed) {
                trigger_pairing_mode();
            }
        }

        /* Expire the general boot window, but only an explicit GPIO13 pairing
         * request suppresses directed advertising. */
        (void)pairing_window_active();
        manage_advertising(now, pairing_status_active());
        update_ble_status_output();

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

static void send_ble_status_frame(uint8_t status)
{
    if (status == s_ble_status) {
        return;  /* no change — skip duplicate */
    }

    uint8_t frame[UART_INPUT_FRAME_LEN];
    memset(frame, 0, UART_INPUT_FRAME_LEN);
    frame[0] = UART_INPUT_MAGIC0;
    frame[1] = UART_MAGIC_BLE_STATUS;
    frame[2] = status;
    frame[7] = uart_frame_checksum(frame);

    uart_write_bytes(UART_INPUT_PORT, (const char *)frame, UART_INPUT_FRAME_LEN);
    s_ble_status = status;

    static const char *names[] = {
        [BLE_STATUS_DISCONNECTED] = "disconnected",
        [BLE_STATUS_CONNECTING]   = "connecting",
        [BLE_STATUS_CONNECTED]    = "connected",
        [BLE_STATUS_PAIRING]      = "pairing",
    };
    ESP_LOGI(TAG, "BLE status -> %s (0x%02x)",
             (status < sizeof(names)/sizeof(names[0])) ? names[status] : "?",
             status);
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
        force_gameplay_drain(true);
        return;
    }

    uint16_t buttons = (frame[2] | ((uint16_t)frame[3] << 8)) &
                       GAMEPLAY_FP_BUTTON_MASK;
    bool gameplay_locked = (frame[4] & GAMEPLAY_FP_LOCK_BIT) != 0;
    uint8_t dpad = frame[4] & GAMEPLAY_FP_DPAD_MASK;
    int8_t x = (int8_t)frame[5];
    int8_t y = (int8_t)frame[6];
    bool payload_neutral = buttons == 0 && dpad == 0 && x == 0 && y == 0;
    uint8_t report[HID_GAMEPAD_REPORT_LEN];
    gameplay_gate_result_t gate_result =
        process_valid_fp_gate(gameplay_locked, payload_neutral);

    if (!s_uart_seen_frame) {
        ESP_LOGI(TAG, "UART input frame received");
        s_uart_seen_frame = true;
    }

    if (gate_result.action != GAMEPLAY_GATE_FORWARD) {
        if (gate_result.report_changed) {
            ESP_LOGD(TAG, "UART gameplay payload suppressed; neutral HID restored");
        }
        return;
    }

    compose_hid_report(buttons, dpad, x, y, report);
    bool changed = set_current_report(report);

    /* Neutral heartbeat frames still release HID state, but only real input
     * refreshes the idle timer. New asserted controls trigger one directed
     * advertising request; repeated held frames do not restart the burst. */
    note_real_input(input_activity_mask(buttons, dpad, x, y));

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
    TickType_t report_delay_ticks = pdMS_TO_TICKS(HID_REPORT_INTERVAL_MS);

    /* Never allow a sub-tick configuration to turn this priority-5 task into
     * a busy loop that starves the CPU0 idle task and trips the task WDT. */
    if (report_delay_ticks == 0) {
        report_delay_ticks = 1;
    }

    while (true) {
        if (neutralize_stale_uart_report()) {
            ESP_LOGW(TAG, "UART input timeout; neutral report restored");
        }

        copy_current_report(report);

        ble_state_snapshot_t state = copy_ble_state();
        bool report_link_ready = s_hid_dev != NULL &&
                                 state.link_connected &&
                                 state.hid_connected &&
                                 state.conn_handle != BLE_HS_CONN_HANDLE_NONE;

        if (report_link_ready) {
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

        vTaskDelay(report_delay_ticks);
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
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_conn_handle = event->connect.conn_handle;
            s_link_connected = true;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            close_pairing_window();

            /* Use the stable identity address, never a rotating OTA RPA. */
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0 &&
                desc.sec_state.bonded &&
                peer_identity_addr_valid(&desc.peer_id_addr)) {
                taskENTER_CRITICAL(&s_ble_state_lock);
                s_peer_addr = desc.peer_id_addr;
                s_have_peer_addr = true;
                taskEXIT_CRITICAL(&s_ble_state_lock);
            }

            if (!transport_bt_enabled()) {
                request_link_termination();
            }
        } else {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_link_connected = false;
            s_hid_connected = false;
            taskEXIT_CRITICAL(&s_ble_state_lock);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnect reason=%d", event->disconnect.reason);
        if (event->disconnect.conn.sec_state.bonded &&
            peer_identity_addr_valid(&event->disconnect.conn.peer_id_addr)) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_peer_addr = event->disconnect.conn.peer_id_addr;
            s_have_peer_addr = true;
            taskEXIT_CRITICAL(&s_ble_state_lock);
        }
        taskENTER_CRITICAL(&s_ble_state_lock);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_link_connected = false;
        s_hid_connected = false;
        s_terminate_requested = false;
        if (s_have_peer_addr) {
            /* A peripheral cannot observe when the PC radio comes back on.
             * Queue one directed burst now, then let the idle state fall back
             * to low-duty directed advertising for the bonded peer. */
            ++s_directed_request_seq;
        }
        taskEXIT_CRITICAL(&s_ble_state_lock);
        reset_real_input_state(xTaskGetTickCount());
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "BLE connection updated status=%d", event->conn_update.status);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        /* pair_button_task observes the stopped procedure and selects the
         * next mode. Manual ble_gap_adv_stop() does not emit this event. */
        ESP_LOGI(TAG, "BLE advertising complete reason=%d", event->adv_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "BLE encryption changed status=%d", event->enc_change.status);
        if (event->enc_change.status == 0 &&
            ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0 &&
            desc.sec_state.bonded &&
            peer_identity_addr_valid(&desc.peer_id_addr)) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_peer_addr = desc.peer_id_addr;
            s_have_peer_addr = true;
            taskEXIT_CRITICAL(&s_ble_state_lock);
        }
        return 0;

    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        if (ble_gap_conn_find(event->identity_resolved.conn_handle, &desc) == 0 &&
            desc.sec_state.bonded &&
            peer_identity_addr_valid(&desc.peer_id_addr)) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_peer_addr = desc.peer_id_addr;
            s_have_peer_addr = true;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            ESP_LOGI(TAG, "BLE peer identity resolved");
        }
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "BLE subscribe conn=%d attr=%d notify=%d indicate=%d",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify,
                 event->subscribe.cur_indicate);
        ensure_report_task_running();
        taskENTER_CRITICAL(&s_ble_state_lock);
        if (s_link_connected) {
            s_hid_connected = true;
        }
        taskEXIT_CRITICAL(&s_ble_state_lock);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc != 0) {
            ESP_LOGW(TAG, "repeat pairing peer lookup failed rc=%d", rc);
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }

        rc = ble_store_util_delete_peer(&desc.peer_id_addr);
        if (rc != 0) {
            ESP_LOGW(TAG, "repeat pairing bond delete failed rc=%d", rc);
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }

        taskENTER_CRITICAL(&s_ble_state_lock);
        if (s_have_peer_addr &&
            s_peer_addr.type == desc.peer_id_addr.type &&
            memcmp(s_peer_addr.val, desc.peer_id_addr.val,
                   sizeof(s_peer_addr.val)) == 0) {
            memset(&s_peer_addr, 0, sizeof(s_peer_addr));
            s_have_peer_addr = false;
        }
        taskEXIT_CRITICAL(&s_ble_state_lock);
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

static int start_directed_advertising(const ble_addr_t *peer_addr, bool high_duty)
{
#if PAIRING_DIAGNOSTIC_DISABLE_ADV
    ESP_LOGI(TAG, "diagnostic: advertising disabled in firmware");
    return BLE_HS_ENOTSUP;
#endif

    if (ble_gap_adv_active()) {
        return BLE_HS_EALREADY;
    }

    if (!s_addr_ready && !ensure_ble_address()) {
        return BLE_HS_ENOADDR;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_DIR;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.high_duty_cycle = high_duty;
    if (!high_duty) {
        adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(ADV_IDLE_ITVL_MS);
        adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(ADV_IDLE_ITVL_MS);
    }

    int rc = ble_gap_adv_start(s_own_addr_type, peer_addr,
                               high_duty ? DIRECTED_ADV_TIMEOUT_MS : BLE_HS_FOREVER,
                               &adv_params, gap_event, NULL);
    if (rc != 0) {
        return rc;
    }

    ESP_LOGI(TAG, "%s directed advertising to peer "
              "%02x:%02x:%02x:%02x:%02x:%02x",
              high_duty ? "high-duty" : "low-duty",
              peer_addr->val[5], peer_addr->val[4], peer_addr->val[3],
              peer_addr->val[2], peer_addr->val[1], peer_addr->val[0]);

    return 0;
}

static int start_advertising(bool fast)
{
#if PAIRING_DIAGNOSTIC_DISABLE_ADV
    ESP_LOGI(TAG, "diagnostic: advertising disabled in firmware");
    return BLE_HS_ENOTSUP;
#endif

    if (ble_gap_adv_active()) {
        return BLE_HS_EALREADY;
    }

    if (!s_addr_ready && !ensure_ble_address()) {
        return BLE_HS_ENOADDR;
    }

    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = ESP_HID_APPEARANCE_GAMEPAD;
    fields.appearance_is_present = 1;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.uuids16 = &s_hid_service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        return rc;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if (fast) {
        adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(ADV_ACTIVE_ITVL_MIN_MS);
        adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(ADV_ACTIVE_ITVL_MAX_MS);
    } else {
        adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(ADV_IDLE_ITVL_MS);
        adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(ADV_IDLE_ITVL_MS);
    }

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    if (rc != 0) {
        return rc;
    }

    ESP_LOGI(TAG, "advertising as %s (%s interval)", DEVICE_NAME,
             fast ? "fast" : "slow-200ms");

    return 0;
}

static void manage_advertising(TickType_t now, bool pairing_active)
{
    ble_state_snapshot_t state = copy_ble_state();
    bool connection_active = ble_state_connection_active(&state);
    bool bt_enabled;
    copy_transport_state(NULL, &bt_enabled);
    bool adv_active = ble_gap_adv_active();

    if (state.terminate_requested) {
        if (state.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            clear_link_termination_request();
        } else {
            int rc = ble_gap_terminate(state.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            if (rc == 0 || rc == BLE_HS_EALREADY || rc == BLE_HS_ENOTCONN) {
                clear_link_termination_request();
            } else {
                ESP_LOGW(TAG, "BLE terminate request failed rc=%d", rc);
            }
        }
        return;
    }

    if (!adv_active && s_adv_mode != ADV_MODE_OFF) {
        if (s_adv_mode == ADV_MODE_DIRECTED_HIGH) {
            ESP_LOGI(TAG, "directed advertising burst finished");
        }
        s_adv_mode = ADV_MODE_OFF;
    }

    if (!bt_enabled || !state.hid_started || connection_active) {
        if (connection_active || !bt_enabled) {
            s_directed_handled_seq = state.directed_request_seq;
        }

        if (adv_active) {
            int rc = ble_gap_adv_stop();
            if (rc == 0 || rc == BLE_HS_EALREADY) {
                s_adv_mode = ADV_MODE_OFF;
            } else {
                ESP_LOGW(TAG, "advertising stop failed rc=%d", rc);
            }
        } else {
            s_adv_mode = ADV_MODE_OFF;
        }
        return;
    }

    bool directed_pending = state.directed_request_seq != s_directed_handled_seq;
    if (directed_pending && (pairing_active || !state.have_peer_addr)) {
        if (!state.have_peer_addr) {
            ESP_LOGI(TAG, "no bonded peer for directed advertising; using fast advertising");
        }
        s_directed_handled_seq = state.directed_request_seq;
        directed_pending = false;
    }

    if ((s_adv_mode == ADV_MODE_DIRECTED_HIGH ||
         s_adv_mode == ADV_MODE_DIRECTED_LOW) && adv_active &&
        !directed_pending && !pairing_active) {
        TickType_t last_input = last_real_input_tick();
        bool recent_input =
            (now - last_input) < pdMS_TO_TICKS(REAL_INPUT_IDLE_MS);
        if ((s_adv_mode == ADV_MODE_DIRECTED_HIGH) || !recent_input) {
            return;
        }
    }

    TickType_t last_input = last_real_input_tick();
    bool fast = pairing_active ||
                (now - last_input) < pdMS_TO_TICKS(REAL_INPUT_IDLE_MS);
    adv_mode_t desired_mode;
    if (directed_pending) {
        desired_mode = ADV_MODE_DIRECTED_HIGH;
    } else if (!fast && state.have_peer_addr) {
        desired_mode = ADV_MODE_DIRECTED_LOW;
    } else {
        desired_mode = fast ? ADV_MODE_FAST : ADV_MODE_SLOW;
    }

    if (adv_active) {
        if (s_adv_mode == desired_mode && !directed_pending) {
            return;
        }

        int rc = ble_gap_adv_stop();
        if (rc == 0 || rc == BLE_HS_EALREADY) {
            s_adv_mode = ADV_MODE_OFF;
            /* Start the new procedure on the next control-task iteration. */
        } else {
            ESP_LOGW(TAG, "advertising mode switch stop failed rc=%d", rc);
            s_adv_retry_after_tick = now + pdMS_TO_TICKS(ADV_RETRY_MS);
        }
        return;
    }

    if (s_adv_retry_after_tick != 0 &&
        (int32_t)(now - s_adv_retry_after_tick) < 0) {
        return;
    }

    int rc;
    if (desired_mode == ADV_MODE_DIRECTED_HIGH ||
        desired_mode == ADV_MODE_DIRECTED_LOW) {
        bool high_duty = desired_mode == ADV_MODE_DIRECTED_HIGH;
        rc = start_directed_advertising(&state.peer_addr, high_duty);
        if (rc == 0) {
            s_adv_mode = desired_mode;
            if (high_duty) {
                s_directed_handled_seq = state.directed_request_seq;
            }
            s_adv_retry_after_tick = 0;
            return;
        }
        ESP_LOGW(TAG, "%s directed advertising start failed rc=%d",
                 high_duty ? "high-duty" : "low-duty", rc);
        if (high_duty) {
            /* Do not strand the controller in an endless retry loop on a stale
             * peer or permanent parameter error. The next real press can retry. */
            s_directed_handled_seq = state.directed_request_seq;
        }
    } else {
        rc = start_advertising(desired_mode == ADV_MODE_FAST);
        if (rc == 0) {
            s_adv_mode = desired_mode;
            s_adv_retry_after_tick = 0;
            return;
        }
        ESP_LOGW(TAG, "undirected advertising start failed rc=%d", rc);
    }

    uint32_t retry_ms = (rc == BLE_HS_ENOMEM || rc == BLE_HS_EBUSY)
                            ? ADV_RESOURCE_RETRY_MS
                            : ADV_RETRY_MS;
    s_adv_retry_after_tick = now + pdMS_TO_TICKS(retry_ms);
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
        taskENTER_CRITICAL(&s_ble_state_lock);
        s_hid_started = true;
        taskEXIT_CRITICAL(&s_ble_state_lock);
        update_hid_battery_level(s_battery_level);
        break;

    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID connected status=%s", esp_err_to_name(param->connect.status));
        if (param->connect.status == ESP_OK) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            /* Reinforce the GAP callback's link state. The ESP-IDF NimBLE HID
             * component maintains a separate connected flag, so both sources
             * must converge before reports or advertising decisions run. */
            s_link_connected = true;
            s_hid_connected = true;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            close_pairing_window();
            update_hid_battery_level(s_battery_level);
            ensure_report_task_running();
        } else {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_hid_connected = false;
            taskEXIT_CRITICAL(&s_ble_state_lock);
        }
        break;

    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID disconnected reason=%d", param->disconnect.reason);
        taskENTER_CRITICAL(&s_ble_state_lock);
        s_hid_connected = false;
        s_link_connected = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        taskEXIT_CRITICAL(&s_ble_state_lock);
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
        taskENTER_CRITICAL(&s_ble_state_lock);
        s_hid_started = false;
        s_hid_connected = false;
        s_link_connected = false;
        s_terminate_requested = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        taskEXIT_CRITICAL(&s_ble_state_lock);
        break;

    default:
        break;
    }
}

typedef struct {
    bool found;
    uint16_t bond_count;
    ble_addr_t peer_addr;
} latest_bonded_peer_t;

static int select_latest_bonded_peer(int obj_type, union ble_store_value *value, void *cookie)
{
    latest_bonded_peer_t *latest = (latest_bonded_peer_t *)cookie;

    if (obj_type != BLE_STORE_OBJ_TYPE_OUR_SEC) {
        return 0;
    }

    if (!latest->found || value->sec.bond_count >= latest->bond_count) {
        latest->found = true;
        latest->bond_count = value->sec.bond_count;
        latest->peer_addr = value->sec.peer_addr;
    }

    return 0;
}

static void restore_latest_bonded_peer(void)
{
    latest_bonded_peer_t latest = {0};
    int rc = ble_store_iterate(BLE_STORE_OBJ_TYPE_OUR_SEC,
                               select_latest_bonded_peer,
                               &latest);
    if (rc != 0) {
        ESP_LOGW(TAG, "bonded peer restore failed rc=%d", rc);
        return;
    }

    if (!latest.found || !peer_identity_addr_valid(&latest.peer_addr)) {
        ESP_LOGI(TAG, "no persisted bonded peer available for directed advertising");
        return;
    }

    taskENTER_CRITICAL(&s_ble_state_lock);
    s_peer_addr = latest.peer_addr;
    s_have_peer_addr = true;
    taskEXIT_CRITICAL(&s_ble_state_lock);

    ESP_LOGI(TAG, "restored bonded peer %02x:%02x:%02x:%02x:%02x:%02x",
             latest.peer_addr.val[5], latest.peer_addr.val[4], latest.peer_addr.val[3],
             latest.peer_addr.val[2], latest.peer_addr.val[1], latest.peer_addr.val[0]);
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

    reset_real_input_state(xTaskGetTickCount());
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
    open_pairing_window(true);
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
    restore_latest_bonded_peer();
    ESP_LOGI(TAG, "app_main: ble store ready");
    ESP_ERROR_CHECK(esp_nimble_enable(host_task));
    ESP_LOGI(TAG, "app_main: nimble host enabled");

    BaseType_t task_result = xTaskCreate(pair_button_task,
                                         "pair_button_task",
                                         3072,
                                         NULL,
                                         5,
                                         &s_pair_button_task);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Boot: default transport to BT if RP2350 hasn't set it yet, then open
     * pairing window so BLE is discoverable + modem sleep has an RF schedule */
    bool mode_seen;
    copy_transport_state(&mode_seen, NULL);
    if (!mode_seen) {
        set_transport_bt_enabled(true);
    }
    open_pairing_window(false);

    ESP_LOGI(TAG, "app_main: pair button task started");
}
