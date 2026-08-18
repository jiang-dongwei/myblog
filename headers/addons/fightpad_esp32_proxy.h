#ifndef _FIGHTPAD_ESP32_PROXY_H_
#define _FIGHTPAD_ESP32_PROXY_H_

#include "BoardConfig.h"
#include "addons/fightpad_ble_profile.h"
#include "gpaddon.h"

#include "hardware/adc.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_ENABLED
#define FIGHTPAD12SLIM_ESP32_PROXY_ENABLED 0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART
#define FIGHTPAD12SLIM_ESP32_PROXY_UART uart0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART_BAUD
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_BAUD 115200
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART_BLOCK
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_BLOCK 0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_RESET_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_RESET_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BOOT_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_BOOT_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART_TX_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_TX_PIN 44
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART_RX_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_RX_PIN 45
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART_CTS_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_CTS_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_UART_RTS_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_UART_RTS_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_USE_FLOW_CONTROL
#define FIGHTPAD12SLIM_ESP32_PROXY_USE_FLOW_CONTROL 0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_AUTO_DTR_RTS
#define FIGHTPAD12SLIM_ESP32_PROXY_AUTO_DTR_RTS 1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_FORCE_BOARD_DEFAULTS
#define FIGHTPAD12SLIM_ESP32_PROXY_FORCE_BOARD_DEFAULTS 0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_RESET_PULSE_MS
#define FIGHTPAD12SLIM_ESP32_PROXY_RESET_PULSE_MS 50
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE
#define FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE 2048
#endif

#ifndef FIGHTPAD12SLIM_ESP32_FW_INFO_PAYLOAD_SIZE
#define FIGHTPAD12SLIM_ESP32_FW_INFO_PAYLOAD_SIZE 256
#endif

#ifndef FIGHTPAD12SLIM_ESP32_FW_INFO_TIMEOUT_MS
#define FIGHTPAD12SLIM_ESP32_FW_INFO_TIMEOUT_MS 200
#endif

#ifndef FIGHTPAD12SLIM_ESP32_BT_STATUS_RESULT_MS
#define FIGHTPAD12SLIM_ESP32_BT_STATUS_RESULT_MS 1000
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROFILE_RETRY_MS
#define FIGHTPAD12SLIM_ESP32_PROFILE_RETRY_MS 250
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROFILE_TIMEOUT_MS
#define FIGHTPAD12SLIM_ESP32_PROFILE_TIMEOUT_MS 2000
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROFILE_OVERLAY_MS
#define FIGHTPAD12SLIM_ESP32_PROFILE_OVERLAY_MS 2500
#endif

#ifndef FIGHTPAD12SLIM_ESP32_FW_INFO_SDK_SIZE
#define FIGHTPAD12SLIM_ESP32_FW_INFO_SDK_SIZE 24
#endif

#ifndef FIGHTPAD12SLIM_ESP32_FW_INFO_PLATFORM_SIZE
#define FIGHTPAD12SLIM_ESP32_FW_INFO_PLATFORM_SIZE 24
#endif

#ifndef FIGHTPAD12SLIM_ESP32_FW_INFO_BOARD_SIZE
#define FIGHTPAD12SLIM_ESP32_FW_INFO_BOARD_SIZE 64
#endif

#ifndef FIGHTPAD12SLIM_ESP32_FW_INFO_CPU_SIZE
#define FIGHTPAD12SLIM_ESP32_FW_INFO_CPU_SIZE 24
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_ENABLED
#define FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_ENABLED 0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_INTERVAL_MS
#define FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_INTERVAL_MS 10
#endif
#ifndef FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_MODE_INTERVAL_MS
#define FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_MODE_INTERVAL_MS 250
#endif

#ifndef FIGHTPAD12SLIM_TRANSPORT_SEL_PIN
#define FIGHTPAD12SLIM_TRANSPORT_SEL_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_TRANSPORT_BT_LEVEL
#define FIGHTPAD12SLIM_TRANSPORT_BT_LEVEL 0
#endif

#ifndef FIGHTPAD12SLIM_TRANSPORT_DEBOUNCE_MS
#define FIGHTPAD12SLIM_TRANSPORT_DEBOUNCE_MS 30
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_ACTIVE_LEVEL
#define FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_ACTIVE_LEVEL 1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_FOLLOWS_TRANSPORT
#define FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_FOLLOWS_TRANSPORT 0
#endif

