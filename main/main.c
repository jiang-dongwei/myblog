#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

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
#include "esp_system.h"
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
#include "ble_profile_state.h"
#include "ble_profile_store.h"
#include "ble_profiles.h"
#include "uart_protocol.h"

#if CONFIG_BT_NIMBLE_SVC_HID_MAX_RPTS < 2
#error "Xbox Series X|S 1914 requires at least two NimBLE HID Report characteristics"
#endif

#define HID_REPORT_INTERVAL_MS 10
#define HID_REPORT_KEEPALIVE_MS 50

#define UART_INPUT_PORT UART_NUM_0
#define UART_INPUT_BAUD 115200
#define UART_INPUT_TX_PIN 16
#define UART_INPUT_RX_PIN 17
#define UART_INPUT_RX_BUFFER_SIZE 512
#define UART_INPUT_FRAME_LEN FIGHTPAD_UART_FRAME_LEN
#define UART_INPUT_MAGIC0 FIGHTPAD_UART_MAGIC
#define UART_INPUT_MAGIC_REPORT FIGHTPAD_UART_TYPE_INPUT_REPORT
#define UART_INPUT_MAGIC_TRANSPORT FIGHTPAD_UART_TYPE_TRANSPORT
#define UART_INPUT_MAGIC_BATTERY FIGHTPAD_UART_TYPE_BATTERY
#define UART_INPUT_STALE_MS 250
#define UART_DONE_SIGNAL_TEXT "C6_DONE\n"
#define UART_DONE_SIGNAL_REPEAT 20
#define UART_DONE_SIGNAL_INTERVAL_MS 100
#define UART_TX_COMPLETE_TIMEOUT_MS 100
#define PROFILE_SYNC_WINDOW_MS 500
#define PROFILE_RESTART_DELAY_MS 50

#define UART_INPUT_MAGIC_FW_INFO FIGHTPAD_UART_TYPE_FW_INFO
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
#define PAIR_BUTTON_ACTIVE_LEVEL 0
#define PAIR_BUTTON_POLL_MS 10
#define PAIR_BUTTON_DEBOUNCE_MS 30
#define PAIRING_WINDOW_MS 30000
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
static const ble_profile_definition_t *s_profile_definition;
static fightpad_ble_profile_t s_active_profile = FIGHTPAD_BLE_PROFILE_GENERIC;
static ble_profile_persisted_state_t s_profile_state;
static bool s_profile_runtime;
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
static uint8_t s_ble_status = 0xFF;  /* deliberately invalid init value — forces first send */
static TickType_t s_last_real_input_tick;
static uint32_t s_previous_input_mask;
static bool s_link_connected;
static bool s_hid_connected;
static bool s_security_ready;
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
static portMUX_TYPE s_profile_lock = portMUX_INITIALIZER_UNLOCKED;

typedef enum {
    ADV_MODE_OFF = 0,
    ADV_MODE_FAST,
    ADV_MODE_BONDED_FAST,
    ADV_MODE_BONDED_SLOW,
    ADV_MODE_DIRECTED_HIGH,
} adv_mode_t;

