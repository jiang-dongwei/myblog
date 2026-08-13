#include "addons/fightpad_esp32_proxy.h"
#include "addons/fightpad_bq27220_battery.h"
#include "addons/scrollwheel_menu.h"

#include "helper.h"
#include "storagemanager.h"
#include "tusb.h"

#include "hardware/sync.h"
#include "pico/critical_section.h"

#include <cstring>

namespace {
constexpr uint8_t kFrameMagic0 = 0x46;
constexpr uint8_t kFrameMagicReport = 0x50;
constexpr uint8_t kFrameMagicTransport = 0x54;
constexpr uint8_t kFrameMagicBattery = 0x42;
constexpr uint8_t kFrameMagicFirmwareInfo = 0x49;
constexpr uint8_t kFrameMagicBluetoothStatus = 0x53;
constexpr uint8_t kFrameMagicBluetoothProfileMode = 0x4D;
constexpr uint8_t kFrameMagicBluetoothProfileAck = 0x41;
constexpr uint8_t kFrameLength = 8;
constexpr uint8_t kInputReportGameplayLockFlag = 0x80;
constexpr uint8_t kFirmwareInfoFlagMask = 0xC0;
constexpr uint8_t kFirmwareInfoSeqMask = 0x3F;
constexpr uint8_t kFirmwareInfoFlagSingle = 0x00;
constexpr uint8_t kFirmwareInfoFlagLast = 0x40;
constexpr uint8_t kFirmwareInfoFlagMiddle = 0x80;
constexpr uint8_t kFirmwareInfoFlagFirst = 0xC0;
constexpr uint16_t kBatterySampleCount = 8;

FightpadESP32FirmwareInfo firmwareInfoSnapshot = {};
critical_section_t firmwareInfoCriticalSection = {};
volatile uint32_t firmwareInfoSnapshotVersion = 0;
FightpadESP32BluetoothStatusEvent bluetoothStatusSnapshot = {};
critical_section_t bluetoothStatusCriticalSection = {};
FightpadESP32BluetoothProfileEvent bluetoothProfileSnapshot = {};
critical_section_t bluetoothProfileCriticalSection = {};
bool bluetoothTransportSnapshotValid = false;
bool bluetoothTransportSnapshot = false;
bool bluetoothActiveProfileSnapshotValid = false;
FightpadBluetoothProfile bluetoothActiveProfileSnapshot = FightpadBluetoothProfile::Generic;

bool firmwareInfoKeyMatches(const uint8_t *key, size_t keyLength, const char *expected)
{
    size_t expectedLength = std::strlen(expected);
    return keyLength == expectedLength && std::memcmp(key, expected, expectedLength) == 0;
}

bool copyFirmwareInfoValue(char *destination, size_t destinationSize, const uint8_t *value, size_t valueLength)
{
    if (valueLength == 0 || valueLength >= destinationSize) {
        return false;
    }

    for (size_t i = 0; i < valueLength; i++) {
        if (value[i] == 0 || value[i] == '\r' || value[i] == '\n') {
            return false;
        }
    }

    std::memcpy(destination, value, valueLength);
    destination[valueLength] = '\0';
    return true;
}

void publishFirmwareInfo(const FightpadESP32FirmwareInfo& info)
{
    critical_section_enter_blocking(&firmwareInfoCriticalSection);
    firmwareInfoSnapshotVersion++;
    __dmb();
    firmwareInfoSnapshot = info;
    __dmb();
    firmwareInfoSnapshotVersion++;
    critical_section_exit(&firmwareInfoCriticalSection);
}

void publishBluetoothStatus(FightpadESP32BluetoothStatus status)
{
    critical_section_enter_blocking(&bluetoothStatusCriticalSection);
    bluetoothStatusSnapshot.valid = true;
    bluetoothStatusSnapshot.status = status;
    bluetoothStatusSnapshot.receivedAtMs = getMillis();
    bluetoothStatusSnapshot.sequence++;
    critical_section_exit(&bluetoothStatusCriticalSection);
}

void publishBluetoothProfileStatus(
    FightpadESP32BluetoothProfileStatus status,
    FightpadBluetoothProfile profile)
{
    critical_section_enter_blocking(&bluetoothProfileCriticalSection);
    bluetoothProfileSnapshot.valid = true;
    bluetoothProfileSnapshot.status = status;
    bluetoothProfileSnapshot.profile = profile;
    bluetoothProfileSnapshot.receivedAtMs = getMillis();
    bluetoothProfileSnapshot.sequence++;
    critical_section_exit(&bluetoothProfileCriticalSection);
}

void publishBluetoothTransport(bool bluetoothSelected)
{
    if (!critical_section_is_initialized(&bluetoothProfileCriticalSection)) {
        return;
    }

    critical_section_enter_blocking(&bluetoothProfileCriticalSection);
    bluetoothTransportSnapshot = bluetoothSelected;
    bluetoothTransportSnapshotValid = true;
    critical_section_exit(&bluetoothProfileCriticalSection);
}

void publishActiveBluetoothProfile(FightpadBluetoothProfile profile)
{
    critical_section_enter_blocking(&bluetoothProfileCriticalSection);
    bluetoothActiveProfileSnapshot = profile;
    bluetoothActiveProfileSnapshotValid = true;
    critical_section_exit(&bluetoothProfileCriticalSection);
}

void invalidateActiveBluetoothProfile()
{
    if (!critical_section_is_initialized(&bluetoothProfileCriticalSection)) {
        return;
    }

    critical_section_enter_blocking(&bluetoothProfileCriticalSection);
    bluetoothActiveProfileSnapshotValid = false;
    critical_section_exit(&bluetoothProfileCriticalSection);
}
}

static FightpadESP32ProxyAddon *activeProxy = nullptr;