#ifndef FIGHTPAD12SLIM_VBAT_SENSE_PIN
#define FIGHTPAD12SLIM_VBAT_SENSE_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_VBAT_ADC_CHANNEL
#define FIGHTPAD12SLIM_VBAT_ADC_CHANNEL -1
#endif

#ifndef FIGHTPAD12SLIM_VBUS_DET_PIN
#define FIGHTPAD12SLIM_VBUS_DET_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_ENABLED
#define FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_ENABLED 1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_INTERVAL_MS
#define FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_INTERVAL_MS 1000
#endif

#define FightpadESP32ProxyName "FightpadESP32Proxy"

struct FightpadESP32FirmwareInfo {
    bool valid = false;
    char sdk[FIGHTPAD12SLIM_ESP32_FW_INFO_SDK_SIZE] = {};
    char platform[FIGHTPAD12SLIM_ESP32_FW_INFO_PLATFORM_SIZE] = {};
    char board[FIGHTPAD12SLIM_ESP32_FW_INFO_BOARD_SIZE] = {};
    char cpu[FIGHTPAD12SLIM_ESP32_FW_INFO_CPU_SIZE] = {};
};

// Core1 uses this function to obtain one complete copy of the information
// parsed and published by the Core0 UART addon.
bool getFightpadESP32FirmwareInfo(FightpadESP32FirmwareInfo& info);

enum class FightpadESP32BluetoothStatus : uint8_t {
    Disconnected = 0x00,
    Connecting = 0x01,
    Connected = 0x02,
    Pairing = 0x03,
};

struct FightpadESP32BluetoothStatusEvent {
    bool valid = false;
    FightpadESP32BluetoothStatus status = FightpadESP32BluetoothStatus::Disconnected;
    uint32_t receivedAtMs = 0;
    uint32_t sequence = 0;
};

// Core0 publishes each valid status frame.  Core0 lighting and Core1 display
// both use the receive timestamp so their transient indication ends together.
bool getFightpadESP32BluetoothStatusEvent(FightpadESP32BluetoothStatusEvent& event);
bool isFightpadESP32BluetoothStatusEventActive(
    const FightpadESP32BluetoothStatusEvent& event,
    uint32_t now);

enum class FightpadESP32BluetoothProfileStatus : uint8_t {
    Ready = 0,
    Applying = 1,
    PairAgain = 2,
    ProtocolError = 3,
    Timeout = 4,
    SaveFailed = 5,
};

struct FightpadESP32BluetoothProfileEvent {
    bool valid = false;
    FightpadESP32BluetoothProfileStatus status = FightpadESP32BluetoothProfileStatus::Ready;
    FightpadBluetoothProfile profile = FightpadBluetoothProfile::Generic;
    uint32_t receivedAtMs = 0;
    uint32_t sequence = 0;
};

bool getFightpadESP32BluetoothProfileEvent(FightpadESP32BluetoothProfileEvent& event);
bool isFightpadESP32BluetoothProfileEventActive(
    const FightpadESP32BluetoothProfileEvent& event,
    uint32_t now);
void publishFightpadESP32BluetoothProfileSaveFailed(FightpadBluetoothProfile profile);

// Returns a profile only while the debounced transport selector is in BT mode
// and ESP32-C6 has acknowledged that profile. Core1 display code uses this
// instead of showing an unconfirmed menu selection.
bool getFightpadESP32ActiveBluetoothProfile(FightpadBluetoothProfile& profile);

class FightpadESP32ProxyAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess() {}
    virtual void process();
    virtual void postprocess(bool sent);
    virtual void reinit();
    virtual std::string name() { return FightpadESP32ProxyName; }

    void setUsbLineState(bool dtr, bool rts);
    void setUsbLineCoding(uint32_t baudrate);