typedef struct {
    bool link_connected;
    bool hid_connected;
    bool security_ready;
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

static uint8_t s_neutral_report[BLE_PROFILE_MAX_REPORT_LEN];
static uint8_t s_current_report[BLE_PROFILE_MAX_REPORT_LEN];
static size_t s_active_report_len;

/* ble_gap may reuse advertising fields during controller connection reattempts,
 * so referenced data must have static lifetime. */
static ble_uuid16_t s_hid_service_uuid = BLE_UUID16_INIT(0x1812);

typedef struct {
    uint8_t data[UART_INPUT_FRAME_LEN];
    uint8_t pos;
} uart_frame_parser_t;

static esp_hid_raw_report_map_t s_ble_report_map;
static esp_hid_device_config_t s_hid_config;

static int start_advertising(bool fast, const ble_addr_t *allowed_peer);
static int start_directed_advertising(const ble_addr_t *peer_addr, bool high_duty);
static void manage_advertising(TickType_t now, bool pairing_active);
static void set_transport_bt_enabled(bool enabled);
static void send_ble_status_frame(uint8_t status);
static void trigger_pairing_mode(void);
static void restore_latest_bonded_peer(void);

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
    state.security_ready = s_security_ready;
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

static ble_addr_t peer_controller_addr(const ble_addr_t *identity_addr)
{
    ble_addr_t controller_addr = *identity_addr;

    /* The bond store and connection descriptor can expose identity address
     * types (PUBLIC_ID / RANDOM_ID). Legacy advertising and controller white
     * list HCI commands require the corresponding over-the-air base type. */
    if (controller_addr.type == BLE_ADDR_PUBLIC_ID) {
        controller_addr.type = BLE_ADDR_PUBLIC;
    } else if (controller_addr.type == BLE_ADDR_RANDOM_ID) {
        controller_addr.type = BLE_ADDR_RANDOM;
    }

    return controller_addr;
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

    if (pairing_status_active()) {
        status = BLE_STATUS_PAIRING;
    } else if (state.hid_connected && state.security_ready) {
        status = BLE_STATUS_CONNECTED;
    } else if (connection_active || high_duty_directed) {
        status = BLE_STATUS_CONNECTING;
    } else {
        status = BLE_STATUS_DISCONNECTED;
    }

    send_ble_status_frame(status);
}

static esp_err_t configure_active_profile(fightpad_ble_profile_t profile)
{
    s_profile_definition = ble_profile_get_definition(profile);
    s_active_profile = s_profile_definition->profile;
    if (!ble_profile_neutral_report(s_active_profile,
                                    s_neutral_report,
                                    sizeof(s_neutral_report),
                                    &s_active_report_len)) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(s_current_report, s_neutral_report, s_active_report_len);
    s_ble_report_map.data = s_profile_definition->report_map;
    s_ble_report_map.len = s_profile_definition->report_map_len;
    s_hid_config = (esp_hid_device_config_t) {
        .vendor_id = s_profile_definition->vendor_id,
        .product_id = s_profile_definition->product_id,
        .version = s_profile_definition->version,
        .device_name = s_profile_definition->device_name,
        .manufacturer_name = s_profile_definition->manufacturer_name,
        .serial_number = s_profile_definition->serial_number,
        .report_maps = &s_ble_report_map,
        .report_maps_len = 1,
    };

    ESP_LOGI(TAG,
             "active BLE Profile=%u (%s), report id=%u len=%u map=%u max_rpts=%u",
             (unsigned)s_active_profile,
             s_profile_definition->profile_label,
             (unsigned)s_profile_definition->input_report_id,
             (unsigned)s_profile_definition->input_report_len,
             (unsigned)s_profile_definition->report_map_len,
             (unsigned)CONFIG_BT_NIMBLE_SVC_HID_MAX_RPTS);
    return ESP_OK;
}

static bool set_current_report(const uint8_t *report)
{
    bool changed;

    taskENTER_CRITICAL(&s_report_lock);
    changed = memcmp(s_current_report, report, s_active_report_len) != 0;
    memcpy(s_current_report, report, s_active_report_len);
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
    report_changed = memcmp(s_current_report, s_neutral_report,
                            s_active_report_len) != 0;
    memcpy(s_current_report, s_neutral_report, s_active_report_len);
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
        result.report_changed = memcmp(s_current_report, s_neutral_report,
                                       s_active_report_len) != 0;
        memcpy(s_current_report, s_neutral_report, s_active_report_len);
        s_previous_input_mask = 0;
    }
    taskEXIT_CRITICAL(&s_report_lock);

    log_gameplay_gate_transition(previous, current);
    return result;
}