bool getFightpadESP32FirmwareInfo(FightpadESP32FirmwareInfo& info)
{
    info = {};
    if (!critical_section_is_initialized(&firmwareInfoCriticalSection)) {
        return false;
    }

    critical_section_enter_blocking(&firmwareInfoCriticalSection);
    uint32_t versionBefore = firmwareInfoSnapshotVersion;
    __dmb();
    info = firmwareInfoSnapshot;
    __dmb();
    uint32_t versionAfter = firmwareInfoSnapshotVersion;
    critical_section_exit(&firmwareInfoCriticalSection);

    return versionBefore == versionAfter &&
           (versionAfter & 1u) == 0 &&
           info.valid;
}

bool getFightpadESP32BluetoothStatusEvent(FightpadESP32BluetoothStatusEvent& event)
{
    event = {};
    if (!critical_section_is_initialized(&bluetoothStatusCriticalSection)) {
        return false;
    }

    critical_section_enter_blocking(&bluetoothStatusCriticalSection);
    event = bluetoothStatusSnapshot;
    critical_section_exit(&bluetoothStatusCriticalSection);
    return event.valid;
}

bool isFightpadESP32BluetoothStatusEventActive(
    const FightpadESP32BluetoothStatusEvent& event,
    uint32_t now)
{
    if (!event.valid) {
        return false;
    }

    if (event.status == FightpadESP32BluetoothStatus::Connecting ||
        event.status == FightpadESP32BluetoothStatus::Pairing) {
        return true;
    }

    return (now - event.receivedAtMs) < FIGHTPAD12SLIM_ESP32_BT_STATUS_RESULT_MS;
}

bool getFightpadESP32BluetoothProfileEvent(FightpadESP32BluetoothProfileEvent& event)
{
    event = {};
    if (!critical_section_is_initialized(&bluetoothProfileCriticalSection)) {
        return false;
    }

    critical_section_enter_blocking(&bluetoothProfileCriticalSection);
    event = bluetoothProfileSnapshot;
    critical_section_exit(&bluetoothProfileCriticalSection);
    return event.valid;
}

bool isFightpadESP32BluetoothProfileEventActive(
    const FightpadESP32BluetoothProfileEvent& event,
    uint32_t now)
{
    return event.valid &&
           event.status != FightpadESP32BluetoothProfileStatus::Ready &&
           (now - event.receivedAtMs) < FIGHTPAD12SLIM_ESP32_PROFILE_OVERLAY_MS;
}

bool getFightpadESP32ActiveBluetoothProfile(FightpadBluetoothProfile& profile)
{
    if (!critical_section_is_initialized(&bluetoothProfileCriticalSection)) {
        return false;
    }

    critical_section_enter_blocking(&bluetoothProfileCriticalSection);
    const bool valid = bluetoothTransportSnapshotValid &&
                       bluetoothTransportSnapshot &&
                       bluetoothActiveProfileSnapshotValid;
    if (valid) {
        profile = bluetoothActiveProfileSnapshot;
    }
    critical_section_exit(&bluetoothProfileCriticalSection);
    return valid;
}

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN
#define FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN -1
#endif

static void setTransportDiagnosticPin(bool bluetoothSelected)
{
    if (!isValidPin(FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN)) {
        return;
    }

    gpio_put(
        FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN,
        bluetoothSelected ? 1 : 0);
}

bool FightpadESP32ProxyAddon::RingBuffer::push(uint8_t value)
{
    if (full()) {
        return false;
    }

    data[head] = value;
    head = (head + 1) % FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE;
    count++;
    return true;
}

bool FightpadESP32ProxyAddon::RingBuffer::pop(uint8_t& value)
{
    if (empty()) {
        return false;
    }

    value = data[tail];
    tail = (tail + 1) % FIGHTPAD12SLIM_ESP32_PROXY_BUFFER_SIZE;
    count--;
    return true;
}

bool FightpadESP32ProxyAddon::available()
{
#if !FIGHTPAD12SLIM_ESP32_PROXY_ENABLED
    return false;
#else
    loadOptions();

    return isValidPin(txPin) &&
           isValidPin(rxPin);
#endif
}

void FightpadESP32ProxyAddon::setup()
{
    loadOptions();
    refreshTurboPinMask();
    activeProxy = this;

    if (!critical_section_is_initialized(&firmwareInfoCriticalSection)) {
        critical_section_init(&firmwareInfoCriticalSection);
    }
    if (!critical_section_is_initialized(&bluetoothStatusCriticalSection)) {
        critical_section_init(&bluetoothStatusCriticalSection);
    }
    if (!critical_section_is_initialized(&bluetoothProfileCriticalSection)) {
        critical_section_init(&bluetoothProfileCriticalSection);
    }

    if (isValidPin(FIGHTPAD12SLIM_TRANSPORT_SEL_PIN)) {
        gpio_init(FIGHTPAD12SLIM_TRANSPORT_SEL_PIN);
        gpio_set_dir(FIGHTPAD12SLIM_TRANSPORT_SEL_PIN, GPIO_IN);
        gpio_pull_up(FIGHTPAD12SLIM_TRANSPORT_SEL_PIN);
    }

#if FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_FOLLOWS_TRANSPORT
    if (isValidPin(FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN)) {
        const bool esp32Enabled = isBluetoothTransportSelected();
        const bool outputLevel = esp32Enabled
            ? (FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_ACTIVE_LEVEL != 0)
            : (FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_ACTIVE_LEVEL == 0);
        gpio_init(FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN);
        gpio_put(FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN, outputLevel);
        gpio_set_dir(FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN, GPIO_OUT);
        lastESP32Enabled = esp32Enabled;
        lastESP32EnabledValid = true;
    }
#endif

    if (isValidPin(bootPin)) {
        gpio_init(bootPin);
        gpio_set_dir(bootPin, GPIO_IN);
        gpio_pull_up(bootPin);
    }

    if (isValidPin(resetPin)) {
        gpio_init(resetPin);
        gpio_set_dir(resetPin, GPIO_IN);
        gpio_pull_up(resetPin);
    }

    if (isValidPin(FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN)) {
        gpio_init(FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN);
        gpio_set_dir(FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_DIAGNOSTIC_PIN, GPIO_OUT);
    }

    if (isValidPin(FIGHTPAD12SLIM_VBUS_DET_PIN)) {
        gpio_init(FIGHTPAD12SLIM_VBUS_DET_PIN);
        gpio_set_dir(FIGHTPAD12SLIM_VBUS_DET_PIN, GPIO_IN);
        gpio_disable_pulls(FIGHTPAD12SLIM_VBUS_DET_PIN);
    }

    if (batteryMonitoringSupported()) {
        adc_gpio_init(FIGHTPAD12SLIM_VBAT_SENSE_PIN);
    }

    gpio_set_function(txPin, UART_FUNCSEL_NUM(uart, txPin));
    gpio_set_function(rxPin, UART_FUNCSEL_NUM(uart, rxPin));
    uart_init(uart, baudrate);
    uart_set_format(uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart, true);

    if (useFlowControl && isValidPin(ctsPin) && isValidPin(rtsPin)) {
        gpio_set_function(ctsPin, GPIO_FUNC_UART);
        gpio_set_function(rtsPin, GPIO_FUNC_UART);
        uart_set_hw_flow(uart, true, true);
    } else {
        uart_set_hw_flow(uart, false, false);
    }

    initialized = true;
    setTransportDiagnosticPin(isBluetoothTransportSelected());
    sendTransportModeFrame(isBluetoothTransportSelected(), true);
    updateBluetoothProfileSync(true);
    sendBatteryStatusFrame(true);
#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
    if (autoDtrRts && isValidPin(resetPin)) {
        resetESP32(false);
    }
#endif
}