private:
    struct RingBuffer {
        uint8_t data[FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE] = {};
        uint16_t head = 0;
        uint16_t tail = 0;
        uint16_t count = 0;

        uint16_t available() const { return count; }
        uint16_t free() const { return FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE - count; }
        bool empty() const { return count == 0; }
        bool full() const { return count >= FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE; }
        bool push(uint8_t value);
        bool pop(uint8_t& value);
    };

    void loadOptions();
    void resetESP32(bool downloadMode);
    void applyLineState();
    void drainCdcToBuffer();
    void drainBufferToUart();
    void drainUartToBuffer();
    void drainBufferToCdc();
    void checkIncomingFrameTimeout();
    void feedIncomingFrameByte(uint8_t value);
    void resyncIncomingFrame();
    void handleIncomingFrame(const uint8_t frame[8]);
    void handleFirmwareInfoFrame(const uint8_t frame[8]);
    void handleBluetoothStatusFrame(const uint8_t frame[8]);
    void handleBluetoothProfileAckFrame(const uint8_t frame[8]);
    void resetFirmwareInfoSequence();
    bool appendFirmwareInfoPayload(const uint8_t payload[4]);
    bool parseFirmwareInfoPayload();
    void refreshTurboPinMask();
    bool isBluetoothTransportSelected();
    void updateESP32EnableFromTransport(bool force = false);
    void sendTransportModeFrame(bool bluetoothSelected, bool force = false);
    void updateBluetoothProfileSync(bool force = false);
    void beginBluetoothProfileRequest(FightpadBluetoothProfile profile);
    void sendBluetoothProfileModeFrame(bool force = false);
    FightpadBluetoothProfile getConfiguredBluetoothProfile() const;
    void sendBatteryStatusFrame(bool force = false);
    void sendInputReportFrame();
    void sendNeutralInputReportFrame();
    void writeUartFrame(const uint8_t frame[8]);
    uint16_t mapInputReportButtons(const GamepadState& state) const;
    int8_t mapInputReportAxis(uint16_t value) const;
    bool batteryMonitoringSupported() const;
    bool readVbusPresent() const;
    uint16_t sampleBatteryAdcRaw() const;

    RingBuffer cdcToUart;
    RingBuffer uartToCdc;
    uart_inst_t *uart = FIGHTPAD12SLIM_ESP32_PROXY_UART;
    uint32_t baudrate = FIGHTPAD12SLIM_ESP32_PROXY_UART_BAUD;
    int resetPin = FIGHTPAD12SLIM_ESP32_PROXY_RESET_PIN;
    int bootPin = FIGHTPAD12SLIM_ESP32_PROXY_BOOT_PIN;
    int txPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_TX_PIN;
    int rxPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_RX_PIN;
    int ctsPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_CTS_PIN;
    int rtsPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_RTS_PIN;
    bool useFlowControl = FIGHTPAD12SLIM_ESP32_PROXY_USE_FLOW_CONTROL != 0;
    bool autoDtrRts = FIGHTPAD12SLIM_ESP32_PROXY_AUTO_DTR_RTS != 0;
    bool lastDtr = false;
    bool lastRts = false;
    bool initialized = false;
    uint8_t incomingFrame[8] = {};
    uint8_t incomingFrameLength = 0;
    uint8_t firmwareInfoPayload[FIGHTPAD12SLIM_ESP32_FW_INFO_PAYLOAD_SIZE] = {};
    uint16_t firmwareInfoPayloadLength = 0;
    uint8_t firmwareInfoExpectedSeq = 0;
    bool firmwareInfoSequenceActive = false;
    uint32_t incomingFrameLastByteTimeMs = 0;
    uint32_t firmwareInfoLastFrameTimeMs = 0;
    uint32_t turboPinMask = 0;
    uint32_t lastInputReportTimeMs = 0;
    uint8_t lastInputReport[8] = {};
    bool lastInputReportValid = false;
    bool inputReportActive = false;
    uint32_t lastTransportModeTimeMs = 0;
    bool lastTransportMode = false;
    bool lastTransportModeValid = false;
    bool transportDebounceCandidate = false;
    bool transportDebounceStable = false;
    bool transportDebounceValid = false;
    uint32_t transportDebounceCandidateSinceMs = 0;
    bool profileTransportValid = false;
    bool profileTransportBluetooth = false;
    bool profileRequestActive = false;
    bool profileRetryBlocked = false;
    bool profileSynchronizedValid = false;
    FightpadBluetoothProfile profileRequested = FightpadBluetoothProfile::Generic;
    FightpadBluetoothProfile profileRetryBlockedValue = FightpadBluetoothProfile::Generic;
    FightpadBluetoothProfile profileSynchronized = FightpadBluetoothProfile::Generic;
    uint8_t profileRequestSequence = 0;
    uint32_t profileRequestStartedMs = 0;
    uint32_t profileLastSendMs = 0;
    bool lastESP32Enabled = false;
    bool lastESP32EnabledValid = false;
    uint32_t lastBatteryStatusTimeMs = 0;
    uint8_t lastBatteryPercent = 0;
    bool lastBatteryPercentValid = false;
    bool lastBatteryVbusPresent = false;
};

#endif