static void copy_current_report(uint8_t *report)
{
    taskENTER_CRITICAL(&s_report_lock);
    memcpy(report, s_current_report, s_active_report_len);
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
        memcpy(s_current_report, s_neutral_report, s_active_report_len);
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
    /* Publish FS 03 before link teardown so an existing HID connection cannot
     * hide the pairing page on the RP2350 display. */
    send_ble_status_frame(BLE_STATUS_PAIRING);

    /* GAP disconnect remains the single authority for the link handle. */
    ble_state_snapshot_t state = copy_ble_state();
    if (ble_state_connection_active(&state)) {
        ESP_LOGI(TAG, "pairing flow: existing BLE link disconnect requested handle=%u",
                 state.conn_handle);
    } else {
        ESP_LOGI(TAG, "pairing flow: no active BLE link; fast advertising requested");
    }
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

    while (1) {
        bool raw_pressed =
            gpio_get_level(PAIR_BUTTON_GPIO) == PAIR_BUTTON_ACTIVE_LEVEL;
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

static esp_err_t send_hid_report(const uint8_t *report)
{
    return esp_hidd_dev_input_set(
        s_hid_dev,
        BLE_PROFILE_REPORT_MAP_INDEX,
        s_profile_definition->input_report_id,
        (uint8_t *)report,
        s_active_report_len
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

static bool write_uart_frame(const uint8_t frame[FIGHTPAD_UART_FRAME_LEN],
                             const char *label)
{
    int written = uart_write_bytes(UART_INPUT_PORT,
                                   (const char *)frame,
                                   FIGHTPAD_UART_FRAME_LEN);
    if (written != FIGHTPAD_UART_FRAME_LEN) {
        ESP_LOGE(TAG, "%s UART write short: %d/%u", label, written,
                 (unsigned)FIGHTPAD_UART_FRAME_LEN);
        return false;
    }
    esp_err_t err = uart_wait_tx_done(
        UART_INPUT_PORT, pdMS_TO_TICKS(UART_TX_COMPLETE_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s UART TX completion failed: %s",
                 label, esp_err_to_name(err));
        return false;
    }
    return true;
}

static void send_ble_status_frame(uint8_t status)
{
    if (status == s_ble_status) {
        return;  /* no change — skip duplicate */
    }

    uint8_t frame[FIGHTPAD_UART_FRAME_LEN];
    fightpad_uart_build_ble_status(status, frame);
    if (!write_uart_frame(frame, "BLE status")) {
        return;
    }
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

static bool send_profile_ack(fightpad_ble_profile_t profile,
                             uint8_t sequence,
                             fightpad_ble_profile_ack_t result)
{
    uint8_t frame[FIGHTPAD_UART_FRAME_LEN];
    fightpad_uart_build_profile_ack(profile, sequence, result, frame);
    bool sent = write_uart_frame(frame, "Profile ACK");
    ESP_LOGI(TAG, "Profile ACK seq=%u profile=%u result=%u sent=%u",
             sequence, (unsigned)profile, (unsigned)result, sent);
    return sent;
}

static void handle_profile_mode_frame(
    const uint8_t frame[FIGHTPAD_UART_FRAME_LEN])
{
    fightpad_ble_profile_mode_t mode = {0};
    fightpad_mode_parse_result_t parse_result =
        fightpad_uart_parse_profile_mode(frame, &mode);
    if (parse_result == FIGHTPAD_MODE_PARSE_BAD_FRAME) {
        return;
    }

    if ((mode.flags & ~BLE_PROFILE_FLAG_KNOWN_MASK) != 0u) {
        ESP_LOGW(TAG, "Profile Mode seq=%u has unknown flags 0x%02x",
                 mode.sequence,
                 mode.flags & (uint8_t)~BLE_PROFILE_FLAG_KNOWN_MASK);
    }
    if (mode.reserved != 0u) {
        ESP_LOGW(TAG, "Profile Mode seq=%u reserved=0x%02x ignored",
                 mode.sequence, mode.reserved);
    }

    ble_profile_persisted_state_t current;
    bool runtime;
    taskENTER_CRITICAL(&s_profile_lock);
    current = s_profile_state;
    runtime = s_profile_runtime;
    taskEXIT_CRITICAL(&s_profile_lock);

    fightpad_ble_profile_t decision_active = runtime
        ? s_active_profile
        : (fightpad_ble_profile_t)current.profile;
    ble_profile_mode_decision_t decision;
    ble_profile_decide_mode(&current,
                            decision_active,
                            runtime ? BLE_PROFILE_PHASE_RUNTIME
                                    : BLE_PROFILE_PHASE_BOOT,
                            parse_result,
                            &mode,
                            &decision);
    if (!decision.respond) {
        return;
    }

    if (decision.persist) {
        esp_err_t err = ble_profile_store_save(&decision.next_state);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Profile state save failed: %s",
                     esp_err_to_name(err));
            (void)send_profile_ack(decision_active,
                                   mode.sequence,
                                   BLE_PROFILE_ACK_INTERNAL_ERROR);
            return;
        }
        taskENTER_CRITICAL(&s_profile_lock);
        s_profile_state = decision.next_state;
        taskEXIT_CRITICAL(&s_profile_lock);
    }

    ESP_LOGI(TAG,
             "Profile Mode seq=%u requested=%u accepted=%u phase=%s restart=%u",
             mode.sequence,
             mode.requested_profile,
             (unsigned)decision.accepted_profile,
             runtime ? "runtime" : "boot",
             decision.restart);
    if (!send_profile_ack(decision.accepted_profile,
                          mode.sequence,
                          decision.ack_result)) {
        return;
    }

    if (decision.restart) {
        ESP_LOGI(TAG, "Profile change persisted; restarting C6 in %d ms",
                 PROFILE_RESTART_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(PROFILE_RESTART_DELAY_MS));
        esp_restart();
    }
}

static void handle_uart_frame(const uint8_t frame[UART_INPUT_FRAME_LEN])
{
    if (!fightpad_uart_frame_checksum_valid(frame)) {
        ESP_LOGD(TAG, "UART frame checksum mismatch type=0x%02x", frame[1]);
        return;
    }

    if (frame[1] == FIGHTPAD_UART_TYPE_PROFILE_MODE) {
        handle_profile_mode_frame(frame);
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
    uint8_t report[BLE_PROFILE_MAX_REPORT_LEN];
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

    normalized_fightpad_state_t input_state = {
        .buttons = buttons,
        .dpad = dpad,
        .x = x,
        .y = y,
    };
    size_t encoded_len = 0;
    if (!ble_profile_encode_report(s_active_profile,
                                   &input_state,
                                   report,
                                   sizeof(report),
                                   &encoded_len) ||
        encoded_len != s_active_report_len) {
        ESP_LOGE(TAG, "Profile report encode failed profile=%u",
                 (unsigned)s_active_profile);
        force_gameplay_drain(true);
        return;
    }
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
        if (fightpad_uart_input_type_allowed(value)) {
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
        frame[7] = fightpad_uart_checksum(frame);

        (void)write_uart_frame(frame, "firmware info");
    }
}

static void report_task(void *arg)
{
    uint8_t report[BLE_PROFILE_MAX_REPORT_LEN];
    uint8_t last_sent[BLE_PROFILE_MAX_REPORT_LEN] = {0};
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
                                 state.security_ready &&
                                 state.conn_handle != BLE_HS_CONN_HANDLE_NONE;

        if (report_link_ready) {
            TickType_t now = xTaskGetTickCount();
            bool changed = !have_last_sent ||
                           memcmp(report, last_sent, s_active_report_len) != 0;
            bool keepalive = have_last_sent &&
                             (now - last_sent_tick) >= pdMS_TO_TICKS(HID_REPORT_KEEPALIVE_MS);

            if (changed || keepalive) {
                esp_err_t err = send_hid_report(report);
                if (err == ESP_OK) {
                    memcpy(last_sent, report, s_active_report_len);
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
            s_security_ready = false;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            close_pairing_window();

            /* Use the stable identity address, never a rotating OTA RPA. */
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
                ESP_LOGI(TAG,
                         "BLE link security encrypted=%u authenticated=%u bonded=%u peer_type=%u id_type=%u",
                         desc.sec_state.encrypted,
                         desc.sec_state.authenticated,
                         desc.sec_state.bonded,
                         desc.peer_ota_addr.type,
                         desc.peer_id_addr.type);
                if (desc.sec_state.bonded &&
                    peer_identity_addr_valid(&desc.peer_id_addr)) {
                    taskENTER_CRITICAL(&s_ble_state_lock);
                    s_peer_addr = desc.peer_id_addr;
                    s_have_peer_addr = true;
                    taskEXIT_CRITICAL(&s_ble_state_lock);
                }
            }

            if (!transport_bt_enabled()) {
                request_link_termination();
            } else {
                /* macOS does not create a BLE HID input device until the
                 * link is encrypted.  Start SMP explicitly instead of
                 * depending on a later encrypted characteristic access. */
                rc = ble_gap_security_initiate(event->connect.conn_handle);
                if (rc == 0) {
                    ESP_LOGI(TAG, "BLE security procedure started");
                } else if (rc == BLE_HS_EALREADY) {
                    ESP_LOGI(TAG, "BLE security procedure already in progress");
                } else {
                    ESP_LOGE(TAG, "BLE security procedure start failed rc=%d", rc);
                    request_link_termination();
                }
            }
        } else {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_link_connected = false;
            s_hid_connected = false;
            s_security_ready = false;
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
        s_security_ready = false;
        s_terminate_requested = false;
        taskEXIT_CRITICAL(&s_ble_state_lock);

        /* Security state and bond persistence can complete after CONNECT.
         * Re-read the store at DISCONNECT so the very first bonded link is not
         * stranded merely because no earlier callback captured its identity. */
        if (!copy_ble_state().have_peer_addr) {
            restore_latest_bonded_peer();
        }

        bool reconnect_queued = false;
        uint32_t reconnect_seq = 0;
        uint8_t reconnect_peer_type = 0;
        taskENTER_CRITICAL(&s_ble_state_lock);
        if (s_have_peer_addr) {
            /* Queue a short directed attempt. manage_advertising() follows it
             * with whitelist-filtered undirected advertising, which Windows
             * reconnects to more reliably while preserving binding isolation. */
            ++s_directed_request_seq;
            reconnect_queued = true;
            reconnect_seq = s_directed_request_seq;
            reconnect_peer_type = s_peer_addr.type;
        }
        taskEXIT_CRITICAL(&s_ble_state_lock);
        if (reconnect_queued) {
            ESP_LOGI(TAG, "reconnect queued seq=%lu peer_type=%u",
                     (unsigned long)reconnect_seq,
                     reconnect_peer_type);
        } else {
            ESP_LOGW(TAG, "disconnect left no bonded peer; reconnect advertising remains off");
        }
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
        if (event->enc_change.status != 0) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_security_ready = false;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            ESP_LOGW(TAG, "BLE security failed; disconnecting insecure link");
            request_link_termination();
            return 0;
        }

        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        if (rc != 0) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_security_ready = false;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            ESP_LOGE(TAG, "BLE security state lookup failed rc=%d", rc);
            request_link_termination();
            return 0;
        }

        ESP_LOGI(TAG,
                 "BLE security state encrypted=%u authenticated=%u bonded=%u id_type=%u",
                 desc.sec_state.encrypted,
                 desc.sec_state.authenticated,
                 desc.sec_state.bonded,
                 desc.peer_id_addr.type);
        if (!desc.sec_state.encrypted || !desc.sec_state.bonded) {
            taskENTER_CRITICAL(&s_ble_state_lock);
            s_security_ready = false;
            taskEXIT_CRITICAL(&s_ble_state_lock);
            ESP_LOGW(TAG, "BLE security incomplete; disconnecting unbonded link");
            request_link_termination();
            return 0;
        }

        taskENTER_CRITICAL(&s_ble_state_lock);
        s_security_ready = true;
        if (peer_identity_addr_valid(&desc.peer_id_addr)) {
            s_peer_addr = desc.peer_id_addr;
            s_have_peer_addr = true;
        }
        taskEXIT_CRITICAL(&s_ble_state_lock);
        ESP_LOGI(TAG, "BLE secure bonded link ready");
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

    ble_addr_t controller_peer = peer_controller_addr(peer_addr);
    int rc = ble_gap_adv_start(s_own_addr_type, &controller_peer,
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

static int start_advertising(bool fast, const ble_addr_t *allowed_peer)
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
    fields.name = (uint8_t *)s_profile_definition->device_name;
    fields.name_len = strlen(s_profile_definition->device_name);
    fields.name_is_complete = 1;
    fields.uuids16 = &s_hid_service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG,
                 "advertising fields rejected rc=%d name=%s name_len=%u",
                 rc,
                 s_profile_definition->device_name,
                 (unsigned)fields.name_len);
        return rc;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if (allowed_peer != NULL) {
        ble_addr_t whitelist_peer = peer_controller_addr(allowed_peer);
        rc = ble_gap_wl_set(&whitelist_peer, 1);
        if (rc != 0) {
            ESP_LOGE(TAG, "bonded peer whitelist setup failed rc=%d type=%u",
                     rc, whitelist_peer.type);
            return rc;
        }
        adv_params.filter_policy = BLE_HCI_ADV_FILT_CONN;
    }
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

    ESP_LOGI(TAG, "advertising as %s (%s interval, %s connections)",
             s_profile_definition->device_name,
             fast ? "fast" : "slow-200ms",
             allowed_peer != NULL ? "bonded-peer-only" : "pairing-open");

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
                if (pairing_active) {
                    ESP_LOGI(TAG, "pairing flow: BLE disconnect command consumed rc=%d",
                             rc);
                }
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

    /* A normal boot without a bond is intentionally silent.  Only an
     * explicit GPIO13/Profile-change pairing window authorizes undirected
     * discoverable advertising for a new host. */
    if (!pairing_active && !state.have_peer_addr) {
        if (adv_active) {
            int rc = ble_gap_adv_stop();
            if (rc != 0 && rc != BLE_HS_EALREADY) {
                ESP_LOGW(TAG, "unauthorized advertising stop failed rc=%d", rc);
            }
        }
        s_adv_mode = ADV_MODE_OFF;
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

    if (s_adv_mode == ADV_MODE_DIRECTED_HIGH && adv_active &&
        !directed_pending && !pairing_active) {
        return;
    }

    TickType_t last_input = last_real_input_tick();
    bool fast = pairing_active ||
                (now - last_input) < pdMS_TO_TICKS(REAL_INPUT_IDLE_MS);
    adv_mode_t desired_mode;
    if (pairing_active) {
        desired_mode = ADV_MODE_FAST;
    } else if (directed_pending) {
        desired_mode = ADV_MODE_DIRECTED_HIGH;
    } else {
        desired_mode = fast ? ADV_MODE_BONDED_FAST : ADV_MODE_BONDED_SLOW;
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
    if (desired_mode == ADV_MODE_DIRECTED_HIGH) {
        rc = start_directed_advertising(&state.peer_addr, true);
        if (rc == 0) {
            s_adv_mode = desired_mode;
            s_directed_handled_seq = state.directed_request_seq;
            s_adv_retry_after_tick = 0;
            return;
        }
        ESP_LOGW(TAG, "high-duty directed advertising start failed rc=%d; falling back to bonded advertising",
                 rc);
        /* Do not strand the controller in an endless retry loop on a stale
         * peer or permanent parameter error. The next loop starts the normal
         * whitelist-filtered reconnect advertisement. */
        s_directed_handled_seq = state.directed_request_seq;
    } else {
        bool bonded_only = desired_mode == ADV_MODE_BONDED_FAST ||
                           desired_mode == ADV_MODE_BONDED_SLOW;
        bool adv_fast = desired_mode == ADV_MODE_FAST ||
                        desired_mode == ADV_MODE_BONDED_FAST;
        rc = start_advertising(adv_fast,
                               bonded_only ? &state.peer_addr : NULL);
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
        /* The HID event is asynchronous and does not carry a connection
         * handle or peer identity. GAP DISCONNECT remains the sole authority
         * for clearing the link and queuing reconnect advertising. */
        s_hid_connected = false;
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
        s_security_ready = false;
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

static esp_err_t load_profile_state(void)
{
    ble_profile_store_load_result_t result;
    ble_profile_persisted_state_t loaded;
    esp_err_t err = ble_profile_store_load(&loaded, &result);
    if (err != ESP_OK) {
        return err;
    }

    taskENTER_CRITICAL(&s_profile_lock);
    s_profile_state = loaded;
    taskEXIT_CRITICAL(&s_profile_lock);
    ESP_LOGI(TAG, "Profile state loaded result=%u profile=%u pending=0x%02x",
             (unsigned)result,
             loaded.profile,
             loaded.pending_flags);
    return ESP_OK;
}

static esp_err_t apply_profile_boot_pending(void)
{
    ble_profile_persisted_state_t current;
    taskENTER_CRITICAL(&s_profile_lock);
    current = s_profile_state;
    taskEXIT_CRITICAL(&s_profile_lock);

    if (current.pending_flags == 0u) {
        return ESP_OK;
    }

    if ((current.pending_flags & BLE_PROFILE_PENDING_CLEAR_BONDS) != 0u) {
        int rc = ble_store_clear();
        if (rc != 0) {
            ESP_LOGE(TAG, "Profile bond clear failed rc=%d; pending retained", rc);
            return ESP_FAIL;
        }
        taskENTER_CRITICAL(&s_ble_state_lock);
        memset(&s_peer_addr, 0, sizeof(s_peer_addr));
        s_have_peer_addr = false;
        taskEXIT_CRITICAL(&s_ble_state_lock);
        ESP_LOGI(TAG, "Profile change cleared old BLE bonds");
    }

    const bool open_pairing =
        (current.pending_flags & BLE_PROFILE_PENDING_PAIRING) != 0u;
    ble_profile_persisted_state_t completed = current;
    completed.pending_flags = 0u;
    esp_err_t err = ble_profile_store_save(&completed);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Profile pending completion save failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    taskENTER_CRITICAL(&s_profile_lock);
    s_profile_state = completed;
    taskEXIT_CRITICAL(&s_profile_lock);
    if (open_pairing) {
        ESP_LOGI(TAG, "Profile change opening %d ms pairing window",
                 PAIRING_WINDOW_MS);
        open_pairing_window(true);
    }
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
    ESP_ERROR_CHECK(load_profile_state());

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
    ESP_LOGI(TAG, "app_main: pair button ready on GPIO%d active_level=%d",
             PAIR_BUTTON_GPIO, PAIR_BUTTON_ACTIVE_LEVEL);

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

    ESP_LOGI(TAG, "waiting %d ms for boot Profile sync",
             PROFILE_SYNC_WINDOW_MS);
    vTaskDelay(pdMS_TO_TICKS(PROFILE_SYNC_WINDOW_MS));

    ble_profile_persisted_state_t boot_profile_state;
    taskENTER_CRITICAL(&s_profile_lock);
    boot_profile_state = s_profile_state;
    taskEXIT_CRITICAL(&s_profile_lock);
    ESP_ERROR_CHECK(configure_active_profile(
        (fightpad_ble_profile_t)boot_profile_state.profile));

    start_done_signal_task();
    ESP_LOGI(TAG, "app_main: done signal task started");

    ble_svc_gap_device_name_set(s_profile_definition->device_name);
    configure_security();
    ESP_LOGI(TAG, "app_main: ble config ready for %s",
             s_profile_definition->device_name);

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
        &s_hid_config,
        ESP_HID_TRANSPORT_BLE,
        hidd_event_callback,
        &s_hid_dev
    ));
    ESP_LOGI(TAG, "app_main: hid init requested");
    taskENTER_CRITICAL(&s_profile_lock);
    s_profile_runtime = true;
    taskEXIT_CRITICAL(&s_profile_lock);

    ble_store_config_init();
    ESP_ERROR_CHECK(apply_profile_boot_pending());
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

    /* Boot defaults to BT only when RP2350 has not sent Transport yet.  A
     * normal unbonded boot remains silent until GPIO13 or a Profile change
     * opens an explicit pairing window. */
    bool mode_seen;
    copy_transport_state(&mode_seen, NULL);
    if (!mode_seen) {
        set_transport_bt_enabled(true);
    }
    ESP_LOGI(TAG, "app_main: pair button task started");
}