void FightpadESP32ProxyAddon::process()
{
    if (!initialized) {
        return;
    }

    updateESP32EnableFromTransport();

    checkIncomingFrameTimeout();

#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
    drainCdcToBuffer();
    drainBufferToUart();
#endif

    drainUartToBuffer();

    updateBluetoothProfileSync();

#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
    drainBufferToCdc();
#endif
}

void FightpadESP32ProxyAddon::postprocess(bool sent)
{
    (void)sent;

    sendBatteryStatusFrame();

#if FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_ENABLED
    sendInputReportFrame();
#endif
}

void FightpadESP32ProxyAddon::reinit()
{
    refreshTurboPinMask();
    incomingFrameLength = 0;
    resetFirmwareInfoSequence();
    lastInputReportValid = false;
    lastTransportModeValid = false;
    profileTransportValid = false;
    profileRequestActive = false;
    profileRetryBlocked = false;
    profileSynchronizedValid = false;
    invalidateActiveBluetoothProfile();
    lastESP32EnabledValid = false;
    updateESP32EnableFromTransport(true);
    lastBatteryPercentValid = false;
}

void FightpadESP32ProxyAddon::setUsbLineState(bool dtr, bool rts)
{
    lastDtr = dtr;
    lastRts = rts;
    applyLineState();
}

void FightpadESP32ProxyAddon::setUsbLineCoding(uint32_t requestedBaudrate)
{
    if (requestedBaudrate == 0 || requestedBaudrate == baudrate) {
        return;
    }

    baudrate = requestedBaudrate;
    if (initialized) {
        uart_set_baudrate(uart, baudrate);
    }
}

void FightpadESP32ProxyAddon::loadOptions()
{
#if FIGHTPAD12SLIM_ESP32_PROXY_FORCE_BOARD_DEFAULTS
    uart = FIGHTPAD12SLIM_ESP32_PROXY_UART;
    baudrate = FIGHTPAD12SLIM_ESP32_PROXY_UART_BAUD;
    resetPin = FIGHTPAD12SLIM_ESP32_PROXY_RESET_PIN;
    bootPin = FIGHTPAD12SLIM_ESP32_PROXY_BOOT_PIN;
    txPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_TX_PIN;
    rxPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_RX_PIN;
    ctsPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_CTS_PIN;
    rtsPin = FIGHTPAD12SLIM_ESP32_PROXY_UART_RTS_PIN;
    useFlowControl = FIGHTPAD12SLIM_ESP32_PROXY_USE_FLOW_CONTROL != 0;
    autoDtrRts = FIGHTPAD12SLIM_ESP32_PROXY_AUTO_DTR_RTS != 0;
#else
    const FightpadESP32ProxyOptions& options = Storage::getInstance().getAddonOptions().fightpadESP32ProxyOptions;
    if (!options.enabled) {
        txPin = -1;
        rxPin = -1;
        return;
    }

    uart = options.uartBlock == 0 ? uart0 : uart1;
    baudrate = options.baud == 0 ? FIGHTPAD12SLIM_ESP32_PROXY_UART_BAUD : options.baud;
    resetPin = options.resetPin;
    bootPin = options.bootPin;
    txPin = options.txPin;
    rxPin = options.rxPin;
    ctsPin = options.ctsPin;
    rtsPin = options.rtsPin;
    useFlowControl = options.useFlowControl;
    autoDtrRts = options.autoDtrRts;
#endif
}

void FightpadESP32ProxyAddon::resetESP32(bool downloadMode)
{
    if (isValidPin(bootPin)) {
        gpio_put(bootPin, downloadMode ? 0 : 1);
    }

    if (!isValidPin(resetPin)) {
        return;
    }

    gpio_put(resetPin, 0);
    sleep_ms(FIGHTPAD12SLIM_ESP32_PROXY_RESET_PULSE_MS);
    gpio_put(resetPin, 1);
    sleep_ms(FIGHTPAD12SLIM_ESP32_PROXY_RESET_PULSE_MS);
}

void FightpadESP32ProxyAddon::applyLineState()
{
    if (!autoDtrRts || !initialized) {
        return;
    }

    if (isValidPin(bootPin)) {
        gpio_put(bootPin, lastDtr ? 0 : 1);
    }
    if (isValidPin(resetPin)) {
        gpio_put(resetPin, lastRts ? 0 : 1);
    }
}

