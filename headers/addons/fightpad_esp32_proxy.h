#ifndef _FIGHTPAD_ESP32_PROXY_H_
#define _FIGHTPAD_ESP32_PROXY_H_

#include "BoardConfig.h"
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

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_EMPTY_MV
#define FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_EMPTY_MV 3300
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_FULL_MV
#define FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_FULL_MV 4200
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_FILTER_SHIFT
#define FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_FILTER_SHIFT 3
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_PERCENT_HYSTERESIS
#define FIGHTPAD12SLIM_ESP32_PROXY_BATTERY_PERCENT_HYSTERESIS 2
#endif

#define FightpadESP32ProxyName "FightpadESP32Proxy"

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
    void refreshTurboPinMask();
    bool isBluetoothTransportSelected() const;
    void sendTransportModeFrame(bool bluetoothSelected, bool force = false);
    void sendBatteryStatusFrame(bool force = false);
    void sendInputReportFrame();
    void sendNeutralInputReportFrame();
    void writeUartFrame(const uint8_t frame[8]);
    uint16_t mapInputReportButtons(const GamepadState& state) const;
    int8_t mapInputReportAxis(uint16_t value) const;
    bool batteryMonitoringSupported() const;
    bool readVbusPresent() const;
    uint16_t sampleBatteryAdcRaw() const;
    uint16_t convertBatteryRawToMillivolts(uint16_t raw) const;
    uint8_t mapBatteryMillivoltsToPercent(uint16_t millivolts) const;

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
    uint32_t turboPinMask = 0;
    uint32_t lastInputReportTimeMs = 0;
    uint8_t lastInputReport[8] = {};
    bool lastInputReportValid = false;
    bool inputReportActive = false;
    uint32_t lastTransportModeTimeMs = 0;
    bool lastTransportMode = false;
    bool lastTransportModeValid = false;
    uint32_t lastBatteryStatusTimeMs = 0;
    uint8_t lastBatteryPercent = 0;
    bool lastBatteryPercentValid = false;
    bool lastBatteryVbusPresent = false;
    uint16_t lastBatteryMillivolts = 0;
    bool lastBatteryMillivoltsValid = false;
};

#endif