void FightpadESP32ProxyAddon::drainCdcToBuffer()
{
#if CFG_TUD_CDC
    if (!tud_cdc_ready() || cdcToUart.full()) {
        return;
    }

    uint8_t buffer[64];
    uint32_t toRead = tud_cdc_available();
    while (toRead > 0 && cdcToUart.free() > 0) {
        uint32_t chunk = toRead;
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (chunk > cdcToUart.free()) {
            chunk = cdcToUart.free();
        }

        uint32_t read = tud_cdc_read(buffer, chunk);
        if (read == 0) {
            break;
        }

        for (uint32_t i = 0; i < read; i++) {
            cdcToUart.push(buffer[i]);
        }

        toRead = tud_cdc_available();
    }
#endif
}

void FightpadESP32ProxyAddon::drainBufferToUart()
{
    uint8_t value;
    while (!cdcToUart.empty() && uart_is_writable(uart)) {
        if (!cdcToUart.pop(value)) {
            break;
        }
        uart_putc_raw(uart, value);
    }
}

void FightpadESP32ProxyAddon::drainUartToBuffer()
{
    while (uart_is_readable(uart)) {
        uint8_t value = uart_getc(uart);
        feedIncomingFrameByte(value);

#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED && CFG_TUD_CDC
        if (!uartToCdc.full()) {
            uartToCdc.push(value);
        }
#endif
    }
}

void FightpadESP32ProxyAddon::drainBufferToCdc()
{
#if CFG_TUD_CDC
    if (!tud_cdc_ready() || uartToCdc.empty()) {
        return;
    }

    uint8_t buffer[64];
    while (!uartToCdc.empty() && tud_cdc_write_available() > 0) {
        uint32_t writable = tud_cdc_write_available();
        uint32_t chunk = writable;
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (chunk > uartToCdc.available()) {
            chunk = uartToCdc.available();
        }

        for (uint32_t i = 0; i < chunk; i++) {
            uartToCdc.pop(buffer[i]);
        }

        uint32_t written = tud_cdc_write(buffer, chunk);
        if (written < chunk) {
            for (uint32_t i = written; i < chunk; i++) {
                if (!uartToCdc.push(buffer[i])) {
                    break;
                }
            }
            break;
        }
    }

    tud_cdc_write_flush();
#endif
}

void FightpadESP32ProxyAddon::checkIncomingFrameTimeout()
{
    uint32_t now = getMillis();

    if (incomingFrameLength > 0 &&
        (now - incomingFrameLastByteTimeMs) >= FIGHTPAD12SLIM_ESP32_FW_INFO_TIMEOUT_MS) {
        incomingFrameLength = 0;
    }

    if (firmwareInfoSequenceActive &&
        (now - firmwareInfoLastFrameTimeMs) >= FIGHTPAD12SLIM_ESP32_FW_INFO_TIMEOUT_MS) {
        resetFirmwareInfoSequence();
    }
}

void FightpadESP32ProxyAddon::feedIncomingFrameByte(uint8_t value)
{
    uint32_t now = getMillis();

    if (incomingFrameLength == 0) {
        if (value == kFrameMagic0) {
            incomingFrame[0] = value;
            incomingFrameLength = 1;
            incomingFrameLastByteTimeMs = now;
        }
        return;
    }

    if (incomingFrameLength == 1) {
        if (value == kFrameMagicFirmwareInfo ||
            value == kFrameMagicBluetoothStatus ||
            value == kFrameMagicBluetoothProfileAck) {
            incomingFrame[1] = value;
            incomingFrameLength = 2;
            incomingFrameLastByteTimeMs = now;
        } else if (value == kFrameMagic0) {
            incomingFrame[0] = value;
            incomingFrameLastByteTimeMs = now;
        } else {
            incomingFrameLength = 0;
        }
        return;
    }

    incomingFrame[incomingFrameLength++] = value;
    incomingFrameLastByteTimeMs = now;
    if (incomingFrameLength < kFrameLength) {
        return;
    }

    uint8_t checksum = 0;
    for (uint8_t i = 0; i < kFrameLength - 1; i++) {
        checksum ^= incomingFrame[i];
    }

    if (checksum == incomingFrame[kFrameLength - 1]) {
        handleIncomingFrame(incomingFrame);
        incomingFrameLength = 0;
        return;
    }

    if (incomingFrame[1] == kFrameMagicFirmwareInfo) {
        resetFirmwareInfoSequence();
    }
    resyncIncomingFrame();
}

void FightpadESP32ProxyAddon::resyncIncomingFrame()
{
    uint8_t preservedStart = kFrameLength;
    uint8_t preservedLength = 0;

    for (uint8_t i = 1; i < kFrameLength; i++) {
        if (incomingFrame[i] != kFrameMagic0) {
            continue;
        }

        if (i == kFrameLength - 1) {
            preservedStart = i;
            preservedLength = 1;
            break;
        }

        if (incomingFrame[i + 1] == kFrameMagicFirmwareInfo ||
            incomingFrame[i + 1] == kFrameMagicBluetoothStatus ||
            incomingFrame[i + 1] == kFrameMagicBluetoothProfileAck) {
            preservedStart = i;
            preservedLength = kFrameLength - i;
            break;
        }
    }

    if (preservedLength > 0) {
        std::memmove(incomingFrame, incomingFrame + preservedStart, preservedLength);
        incomingFrameLength = preservedLength;
        incomingFrameLastByteTimeMs = getMillis();
    } else {
        incomingFrameLength = 0;
    }
}

void FightpadESP32ProxyAddon::handleIncomingFrame(const uint8_t frame[8])
{
    switch (frame[1]) {
    case kFrameMagicFirmwareInfo:
        handleFirmwareInfoFrame(frame);
        break;
    case kFrameMagicBluetoothStatus:
        handleBluetoothStatusFrame(frame);
        break;
    case kFrameMagicBluetoothProfileAck:
        handleBluetoothProfileAckFrame(frame);
        break;
    default:
        break;
    }
}

void FightpadESP32ProxyAddon::handleBluetoothStatusFrame(const uint8_t frame[8])
{
    if (frame[2] > static_cast<uint8_t>(FightpadESP32BluetoothStatus::Pairing)) {
        return;
    }

    publishBluetoothStatus(static_cast<FightpadESP32BluetoothStatus>(frame[2]));
}

void FightpadESP32ProxyAddon::handleBluetoothProfileAckFrame(const uint8_t frame[8])
{
    if (!profileRequestActive || frame[4] != profileRequestSequence) {
        return;
    }

    auto failRequest = [this]() {
        profileRequestActive = false;
        profileSynchronizedValid = false;
        profileRetryBlocked = true;
        profileRetryBlockedValue = profileRequested;
        publishBluetoothProfileStatus(
            FightpadESP32BluetoothProfileStatus::ProtocolError,
            profileRequested);
    };

    if (frame[2] != FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION ||
        !isValidFightpadBluetoothProfile(frame[3]) ||
        frame[5] > static_cast<uint8_t>(FightpadBluetoothProfileAckResult::InternalError)) {
        failRequest();
        return;
    }

    const FightpadBluetoothProfile acceptedProfile =
        static_cast<FightpadBluetoothProfile>(frame[3]);
    const FightpadBluetoothProfileAckResult result =
        static_cast<FightpadBluetoothProfileAckResult>(frame[5]);

    switch (result) {
    case FightpadBluetoothProfileAckResult::ActiveUnchanged:
        if (acceptedProfile != profileRequested) {
            failRequest();
            return;
        }
        profileRequestActive = false;
        profileRetryBlocked = false;
        profileSynchronized = acceptedProfile;
        profileSynchronizedValid = true;
        publishActiveBluetoothProfile(acceptedProfile);
        publishBluetoothProfileStatus(
            FightpadESP32BluetoothProfileStatus::Ready,
            acceptedProfile);
        break;

    case FightpadBluetoothProfileAckResult::Restarting:
    case FightpadBluetoothProfileAckResult::ApplyingAtBoot:
        if (acceptedProfile != profileRequested) {
            failRequest();
            return;
        }
        profileRequestActive = false;
        profileRetryBlocked = false;
        profileSynchronized = acceptedProfile;
        profileSynchronizedValid = true;
        publishActiveBluetoothProfile(acceptedProfile);
        publishBluetoothProfileStatus(
            FightpadESP32BluetoothProfileStatus::PairAgain,
            acceptedProfile);
        break;

    case FightpadBluetoothProfileAckResult::InvalidFallback:
        profileRequestActive = false;
        profileSynchronized = acceptedProfile;
        profileSynchronizedValid = true;
        publishActiveBluetoothProfile(acceptedProfile);
        profileRetryBlocked = true;
        profileRetryBlockedValue = profileRequested;
        publishBluetoothProfileStatus(
            FightpadESP32BluetoothProfileStatus::ProtocolError,
            acceptedProfile);
        break;

    case FightpadBluetoothProfileAckResult::UnsupportedVersion:
    case FightpadBluetoothProfileAckResult::InternalError:
    default:
        failRequest();
        break;
    }
}

void FightpadESP32ProxyAddon::handleFirmwareInfoFrame(const uint8_t frame[8])
{
    uint8_t flag = frame[2] & kFirmwareInfoFlagMask;
    uint8_t seq = frame[2] & kFirmwareInfoSeqMask;
    const uint8_t *payload = frame + 3;
    uint32_t now = getMillis();

    if (flag == kFirmwareInfoFlagFirst) {
        resetFirmwareInfoSequence();
        if (seq != 0) {
            return;
        }

        firmwareInfoSequenceActive = true;
        firmwareInfoExpectedSeq = 1;
        firmwareInfoLastFrameTimeMs = now;
        if (!appendFirmwareInfoPayload(payload)) {
            resetFirmwareInfoSequence();
        }
        return;
    }

    if (flag == kFirmwareInfoFlagSingle) {
        resetFirmwareInfoSequence();
        if (appendFirmwareInfoPayload(payload)) {
            parseFirmwareInfoPayload();
        }
        resetFirmwareInfoSequence();
        return;
    }

    if (!firmwareInfoSequenceActive || seq != firmwareInfoExpectedSeq) {
        resetFirmwareInfoSequence();
        return;
    }

    if (!appendFirmwareInfoPayload(payload)) {
        resetFirmwareInfoSequence();
        return;
    }

    firmwareInfoExpectedSeq = (seq + 1) & kFirmwareInfoSeqMask;
    firmwareInfoLastFrameTimeMs = now;

    if (flag == kFirmwareInfoFlagLast) {
        firmwareInfoSequenceActive = false;
        parseFirmwareInfoPayload();
        resetFirmwareInfoSequence();
    } else if (flag != kFirmwareInfoFlagMiddle) {
        resetFirmwareInfoSequence();
    }
}

void FightpadESP32ProxyAddon::resetFirmwareInfoSequence()
{
    firmwareInfoPayloadLength = 0;
    firmwareInfoExpectedSeq = 0;
    firmwareInfoSequenceActive = false;
    firmwareInfoLastFrameTimeMs = 0;
}

bool FightpadESP32ProxyAddon::appendFirmwareInfoPayload(const uint8_t payload[4])
{
    if (firmwareInfoPayloadLength + 4 > FIGHTPAD12SLIM_ESP32_FW_INFO_PAYLOAD_SIZE) {
        return false;
    }

    std::memcpy(firmwareInfoPayload + firmwareInfoPayloadLength, payload, 4);
    firmwareInfoPayloadLength += 4;
    return true;
}

bool FightpadESP32ProxyAddon::parseFirmwareInfoPayload()
{
    uint16_t payloadLength = firmwareInfoPayloadLength;
    while (payloadLength > 0 && firmwareInfoPayload[payloadLength - 1] == 0) {
        payloadLength--;
    }
    if (payloadLength == 0) {
        return false;
    }

    FightpadESP32FirmwareInfo parsed = {};
    bool hasSdk = false;
    bool hasPlatform = false;
    bool hasBoard = false;
    bool hasCpu = false;
    uint16_t lineStart = 0;

    while (lineStart < payloadLength) {
        uint16_t lineEnd = lineStart;
        while (lineEnd < payloadLength && firmwareInfoPayload[lineEnd] != '\n') {
            lineEnd++;
        }
        if (lineEnd >= payloadLength) {
            return false;
        }

        uint16_t equals = lineStart;
        while (equals < lineEnd && firmwareInfoPayload[equals] != '=') {
            equals++;
        }
        if (equals == lineStart || equals >= lineEnd) {
            return false;
        }

        const uint8_t *key = firmwareInfoPayload + lineStart;
        size_t keyLength = equals - lineStart;
        const uint8_t *value = firmwareInfoPayload + equals + 1;
        size_t valueLength = lineEnd - equals - 1;

        if (firmwareInfoKeyMatches(key, keyLength, "SDK")) {
            if (hasSdk || !copyFirmwareInfoValue(parsed.sdk, sizeof(parsed.sdk), value, valueLength)) {
                return false;
            }
            hasSdk = true;
        } else if (firmwareInfoKeyMatches(key, keyLength, "Plat")) {
            if (hasPlatform || !copyFirmwareInfoValue(parsed.platform, sizeof(parsed.platform), value, valueLength)) {
                return false;
            }
            hasPlatform = true;
        } else if (firmwareInfoKeyMatches(key, keyLength, "Board")) {
            if (hasBoard || !copyFirmwareInfoValue(parsed.board, sizeof(parsed.board), value, valueLength)) {
                return false;
            }
            hasBoard = true;
        } else if (firmwareInfoKeyMatches(key, keyLength, "CPU")) {
            if (hasCpu || !copyFirmwareInfoValue(parsed.cpu, sizeof(parsed.cpu), value, valueLength)) {
                return false;
            }
            hasCpu = true;
        }

        lineStart = lineEnd + 1;
    }

    if (!hasSdk || !hasPlatform || !hasBoard || !hasCpu) {
        return false;
    }

    parsed.valid = true;
    publishFirmwareInfo(parsed);
    return true;
}

void FightpadESP32ProxyAddon::refreshTurboPinMask()
{
    turboPinMask = 0;

    GpioMappingInfo* pinMappings = Storage::getInstance().getProfilePinMappings();
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
        if (pinMappings[pin].action == GpioAction::BUTTON_PRESS_TURBO) {
            turboPinMask |= 1 << pin;
        }
    }
}

bool FightpadESP32ProxyAddon::isBluetoothTransportSelected()
{
    if (!isValidPin(FIGHTPAD12SLIM_TRANSPORT_SEL_PIN)) {
        publishBluetoothTransport(true);
        return true;
    }

    const bool rawBluetoothSelected =
        gpio_get(FIGHTPAD12SLIM_TRANSPORT_SEL_PIN) == FIGHTPAD12SLIM_TRANSPORT_BT_LEVEL;
    const uint32_t now = getMillis();

    if (!transportDebounceValid) {
        transportDebounceCandidate = rawBluetoothSelected;
        transportDebounceStable = rawBluetoothSelected;
        transportDebounceCandidateSinceMs = now;
        transportDebounceValid = true;
        publishBluetoothTransport(transportDebounceStable);
        return transportDebounceStable;
    }

    if (rawBluetoothSelected != transportDebounceCandidate) {
        transportDebounceCandidate = rawBluetoothSelected;
        transportDebounceCandidateSinceMs = now;
    } else if (transportDebounceStable != transportDebounceCandidate &&
               (now - transportDebounceCandidateSinceMs) >= FIGHTPAD12SLIM_TRANSPORT_DEBOUNCE_MS) {
        transportDebounceStable = transportDebounceCandidate;
    }

    publishBluetoothTransport(transportDebounceStable);
    return transportDebounceStable;
}

void FightpadESP32ProxyAddon::updateESP32EnableFromTransport(bool force)
{
#if FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_FOLLOWS_TRANSPORT
    if (!isValidPin(FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN)) {
        return;
    }

    const bool esp32Enabled = isBluetoothTransportSelected();
    if (!force && lastESP32EnabledValid && lastESP32Enabled == esp32Enabled) {
        return;
    }

    const bool outputLevel = esp32Enabled
        ? (FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_ACTIVE_LEVEL != 0)
        : (FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_ACTIVE_LEVEL == 0);
    gpio_put(FIGHTPAD12SLIM_ESP32_PROXY_ENABLE_PIN, outputLevel);
    lastESP32Enabled = esp32Enabled;
    lastESP32EnabledValid = true;
#else
    (void)force;
#endif
}

void FightpadESP32ProxyAddon::sendTransportModeFrame(bool bluetoothSelected, bool force)
{
    if (!initialized) {
        return;
    }

    setTransportDiagnosticPin(bluetoothSelected);

    uint32_t now = getMillis();
    bool changed = !lastTransportModeValid || lastTransportMode != bluetoothSelected;
    bool due = force ||
               changed ||
               (now - lastTransportModeTimeMs) >= FIGHTPAD12SLIM_ESP32_PROXY_TRANSPORT_MODE_INTERVAL_MS;

    if (!due) {
        return;
    }

    uint8_t frame[8] = {
        kFrameMagic0,
        kFrameMagicTransport,
        (uint8_t)(bluetoothSelected ? 0x01 : 0x00),
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };

    for (uint8_t i = 0; i < sizeof(frame) - 1; i++) {
        frame[sizeof(frame) - 1] ^= frame[i];
    }

    writeUartFrame(frame);
    lastTransportMode = bluetoothSelected;
    lastTransportModeValid = true;
    lastTransportModeTimeMs = now;
}

FightpadBluetoothProfile FightpadESP32ProxyAddon::getConfiguredBluetoothProfile() const
{
    const FightpadESP32ProxyOptions& options =
        Storage::getInstance().getAddonOptions().fightpadESP32ProxyOptions;
    return normalizeFightpadBluetoothProfile(options.bluetoothProfile);
}

void FightpadESP32ProxyAddon::beginBluetoothProfileRequest(FightpadBluetoothProfile profile)
{
    profileRequestSequence++;
    profileRequested = profile;
    profileRequestActive = true;
    profileRetryBlocked = false;
    profileRequestStartedMs = getMillis();
    profileLastSendMs = profileRequestStartedMs;

    publishBluetoothProfileStatus(
        FightpadESP32BluetoothProfileStatus::Applying,
        profile);
    sendBluetoothProfileModeFrame(true);
}

void FightpadESP32ProxyAddon::sendBluetoothProfileModeFrame(bool force)
{
    if (!initialized || !profileRequestActive) {
        return;
    }

    const uint32_t now = getMillis();
    if (!force &&
        (now - profileLastSendMs) < FIGHTPAD12SLIM_ESP32_PROFILE_RETRY_MS) {
        return;
    }

    uint8_t frame[8] = {
        kFrameMagic0,
        kFrameMagicBluetoothProfileMode,
        FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION,
        static_cast<uint8_t>(profileRequested),
        profileRequestSequence,
        FIGHTPAD_BLE_PROFILE_FLAG_APPLY_NOW,
        0x00,
        0x00,
    };

    for (uint8_t i = 0; i < sizeof(frame) - 1; i++) {
        frame[sizeof(frame) - 1] ^= frame[i];
    }

    writeUartFrame(frame);
    profileLastSendMs = now;
}

void FightpadESP32ProxyAddon::updateBluetoothProfileSync(bool force)
{
    const bool bluetoothSelected = isBluetoothTransportSelected();
    const bool transportChanged =
        !profileTransportValid || profileTransportBluetooth != bluetoothSelected;

    if (transportChanged) {
        profileTransportValid = true;
        profileTransportBluetooth = bluetoothSelected;
        profileRequestActive = false;
        profileRetryBlocked = false;
        profileSynchronizedValid = false;

        if (bluetoothSelected) {
            /* Do not expose a profile from the previous BT session until the
             * newly powered C6 confirms its active profile with FA ACK. */
            invalidateActiveBluetoothProfile();
        } 

        if (!bluetoothSelected) {
            publishBluetoothProfileStatus(
                FightpadESP32BluetoothProfileStatus::Ready,
                getConfiguredBluetoothProfile());
            return;
        }
    }

    if (!bluetoothSelected) {
        return;
    }

    const FightpadBluetoothProfile configuredProfile = getConfiguredBluetoothProfile();

    if (profileRequestActive && configuredProfile != profileRequested) {
        beginBluetoothProfileRequest(configuredProfile);
        return;
    }

    if (profileRequestActive) {
        const uint32_t now = getMillis();
        if ((now - profileRequestStartedMs) >= FIGHTPAD12SLIM_ESP32_PROFILE_TIMEOUT_MS) {
            profileRequestActive = false;
            profileSynchronizedValid = false;
            profileRetryBlocked = true;
            profileRetryBlockedValue = profileRequested;
            publishBluetoothProfileStatus(
                FightpadESP32BluetoothProfileStatus::Timeout,
                profileRequested);
            return;
        }

        sendBluetoothProfileModeFrame(force);
        return;
    }

    if (profileRetryBlocked) {
        if (configuredProfile == profileRetryBlockedValue) {
            return;
        }
        profileRetryBlocked = false;
    }

    if (!profileSynchronizedValid || profileSynchronized != configuredProfile) {
        beginBluetoothProfileRequest(configuredProfile);
    }
}

bool FightpadESP32ProxyAddon::batteryMonitoringSupported() const
{
    return FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_ENABLED &&
           isValidPin(FIGHTPAD12SLIM_VBAT_SENSE_PIN) &&
           FIGHTPAD12SLIM_VBAT_ADC_CHANNEL >= 0;
}

bool FightpadESP32ProxyAddon::readVbusPresent() const
{
    if (!isValidPin(FIGHTPAD12SLIM_VBUS_DET_PIN)) {
        return false;
    }

    return gpio_get(FIGHTPAD12SLIM_VBUS_DET_PIN) != 0;
}

uint16_t FightpadESP32ProxyAddon::sampleBatteryAdcRaw() const
{
    if (!batteryMonitoringSupported()) {
        return 0;
    }

    uint32_t total = 0;
    adc_select_input(FIGHTPAD12SLIM_VBAT_ADC_CHANNEL);
    adc_read();

    for (uint16_t sample = 0; sample < kBatterySampleCount; sample++) {
        total += adc_read();
    }

    return (uint16_t)((total + (kBatterySampleCount / 2)) / kBatterySampleCount);
}

void FightpadESP32ProxyAddon::sendBatteryStatusFrame(bool force)
{
    if (!initialized || !batteryMonitoringSupported()) {
        return;
    }

    uint32_t now = getMillis();
    bool due = force ||
               !lastBatteryPercentValid ||
               (now - lastBatteryStatusTimeMs) >= FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_INTERVAL_MS;
    if (!due) {
        return;
    }

    uint8_t batteryPercent = 0;
    if (!FightpadBQ27220BatteryAddon::getBatteryPercentSnapshot(batteryPercent)) {
        return;
    }

    bool vbusPresent = readVbusPresent();
    uint16_t adcRaw = sampleBatteryAdcRaw();

    bool changed = !lastBatteryPercentValid ||
                   lastBatteryPercent != batteryPercent ||
                   lastBatteryVbusPresent != vbusPresent;

    if (!force && !changed &&
        (now - lastBatteryStatusTimeMs) < FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_INTERVAL_MS) {
        return;
    }

    uint8_t frame[8] = {
        kFrameMagic0,
        kFrameMagicBattery,
        batteryPercent,
        (uint8_t)(vbusPresent ? 0x01 : 0x00),
        (uint8_t)(adcRaw & 0xFF),
        (uint8_t)((adcRaw >> 8) & 0xFF),
        0x00,
        0x00,
    };

    for (uint8_t i = 0; i < sizeof(frame) - 1; i++) {
        frame[sizeof(frame) - 1] ^= frame[i];
    }

    writeUartFrame(frame);
    lastBatteryPercent = batteryPercent;
    lastBatteryVbusPresent = vbusPresent;
    lastBatteryPercentValid = true;
    lastBatteryStatusTimeMs = now;
}

uint16_t FightpadESP32ProxyAddon::mapInputReportButtons(const GamepadState& state) const
{
    uint16_t buttons = 0;

    if (state.buttons & GAMEPAD_MASK_B1) buttons |= 1 << 0;
    if (state.buttons & GAMEPAD_MASK_B2) buttons |= 1 << 1;
    if (state.buttons & GAMEPAD_MASK_B3) buttons |= 1 << 2;
    if (state.buttons & GAMEPAD_MASK_B4) buttons |= 1 << 3;
    if (state.buttons & GAMEPAD_MASK_L1) buttons |= 1 << 4;
    if (state.buttons & GAMEPAD_MASK_L2) buttons |= 1 << 5;
    if (state.buttons & GAMEPAD_MASK_R1) buttons |= 1 << 6;
    if (state.buttons & GAMEPAD_MASK_R2) buttons |= 1 << 7;
    if (state.buttons & GAMEPAD_MASK_L3) buttons |= 1 << 8;
    if (state.buttons & GAMEPAD_MASK_R3) buttons |= 1 << 9;
    if (state.buttons & GAMEPAD_MASK_S1) buttons |= 1 << 10;
    if (state.buttons & GAMEPAD_MASK_S2) buttons |= 1 << 11;
    if (state.buttons & GAMEPAD_MASK_A1) buttons |= 1 << 12;
    if (state.buttons & GAMEPAD_MASK_A2) buttons |= 1 << 13;

    Gamepad *rawGamepad = Storage::getInstance().GetGamepad();
    if (rawGamepad != nullptr && (rawGamepad->debouncedGpio & turboPinMask)) {
        buttons |= 1 << 14;
    }

    return buttons;
}

int8_t FightpadESP32ProxyAddon::mapInputReportAxis(uint16_t value) const
{
    int32_t scaled = (((int32_t)value * 254) + 32767) / 65535 - 127;

    if (scaled < -127) {
        scaled = -127;
    } else if (scaled > 127) {
        scaled = 127;
    }

    return (int8_t)scaled;
}

void FightpadESP32ProxyAddon::sendInputReportFrame()
{
    if (!initialized) {
        return;
    }

    bool bluetoothSelected = isBluetoothTransportSelected();
    sendTransportModeFrame(bluetoothSelected);

    if (!bluetoothSelected) {
        if (inputReportActive) {
            sendNeutralInputReportFrame();
            inputReportActive = false;
            lastInputReportValid = false;
        }
        return;
    }

    Gamepad *gamepad = Storage::getInstance().GetProcessedGamepad();
    if (gamepad == nullptr) {
        return;
    }

    uint16_t buttons = mapInputReportButtons(gamepad->state);
    uint8_t dpadAndFlags = (uint8_t)(gamepad->state.dpad & GAMEPAD_MASK_DPAD);
    if (isScrollWheelGameplayInputLocked()) {
        dpadAndFlags |= kInputReportGameplayLockFlag;
    }

    uint8_t frame[8] = {
        kFrameMagic0,
        kFrameMagicReport,
        (uint8_t)(buttons & 0xFF),
        (uint8_t)(buttons >> 8),
        dpadAndFlags,
        (uint8_t)mapInputReportAxis(gamepad->state.lx),
        (uint8_t)mapInputReportAxis(gamepad->state.ly),
        0x00,
    };

    for (uint8_t i = 0; i < sizeof(frame) - 1; i++) {
        frame[sizeof(frame) - 1] ^= frame[i];
    }

    inputReportActive = true;
    uint32_t now = getMillis();
    bool changed = !lastInputReportValid ||
                   std::memcmp(frame, lastInputReport, sizeof(frame)) != 0;
    bool due = !lastInputReportValid ||
               (now - lastInputReportTimeMs) >= FIGHTPAD12SLIM_ESP32_PROXY_INPUT_REPORT_INTERVAL_MS;

    if (!changed && !due) {
        return;
    }

    writeUartFrame(frame);
    std::memcpy(lastInputReport, frame, sizeof(frame));
    lastInputReportTimeMs = now;
    lastInputReportValid = true;
    inputReportActive = true;
}

void FightpadESP32ProxyAddon::sendNeutralInputReportFrame()
{
    uint8_t frame[8] = {
        kFrameMagic0,
        kFrameMagicReport,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };

    for (uint8_t i = 0; i < sizeof(frame) - 1; i++) {
        frame[sizeof(frame) - 1] ^= frame[i];
    }

    writeUartFrame(frame);
    std::memcpy(lastInputReport, frame, sizeof(frame));
    lastInputReportTimeMs = getMillis();
    lastInputReportValid = true;
}

void FightpadESP32ProxyAddon::writeUartFrame(const uint8_t frame[8])
{
    uart_write_blocking(uart, frame, 8);
}

#if CFG_TUD_CDC
extern "C" void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    if (itf == 0 && activeProxy != nullptr) {
        activeProxy->setUsbLineState(dtr, rts);
    }
}

extern "C" void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *lineCoding)
{
    if (itf == 0 && activeProxy != nullptr && lineCoding != nullptr) {
        activeProxy->setUsbLineCoding(lineCoding->bit_rate);
    }
}
#endif
