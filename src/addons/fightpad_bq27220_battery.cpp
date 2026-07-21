#include "addons/fightpad_bq27220_battery.h"

#include "hardware/uart.h"
#include "pico/stdlib.h"

#include <atomic>
#include <cmath>
#include <cstdio>

namespace
{
    static constexpr uint8_t BQ27220_I2C_ADDRESS = 0x55;
    static constexpr uint8_t BQ27220_COMMAND_CONTROL = 0x00;
    static constexpr uint8_t BQ27220_COMMAND_VOLTAGE = 0x08;
    static constexpr uint8_t BQ27220_COMMAND_BATTERY_STATUS = 0x0A;
    static constexpr uint8_t BQ27220_COMMAND_CURRENT = 0x0C;
    static constexpr uint8_t BQ27220_COMMAND_REMAINING_CAPACITY = 0x10;
    static constexpr uint8_t BQ27220_COMMAND_FULL_CHARGE_CAPACITY = 0x12;
    static constexpr uint8_t BQ27220_COMMAND_AVERAGE_CURRENT = 0x14;
    static constexpr uint8_t BQ27220_COMMAND_STATE_OF_CHARGE = 0x2C;
    static constexpr uint8_t BQ27220_COMMAND_DESIGN_CAPACITY = 0x3C;
    static constexpr uint8_t BQ27220_COMMAND_OPERATION_STATUS = 0x3A;
    static constexpr uint8_t BQ27220_COMMAND_DATA_MEMORY_ADDRESS = 0x3E;
    static constexpr uint8_t BQ27220_COMMAND_MANUFACTURER_ACCESS_CONTROL = 0x3E;
    static constexpr uint8_t BQ27220_COMMAND_BLOCK_DATA = 0x40;
    static constexpr uint8_t BQ27220_COMMAND_BLOCK_DATA_CHECKSUM = 0x60;
    static constexpr uint8_t BQ27220_COMMAND_BLOCK_DATA_LENGTH = 0x61;
    static constexpr uint16_t BQ27220_CONTROL_UNSEAL_KEY_1 = 0x8000;
    static constexpr uint16_t BQ27220_CONTROL_UNSEAL_KEY_2 = 0x8000;
    static constexpr uint16_t BQ27220_CONTROL_ALT_UNSEAL_KEY_1 = 0x0414;
    static constexpr uint16_t BQ27220_CONTROL_ALT_UNSEAL_KEY_2 = 0x3672;
    static constexpr uint16_t BQ27220_CONTROL_FULL_ACCESS = 0xFFFF;
    static constexpr uint16_t BQ27220_CONTROL_ENTER_CONFIG_UPDATE = 0x0090;
    static constexpr uint16_t BQ27220_CONTROL_EXIT_CONFIG_UPDATE_REINIT = 0x0091;
    static constexpr uint16_t BQ27220_CONTROL_EXIT_CONFIG_UPDATE = 0x0092;
    static constexpr uint16_t BQ27220_DATA_CC_OFFSET = 0x9180;
    static constexpr uint16_t BQ27220_DATA_CC_GAIN = 0x9184;
    static constexpr uint16_t BQ27220_DATA_CC_DELTA = 0x9188;
    static constexpr uint16_t BQ27220_DATA_BOARD_OFFSET = 0x91B4;
    static constexpr uint16_t BQ27220_DATA_CHARGING_VOLTAGE = 0x91FD;
    static constexpr uint16_t BQ27220_DATA_TAPER_CURRENT = 0x9201;
    static constexpr uint16_t BQ27220_DATA_BATTERY_LOW_PERCENT = 0x9251;
    static constexpr uint16_t BQ27220_DATA_SOC_FLAG_CONFIG_A = 0x927F;
    static constexpr uint16_t BQ27220_DATA_BATTERY_ID = 0x929A;
    static constexpr uint16_t BQ27220_DATA_CEDV_GAUGING_CONFIG = 0x929B;
    static constexpr uint16_t BQ27220_DATA_FULL_CHARGE_CAPACITY = 0x929D;
    static constexpr uint16_t BQ27220_DATA_DESIGN_CAPACITY = 0x929F;
    static constexpr uint16_t BQ27220_DATA_DESIGN_VOLTAGE = 0x92A3;
    static constexpr uint16_t BQ27220_DATA_TAPER_VOLTAGE = 0x92A5;
    static constexpr uint16_t BQ27220_DATA_FIXED_EDV0 = 0x92B4;
    static constexpr uint16_t BQ27220_DATA_FIXED_EDV1 = 0x92B7;
    static constexpr uint16_t BQ27220_DATA_FIXED_EDV2 = 0x92BA;
    static constexpr uint16_t BQ27220_DATA_VOLTAGE_0_DOD = 0x92BD;
    static constexpr uint16_t BQ27220_DATA_VOLTAGE_100_DOD = 0x92D1;
    static constexpr uint16_t BQ27220_CEDV_SC_MASK = 0x0010;
    static constexpr uint16_t BQ27220_CEDV_EDV_CMP_MASK = 0x0008;
    static constexpr uint16_t BQ27220_CEDV_CSYNC_MASK = 0x0002;
    static constexpr uint16_t BQ27220_CEDV_MANAGED_MASK =
        BQ27220_CEDV_SC_MASK | BQ27220_CEDV_EDV_CMP_MASK | BQ27220_CEDV_CSYNC_MASK;
    static constexpr uint16_t BQ27220_SOC_FLAG_PRIMARY_TERMINATION_MASK = 0x0C00;
    static constexpr uint16_t BQ27220_BATTERY_STATUS_FC_MASK = 0x0200;
    static constexpr uint16_t BQ27220_BATTERY_STATUS_TCA_MASK = 0x0040;
    static constexpr uint8_t BQ27220_OPERATION_STATUS_CFGUPDATE_MASK = 0x04;
    static constexpr uint8_t BQ27220_SHORT_MAC_DATA_LENGTH = 0x06;
    static constexpr uint16_t I2C_SCL_HIGH_TIMEOUT_US = 10000;
    static constexpr uint16_t I2C_BUS_FREE_DELAY_US = 80;
    static constexpr uint16_t BATTERY_PERCENT_SNAPSHOT_VALID = 0x0100;
    bool batteryPercentValid = false;
    uint8_t batteryPercent = 0;
    std::atomic<uint16_t> batteryPercentSnapshot { 0 };
    uint8_t batteryLevelBars = 0;
    std::atomic_bool batteryLowLightCutoffActive { false };
    bool batteryVoltageValid = false;
    uint16_t batteryVoltageMillivolts = 0;
    bool batteryCurrentValid = false;
    int16_t batteryCurrentMilliamps = 0;
    bool batteryAverageCurrentValid = false;
    int16_t batteryAverageCurrentMilliamps = 0;
    bool batteryStatusValid = false;
    uint16_t batteryStatus = 0;
    bool batteryRemainingCapacityValid = false;
    uint16_t batteryRemainingCapacityMah = 0;
    bool batteryFullChargeCapacityValid = false;
    uint16_t batteryFullChargeCapacityMah = 0;
    FightpadBQ27220BatteryAddon::ReadStatus batteryReadStatus = FightpadBQ27220BatteryAddon::ReadStatus::NOT_STARTED;
    bool batterySecurityStatusValid = false;
    uint8_t batterySecurityStatusBits = 0;
    bool batteryDataMemoryDebugValid = false;
    uint16_t batteryDataMemoryDebugAddress = 0;
    uint16_t batteryDataMemoryDebugOldValue = 0;
    uint16_t batteryDataMemoryDebugTargetValue = 0;
    uint16_t batteryDataMemoryDebugVerifyValue = 0;
    uint8_t batteryDataMemoryDebugOldChecksum = 0;
    uint8_t batteryDataMemoryDebugNewChecksum = 0;
    uint8_t batteryDataMemoryDebugLength = 0;
    FightpadBQ27220BatteryAddon::ConfigurationSnapshot batteryConfigurationSnapshot = {};

    bool decodeXemicsF4(const uint8_t bytes[4], double& value)
    {
        const int exponent = static_cast<int>(bytes[0]) - 128;
        const int sign = (bytes[1] & 0x80) ? -1 : 1;
        const uint32_t mantissa = 0x800000u |
            (static_cast<uint32_t>(bytes[1] & 0x7F) << 16) |
            (static_cast<uint32_t>(bytes[2]) << 8) |
            static_cast<uint32_t>(bytes[3]);
        value = static_cast<double>(sign) * std::ldexp(static_cast<double>(mantissa), exponent - 24);
        return std::isfinite(value);
    }

    uint32_t bytesToU32(const uint8_t bytes[4])
    {
        return (static_cast<uint32_t>(bytes[0]) << 24) |
            (static_cast<uint32_t>(bytes[1]) << 16) |
            (static_cast<uint32_t>(bytes[2]) << 8) |
            static_cast<uint32_t>(bytes[3]);
    }

    bool timeReached(uint32_t now, uint32_t target)
    {
        return static_cast<int32_t>(now - target) >= 0;
    }

    bool isConfigVerifyStatus(FightpadBQ27220BatteryAddon::ReadStatus status)
    {
        switch (status) {
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_TAPER_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_TAPER_VOLTAGE_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_SOC_FLAG_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_FCC_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_DESIGN_CAPACITY_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_DESIGN_VOLTAGE_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_CHARGING_VOLTAGE_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_EDV0_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_EDV1_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_EDV2_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_VOLTAGE_0_DOD_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_VOLTAGE_100_DOD_FAILED:
                return true;
            default:
                return false;
        }
    }
}

bool FightpadBQ27220BatteryAddon::available()
{
    return FIGHTPAD12SLIM_BQ27220_ENABLED &&
        FIGHTPAD12SLIM_BQ27220_SCL_PIN >= 0 &&
        FIGHTPAD12SLIM_BQ27220_SDA_PIN >= 0;
}

void FightpadBQ27220BatteryAddon::setup()
{
    batteryPercentValid = false;
    batteryPercent = 0;
    batteryPercentSnapshot.store(0, std::memory_order_release);
    batteryLevelBars = 0;
    batteryLowLightCutoffActive.store(false, std::memory_order_release);
    batteryVoltageValid = false;
    batteryVoltageMillivolts = 0;
    batteryCurrentValid = false;
    batteryCurrentMilliamps = 0;
    batteryAverageCurrentValid = false;
    batteryAverageCurrentMilliamps = 0;
    batteryStatusValid = false;
    batteryStatus = 0;
    batteryRemainingCapacityValid = false;
    batteryRemainingCapacityMah = 0;
    batteryFullChargeCapacityValid = false;
    batteryFullChargeCapacityMah = 0;
    batteryConfigAttempted = false;
    batteryConfigApplied = false;
    cedvConfigNeedsRepair = false;
    batteryLogUartConfigured = false;
    batteryReadStatus = ReadStatus::NOT_STARTED;
    batterySecurityStatusValid = false;
    batterySecurityStatusBits = 0;
    batteryDataMemoryDebugValid = false;
    batteryDataMemoryDebugAddress = 0;
    batteryDataMemoryDebugOldValue = 0;
    batteryDataMemoryDebugTargetValue = 0;
    batteryDataMemoryDebugVerifyValue = 0;
    batteryDataMemoryDebugOldChecksum = 0;
    batteryDataMemoryDebugNewChecksum = 0;
    batteryDataMemoryDebugLength = 0;
    batteryConfigurationSnapshot = {};
    nextPollTimeMs = getMillis() + FIGHTPAD12SLIM_BQ27220_BOOT_DELAY_MS;
    nextBatteryLogTimeMs = nextPollTimeMs;

    configureBatteryLogUart();

    if (FIGHTPAD12SLIM_BQ27220_GPOUT_PIN >= 0) {
        gpio_init(FIGHTPAD12SLIM_BQ27220_GPOUT_PIN);
        gpio_set_dir(FIGHTPAD12SLIM_BQ27220_GPOUT_PIN, GPIO_IN);
        gpio_disable_pulls(FIGHTPAD12SLIM_BQ27220_GPOUT_PIN);
    }
}

void FightpadBQ27220BatteryAddon::process()
{
    const uint32_t now = getMillis();
    if (!timeReached(now, nextPollTimeMs)) {
        return;
    }

    if (!pinsConfigured) {
        configurePins();
    }

#if FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
    if (!batteryConfigApplied && !batteryConfigAttempted) {
        bool configurationCurrent = false;
        bool requiresReinitialization = false;
        if (checkBatteryGaugeConfiguration(configurationCurrent, requiresReinitialization)) {
            batteryConfigAttempted = true;
            batteryConfigApplied = configurationCurrent || configureBatteryGauge(requiresReinitialization);
        }
    }
#endif

    batteryPercentValid = false;
    batteryPercentSnapshot.store(0, std::memory_order_release);
    batteryVoltageValid = false;
    batteryCurrentValid = false;
    batteryAverageCurrentValid = false;
    batteryStatusValid = false;
    batteryRemainingCapacityValid = false;
    batteryFullChargeCapacityValid = false;

    uint8_t percent = 0;
    if (readStateOfCharge(percent)) {
        batteryPercent = percent;
        batteryLevelBars = percentToBars(percent);
        batteryPercentValid = true;
        batteryPercentSnapshot.store(
            BATTERY_PERCENT_SNAPSHOT_VALID | percent,
            std::memory_order_release);
        batteryLowLightCutoffActive.store(
            percent <= FIGHTPAD12SLIM_BQ27220_LIGHTS_OFF_PERCENT,
            std::memory_order_release);
    } else {
        batteryLevelBars = 0;
    }

    uint16_t millivolts = 0;
    if (readVoltage(millivolts)) {
        batteryVoltageMillivolts = millivolts;
        batteryVoltageValid = true;
    }

    int16_t milliamps = 0;
    if (readCurrent(milliamps)) {
        batteryCurrentMilliamps = milliamps;
        batteryCurrentValid = true;
    }

    int16_t averageMilliamps = 0;
    if (readAverageCurrent(averageMilliamps)) {
        batteryAverageCurrentMilliamps = averageMilliamps;
        batteryAverageCurrentValid = true;
    }

    uint16_t status = 0;
    if (readBatteryStatus(status)) {
        batteryStatus = status;
        batteryStatusValid = true;
    }

    uint16_t remainingCapacityMah = 0;
    if (readRemainingCapacity(remainingCapacityMah)) {
        batteryRemainingCapacityMah = remainingCapacityMah;
        batteryRemainingCapacityValid = true;
    }

    uint16_t capacityMah = 0;
    if (readFullChargeCapacity(capacityMah)) {
        batteryFullChargeCapacityMah = capacityMah;
        batteryFullChargeCapacityValid = true;
    }

    if (batteryPercentValid && batteryVoltageValid && batteryCurrentValid &&
        batteryAverageCurrentValid && batteryStatusValid &&
        batteryRemainingCapacityValid && batteryFullChargeCapacityValid) {
#if FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
        if (batteryConfigApplied) {
            batteryReadStatus = ReadStatus::OK;
        }
#else
        batteryReadStatus = ReadStatus::OK;
#endif
    }

    logPeriodicBatterySnapshot(now);

    nextPollTimeMs = now + FIGHTPAD12SLIM_BQ27220_POLL_INTERVAL_MS;
}

void FightpadBQ27220BatteryAddon::configureBatteryLogUart()
{
#if FIGHTPAD12SLIM_BQ27220_LOG_UART_ENABLED
    if (batteryLogUartConfigured ||
        FIGHTPAD12SLIM_BQ27220_LOG_UART_TX_PIN < 0 ||
        FIGHTPAD12SLIM_BQ27220_LOG_UART_RX_PIN < 0) {
        return;
    }

    gpio_set_function(FIGHTPAD12SLIM_BQ27220_LOG_UART_TX_PIN,
        UART_FUNCSEL_NUM(FIGHTPAD12SLIM_BQ27220_LOG_UART, FIGHTPAD12SLIM_BQ27220_LOG_UART_TX_PIN));
    gpio_set_function(FIGHTPAD12SLIM_BQ27220_LOG_UART_RX_PIN,
        UART_FUNCSEL_NUM(FIGHTPAD12SLIM_BQ27220_LOG_UART, FIGHTPAD12SLIM_BQ27220_LOG_UART_RX_PIN));
    uart_init(FIGHTPAD12SLIM_BQ27220_LOG_UART, FIGHTPAD12SLIM_BQ27220_LOG_UART_BAUD);
    uart_set_format(FIGHTPAD12SLIM_BQ27220_LOG_UART, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(FIGHTPAD12SLIM_BQ27220_LOG_UART, false, false);
    uart_set_fifo_enabled(FIGHTPAD12SLIM_BQ27220_LOG_UART, true);
    batteryLogUartConfigured = true;
#endif
}

bool FightpadBQ27220BatteryAddon::writeBatteryLogLine(const char* line)
{
#if FIGHTPAD12SLIM_BQ27220_LOG_UART_ENABLED
    const auto writeText = [](const char* text) {
        while (*text != '\0') {
            const uint64_t waitStartUs = time_us_64();
            while (!uart_is_writable(FIGHTPAD12SLIM_BQ27220_LOG_UART)) {
                if ((time_us_64() - waitStartUs) >= FIGHTPAD12SLIM_BQ27220_LOG_UART_BYTE_TIMEOUT_US) {
                    return false;
                }
                tight_loop_contents();
            }
            uart_putc_raw(FIGHTPAD12SLIM_BQ27220_LOG_UART, *text++);
        }
        return true;
    };

    if (!writeText(line) || !writeText("\r\n")) {
        batteryLogUartConfigured = false;
        return false;
    }
    return true;
#else
    (void)line;
    return false;
#endif
}

void FightpadBQ27220BatteryAddon::logPeriodicBatterySnapshot(uint32_t now)
{
#if FIGHTPAD12SLIM_BQ27220_LOG_UART_ENABLED
    if (!timeReached(now, nextBatteryLogTimeMs)) {
        return;
    }
    nextBatteryLogTimeMs = now + FIGHTPAD12SLIM_BQ27220_LOG_INTERVAL_MS;

    if (!batteryLogUartConfigured) {
        configureBatteryLogUart();
    }
    if (!batteryLogUartConfigured) {
        return;
    }

    logBatterySnapshot();
#endif
}

void FightpadBQ27220BatteryAddon::logBatterySnapshot()
{
#if FIGHTPAD12SLIM_BQ27220_LOG_UART_ENABLED
    char line[80] = {};
    char voltage[16] = {};
    char current[16] = {};
    char fullChargeCapacity[16] = {};
    char stateOfCharge[8] = {};

    if (batteryPercentValid) {
        std::snprintf(stateOfCharge, sizeof(stateOfCharge), "%u%%",
            static_cast<unsigned int>(batteryPercent));
    } else {
        std::snprintf(stateOfCharge, sizeof(stateOfCharge), "NA");
    }
    if (batteryVoltageValid) {
        std::snprintf(voltage, sizeof(voltage), "%umV", static_cast<unsigned int>(batteryVoltageMillivolts));
    } else {
        std::snprintf(voltage, sizeof(voltage), "NA");
    }
    if (batteryCurrentValid) {
        std::snprintf(current, sizeof(current), "%+dmA", static_cast<int>(batteryCurrentMilliamps));
    } else {
        std::snprintf(current, sizeof(current), "NA");
    }
    if (batteryFullChargeCapacityValid) {
        std::snprintf(fullChargeCapacity, sizeof(fullChargeCapacity), "%umAh",
            static_cast<unsigned int>(batteryFullChargeCapacityMah));
    } else {
        std::snprintf(fullChargeCapacity, sizeof(fullChargeCapacity), "NA");
    }

    std::snprintf(line, sizeof(line),
        "SOC:%s V:%s I:%s FCC:%s",
        stateOfCharge, voltage, current, fullChargeCapacity);
    writeBatteryLogLine(line);
#endif
}

bool FightpadBQ27220BatteryAddon::isBatteryPercentValid()
{
    return (batteryPercentSnapshot.load(std::memory_order_acquire) &
            BATTERY_PERCENT_SNAPSHOT_VALID) != 0;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryPercent()
{
    return static_cast<uint8_t>(
        batteryPercentSnapshot.load(std::memory_order_acquire) & 0x00FF);
}

bool FightpadBQ27220BatteryAddon::getBatteryPercentSnapshot(uint8_t& percent)
{
    const uint16_t snapshot = batteryPercentSnapshot.load(std::memory_order_acquire);
    if ((snapshot & BATTERY_PERCENT_SNAPSHOT_VALID) == 0) {
        return false;
    }

    percent = static_cast<uint8_t>(snapshot & 0x00FF);
    return true;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryLevelBars()
{
    return batteryLevelBars;
}

bool FightpadBQ27220BatteryAddon::isLowBatteryLightCutoffActive()
{
    return batteryLowLightCutoffActive.load(std::memory_order_acquire);
}

bool FightpadBQ27220BatteryAddon::isBatteryVoltageValid()
{
    return batteryVoltageValid;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryVoltageMillivolts()
{
    return batteryVoltageMillivolts;
}

bool FightpadBQ27220BatteryAddon::isBatteryCurrentValid()
{
    return batteryCurrentValid;
}

int16_t FightpadBQ27220BatteryAddon::getBatteryCurrentMilliamps()
{
    return batteryCurrentMilliamps;
}

bool FightpadBQ27220BatteryAddon::isBatteryAverageCurrentValid()
{
    return batteryAverageCurrentValid;
}

int16_t FightpadBQ27220BatteryAddon::getBatteryAverageCurrentMilliamps()
{
    return batteryAverageCurrentMilliamps;
}

bool FightpadBQ27220BatteryAddon::isBatteryStatusValid()
{
    return batteryStatusValid;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryStatus()
{
    return batteryStatus;
}

bool FightpadBQ27220BatteryAddon::isBatteryFullChargeDetected()
{
    return batteryStatusValid && (batteryStatus & BQ27220_BATTERY_STATUS_FC_MASK) != 0;
}

bool FightpadBQ27220BatteryAddon::isBatteryTerminateChargeAlarm()
{
    return batteryStatusValid && (batteryStatus & BQ27220_BATTERY_STATUS_TCA_MASK) != 0;
}

bool FightpadBQ27220BatteryAddon::isBatteryRemainingCapacityValid()
{
    return batteryRemainingCapacityValid;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryRemainingCapacityMah()
{
    return batteryRemainingCapacityMah;
}

bool FightpadBQ27220BatteryAddon::isBatteryFullChargeCapacityValid()
{
    return batteryFullChargeCapacityValid;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryFullChargeCapacityMah()
{
    return batteryFullChargeCapacityMah;
}

bool FightpadBQ27220BatteryAddon::getConfigurationSnapshot(ConfigurationSnapshot& snapshot)
{
    snapshot = batteryConfigurationSnapshot;
    return snapshot.valid;
}

FightpadBQ27220BatteryAddon::ReadStatus FightpadBQ27220BatteryAddon::getReadStatus()
{
    return batteryReadStatus;
}

bool FightpadBQ27220BatteryAddon::isBatterySecurityStatusValid()
{
    return batterySecurityStatusValid;
}

char FightpadBQ27220BatteryAddon::getBatterySecurityStatusCode()
{
    switch (batterySecurityStatusBits) {
        case 3:
            return 'S';
        case 2:
            return 'U';
        case 1:
            return 'F';
        default:
            return '?';
    }
}

bool FightpadBQ27220BatteryAddon::isBatteryDataMemoryDebugValid()
{
    return batteryDataMemoryDebugValid;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugAddress()
{
    return batteryDataMemoryDebugAddress;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugOldValue()
{
    return batteryDataMemoryDebugOldValue;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugTargetValue()
{
    return batteryDataMemoryDebugTargetValue;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugVerifyValue()
{
    return batteryDataMemoryDebugVerifyValue;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugOldChecksum()
{
    return batteryDataMemoryDebugOldChecksum;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugNewChecksum()
{
    return batteryDataMemoryDebugNewChecksum;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugLength()
{
    return batteryDataMemoryDebugLength;
}

void FightpadBQ27220BatteryAddon::configurePins()
{
    gpio_init(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
    gpio_init(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    gpio_disable_pulls(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
    gpio_disable_pulls(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    sleep_us(FIGHTPAD12SLIM_BQ27220_I2C_DELAY_US * 2);
    pinsConfigured = true;
}

bool FightpadBQ27220BatteryAddon::checkBatteryGaugeConfiguration(bool& configurationCurrent, bool& requiresReinitialization)
{
#if !FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
    configurationCurrent = true;
    requiresReinitialization = false;
    return true;
#else
    configurationCurrent = false;
    requiresReinitialization = false;

    uint16_t designCapacity = 0;
    if (!readWord(BQ27220_COMMAND_DESIGN_CAPACITY, designCapacity)) {
        return false;
    }

    if (!enterFullAccessMode()) {
        return false;
    }

    if (!readConfigurationSnapshot(false)) {
        return false;
    }

    uint16_t cedvConfig = 0;
    if (!readDataMemoryWord(BQ27220_DATA_CEDV_GAUGING_CONFIG, cedvConfig)) {
        batteryConfigurationSnapshot.overallResult = ConfigCheckResult::BAD;
        return false;
    }

    uint16_t targetCedvBits = 0;
    if (FIGHTPAD12SLIM_BQ27220_INDEPENDENT_CHARGER) {
        targetCedvBits |= BQ27220_CEDV_SC_MASK;
    }
    if (FIGHTPAD12SLIM_BQ27220_EDV_CMP) {
        targetCedvBits |= BQ27220_CEDV_EDV_CMP_MASK;
    }
    if (FIGHTPAD12SLIM_BQ27220_CSYNC) {
        targetCedvBits |= BQ27220_CEDV_CSYNC_MASK;
    }

    requiresReinitialization = designCapacity != FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH;
    batteryConfigurationSnapshot.ramReinitializationRequired = requiresReinitialization;
    cedvConfigNeedsRepair = (cedvConfig & BQ27220_CEDV_MANAGED_MASK) != targetCedvBits;

    const bool dataMemoryCurrent =
        batteryConfigurationSnapshot.chargingVoltage.before == batteryConfigurationSnapshot.chargingVoltage.target &&
        batteryConfigurationSnapshot.taperCurrent.before == batteryConfigurationSnapshot.taperCurrent.target &&
        batteryConfigurationSnapshot.taperVoltage.before == batteryConfigurationSnapshot.taperVoltage.target &&
        batteryConfigurationSnapshot.socFlagConfigA.before == batteryConfigurationSnapshot.socFlagConfigA.target &&
        batteryConfigurationSnapshot.batteryLow.before == batteryConfigurationSnapshot.batteryLow.target &&
        batteryConfigurationSnapshot.edv0.before == batteryConfigurationSnapshot.edv0.target &&
        batteryConfigurationSnapshot.edv1.before == batteryConfigurationSnapshot.edv1.target &&
        batteryConfigurationSnapshot.edv2.before == batteryConfigurationSnapshot.edv2.target;
    configurationCurrent = !requiresReinitialization && !cedvConfigNeedsRepair && dataMemoryCurrent;
    if (configurationCurrent) {
        finalizeConfigurationSnapshot();
    }
    return true;
#endif
}

bool FightpadBQ27220BatteryAddon::configureBatteryGauge(bool reinitialize)
{
#if !FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
    return true;
#else
    bool configEntryCommandSent = false;
    bool enteredConfig = false;
    bool ok = enterFullAccessMode();
    if (ok) {
        configEntryCommandSent = writeControlWord(BQ27220_CONTROL_ENTER_CONFIG_UPDATE);
        ok = configEntryCommandSent;
    }

    if (ok) {
        enteredConfig = waitForConfigUpdateMode(true);
        ok = enteredConfig;
    }

    if (ok && (reinitialize ||
        batteryConfigurationSnapshot.chargingVoltage.before != batteryConfigurationSnapshot.chargingVoltage.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_CHARGING_VOLTAGE, FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_CHARGING_VOLTAGE_FAILED);
    }
    if (ok && (reinitialize ||
        batteryConfigurationSnapshot.taperCurrent.before != batteryConfigurationSnapshot.taperCurrent.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_TAPER_CURRENT, FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA, ReadStatus::CONFIG_VERIFY_TAPER_FAILED);
    }
    if (ok && (reinitialize ||
        batteryConfigurationSnapshot.taperVoltage.before != batteryConfigurationSnapshot.taperVoltage.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_TAPER_VOLTAGE, FIGHTPAD12SLIM_BQ27220_TAPER_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_TAPER_VOLTAGE_FAILED);
    }
    if (ok && (reinitialize || batteryConfigurationSnapshot.batteryLow.before != batteryConfigurationSnapshot.batteryLow.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_BATTERY_LOW_PERCENT, FIGHTPAD12SLIM_BQ27220_BATTERY_LOW_PERCENT_X100, ReadStatus::CONFIG_VERIFY_FAILED);
    }
    if (ok && (reinitialize || cedvConfigNeedsRepair)) {
        uint16_t cedvBits = 0;
        if (FIGHTPAD12SLIM_BQ27220_INDEPENDENT_CHARGER) {
            cedvBits |= BQ27220_CEDV_SC_MASK;
        }
        if (FIGHTPAD12SLIM_BQ27220_EDV_CMP) {
            cedvBits |= BQ27220_CEDV_EDV_CMP_MASK;
        }
        if (FIGHTPAD12SLIM_BQ27220_CSYNC) {
            cedvBits |= BQ27220_CEDV_CSYNC_MASK;
        }
        ok = updateDataMemoryWordBits(BQ27220_DATA_CEDV_GAUGING_CONFIG, BQ27220_CEDV_MANAGED_MASK, cedvBits, ReadStatus::CONFIG_VERIFY_FAILED);
    }
    if (ok && (reinitialize ||
        batteryConfigurationSnapshot.socFlagConfigA.before != batteryConfigurationSnapshot.socFlagConfigA.target)) {
        ok = updateDataMemoryWordBits(BQ27220_DATA_SOC_FLAG_CONFIG_A, 0, BQ27220_SOC_FLAG_PRIMARY_TERMINATION_MASK, ReadStatus::CONFIG_VERIFY_SOC_FLAG_FAILED);
    }
    // Restore the FCC baseline only after the gauge RAM has returned to defaults.
    if (ok && reinitialize) {
        ok = writeDataMemoryWord(BQ27220_DATA_FULL_CHARGE_CAPACITY, FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH, ReadStatus::CONFIG_VERIFY_FCC_FAILED);
    }
    if (ok && reinitialize) {
        ok = writeDataMemoryWord(BQ27220_DATA_DESIGN_CAPACITY, FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH, ReadStatus::CONFIG_VERIFY_DESIGN_CAPACITY_FAILED);
    }
    if (ok && reinitialize) {
        ok = writeDataMemoryWord(BQ27220_DATA_DESIGN_VOLTAGE, FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_DESIGN_VOLTAGE_FAILED);
    }
    if (ok && (reinitialize || batteryConfigurationSnapshot.edv0.before != batteryConfigurationSnapshot.edv0.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_FIXED_EDV0, FIGHTPAD12SLIM_BQ27220_EDV0_MV, ReadStatus::CONFIG_VERIFY_EDV0_FAILED);
    }
    if (ok && (reinitialize || batteryConfigurationSnapshot.edv1.before != batteryConfigurationSnapshot.edv1.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_FIXED_EDV1, FIGHTPAD12SLIM_BQ27220_EDV1_MV, ReadStatus::CONFIG_VERIFY_EDV1_FAILED);
    }
    if (ok && (reinitialize || batteryConfigurationSnapshot.edv2.before != batteryConfigurationSnapshot.edv2.target)) {
        ok = writeDataMemoryWord(BQ27220_DATA_FIXED_EDV2, FIGHTPAD12SLIM_BQ27220_EDV2_MV, ReadStatus::CONFIG_VERIFY_EDV2_FAILED);
    }
    if (ok && reinitialize) {
        ok = writeDataMemoryWord(BQ27220_DATA_VOLTAGE_0_DOD, FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_VOLTAGE_0_DOD_FAILED);
    }
    if (ok && reinitialize) {
        ok = writeDataMemoryWord(BQ27220_DATA_VOLTAGE_100_DOD, FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_VOLTAGE_100_DOD_FAILED);
    }

    bool exitOk = true;
    // Once ENTER_CFG_UPDATE was accepted, always try to leave the mode even
    // when the status poll or a later write failed.
    if (configEntryCommandSent) {
        const uint16_t exitCommand = reinitialize ?
            BQ27220_CONTROL_EXIT_CONFIG_UPDATE_REINIT :
            BQ27220_CONTROL_EXIT_CONFIG_UPDATE;
        exitOk = writeControlWord(exitCommand);
        if (exitOk) {
            exitOk = waitForConfigUpdateMode(false);
        }
    }

    if (!ok || !exitOk) {
        finalizeConfigurationSnapshot();
        batteryConfigurationSnapshot.overallResult = ConfigCheckResult::BAD;
        if (!isConfigVerifyStatus(batteryReadStatus) &&
            batteryReadStatus != ReadStatus::CONFIG_FULL_ACCESS_FAILED) {
            batteryReadStatus = ReadStatus::CONFIG_UPDATE_FAILED;
        }
        return false;
    }

    if (!enterFullAccessMode() || !readConfigurationSnapshot(true)) {
        finalizeConfigurationSnapshot();
        batteryConfigurationSnapshot.overallResult = ConfigCheckResult::BAD;
        return false;
    }

    return batteryConfigurationSnapshot.overallResult != ConfigCheckResult::BAD;
#endif
}

bool FightpadBQ27220BatteryAddon::enterFullAccessMode()
{
    if (sendFullAccessKeys() && waitForFullAccessMode()) {
        return true;
    }

    if (sendUnsealKeys(BQ27220_CONTROL_ALT_UNSEAL_KEY_1, BQ27220_CONTROL_ALT_UNSEAL_KEY_2) &&
        sendFullAccessKeys() && waitForFullAccessMode()) {
        return true;
    }

    if (sendUnsealKeys(BQ27220_CONTROL_UNSEAL_KEY_1, BQ27220_CONTROL_UNSEAL_KEY_2) &&
        sendFullAccessKeys() && waitForFullAccessMode()) {
        return true;
    }

    if (sendFullAccessKeysViaManufacturerAccess() && waitForFullAccessMode()) {
        return true;
    }

    batteryReadStatus = ReadStatus::CONFIG_FULL_ACCESS_FAILED;
    return false;
}

bool FightpadBQ27220BatteryAddon::sendUnsealKeys(uint16_t key1, uint16_t key2)
{
    bool ok = writeControlWord(key1);
    ok = ok && writeControlWord(key2);
    sleep_ms(10);
    return ok;
}

bool FightpadBQ27220BatteryAddon::sendFullAccessKeys()
{
    bool ok = writeControlWord(BQ27220_CONTROL_FULL_ACCESS);
    ok = ok && writeControlWord(BQ27220_CONTROL_FULL_ACCESS);
    sleep_ms(10);
    return ok;
}

bool FightpadBQ27220BatteryAddon::sendFullAccessKeysViaManufacturerAccess()
{
    bool ok = writeManufacturerAccessWord(BQ27220_CONTROL_FULL_ACCESS);
    ok = ok && writeManufacturerAccessWord(BQ27220_CONTROL_FULL_ACCESS);
    sleep_ms(10);
    return ok;
}

bool FightpadBQ27220BatteryAddon::waitForFullAccessMode()
{
    const uint32_t deadline = getMillis() + 1000;
    do {
        uint8_t statusLow = 0;
        uint8_t statusHigh = 0;
        if (readOperationStatus(statusLow, statusHigh) && batterySecurityStatusBits == 1) {
            return true;
        }
        sleep_ms(20);
    } while (!timeReached(getMillis(), deadline));

    batteryReadStatus = ReadStatus::CONFIG_FULL_ACCESS_FAILED;
    return false;
}

bool FightpadBQ27220BatteryAddon::waitForConfigUpdateMode(bool enabled)
{
    // TI requires at least 2 s before checking CFGUPDATE after ENTER_CFG_UPDATE.
    const uint32_t deadline = getMillis() + (enabled ? 3000u : 1000u);
    if (enabled) {
        sleep_ms(2000);
    }
    do {
        uint8_t statusLow = 0;
        uint8_t statusHigh = 0;
        if (readOperationStatus(statusLow, statusHigh)) {
            const bool cfgUpdate = (statusHigh & BQ27220_OPERATION_STATUS_CFGUPDATE_MASK) != 0;
            if (cfgUpdate == enabled) {
                return true;
            }
        }
        sleep_ms(20);
    } while (!timeReached(getMillis(), deadline));

    batteryReadStatus = ReadStatus::CONFIG_UPDATE_FAILED;
    return false;
}

bool FightpadBQ27220BatteryAddon::readOperationStatus(uint8_t& statusLow, uint8_t& statusHigh)
{
    uint8_t status[2] = {0, 0};
    if (!readRegisterBytes(BQ27220_COMMAND_OPERATION_STATUS, status, sizeof(status))) {
        return false;
    }

    statusLow = status[0];
    statusHigh = status[1];
    batterySecurityStatusBits = static_cast<uint8_t>((statusLow >> 1) & 0x03);
    batterySecurityStatusValid = true;
    return true;
}

bool FightpadBQ27220BatteryAddon::writeControlWord(uint16_t command)
{
    const uint8_t data[2] = {
        static_cast<uint8_t>(command & 0xFF),
        static_cast<uint8_t>((command >> 8) & 0xFF),
    };
    const bool ok = writeRegisterBytes(BQ27220_COMMAND_CONTROL, data, sizeof(data));
    sleep_ms(2);
    return ok;
}

bool FightpadBQ27220BatteryAddon::writeManufacturerAccessWord(uint16_t command)
{
    const uint8_t data[2] = {
        static_cast<uint8_t>(command & 0xFF),
        static_cast<uint8_t>((command >> 8) & 0xFF),
    };
    const bool ok = writeRegisterBytes(BQ27220_COMMAND_MANUFACTURER_ACCESS_CONTROL, data, sizeof(data));
    sleep_ms(2);
    return ok;
}

bool FightpadBQ27220BatteryAddon::readDataMemoryBytes(uint16_t address, uint8_t* data, uint8_t length)
{
    if (data == nullptr || length == 0 || length > 32) {
        batteryReadStatus = ReadStatus::VALUE_OUT_OF_RANGE;
        return false;
    }

    const uint8_t addressBytes[2] = {
        static_cast<uint8_t>(address & 0xFF),
        static_cast<uint8_t>((address >> 8) & 0xFF),
    };
    if (!writeRegisterBytes(BQ27220_COMMAND_DATA_MEMORY_ADDRESS, addressBytes, sizeof(addressBytes))) {
        return false;
    }
    sleep_ms(10);

    return readRegisterBytes(BQ27220_COMMAND_BLOCK_DATA, data, length);
}

bool FightpadBQ27220BatteryAddon::readDataMemoryWord(uint16_t address, uint16_t& value)
{
    uint8_t data[2] = {0, 0};
    if (!readDataMemoryBytes(address, data, sizeof(data))) {
        return false;
    }

    value = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    return true;
}

bool FightpadBQ27220BatteryAddon::readConfigurationSnapshot(bool afterRepair)
{
    uint8_t batteryId = 0;
    uint16_t chargingVoltage = 0;
    uint16_t taperCurrent = 0;
    uint16_t taperVoltage = 0;
    uint16_t socFlagConfigA = 0;
    uint16_t batteryLow = 0;
    uint16_t edv0 = 0;
    uint16_t edv1 = 0;
    uint16_t edv2 = 0;
    uint16_t ccOffsetRaw = 0;
    uint8_t boardOffsetRaw = 0;
    uint8_t ccGainBytes[4] = {0, 0, 0, 0};
    uint8_t ccDeltaBytes[4] = {0, 0, 0, 0};

    const bool readOk =
        readDataMemoryBytes(BQ27220_DATA_BATTERY_ID, &batteryId, 1) &&
        readDataMemoryWord(BQ27220_DATA_CHARGING_VOLTAGE, chargingVoltage) &&
        readDataMemoryWord(BQ27220_DATA_TAPER_CURRENT, taperCurrent) &&
        readDataMemoryWord(BQ27220_DATA_TAPER_VOLTAGE, taperVoltage) &&
        readDataMemoryWord(BQ27220_DATA_SOC_FLAG_CONFIG_A, socFlagConfigA) &&
        readDataMemoryWord(BQ27220_DATA_BATTERY_LOW_PERCENT, batteryLow) &&
        readDataMemoryWord(BQ27220_DATA_FIXED_EDV0, edv0) &&
        readDataMemoryWord(BQ27220_DATA_FIXED_EDV1, edv1) &&
        readDataMemoryWord(BQ27220_DATA_FIXED_EDV2, edv2) &&
        readDataMemoryWord(BQ27220_DATA_CC_OFFSET, ccOffsetRaw) &&
        readDataMemoryBytes(BQ27220_DATA_BOARD_OFFSET, &boardOffsetRaw, 1) &&
        readDataMemoryBytes(BQ27220_DATA_CC_GAIN, ccGainBytes, sizeof(ccGainBytes)) &&
        readDataMemoryBytes(BQ27220_DATA_CC_DELTA, ccDeltaBytes, sizeof(ccDeltaBytes));

    if (!readOk) {
        batteryConfigurationSnapshot.valid = true;
        batteryConfigurationSnapshot.overallResult = ConfigCheckResult::BAD;
        return false;
    }

    double ccGain = 0.0;
    double ccDelta = 0.0;
    const bool ccGainDecoded = decodeXemicsF4(ccGainBytes, ccGain) && ccGain >= 0.1 && ccGain <= 4.0;
    const bool ccDeltaDecoded = decodeXemicsF4(ccDeltaBytes, ccDelta) && ccDelta >= 30000.0 && ccDelta <= 3000000.0;

    if (!afterRepair) {
        batteryConfigurationSnapshot = {};
        batteryConfigurationSnapshot.currentCalibrated = FIGHTPAD12SLIM_BQ27220_CURRENT_CALIBRATED != 0;
        batteryConfigurationSnapshot.batteryIdValid = true;
        batteryConfigurationSnapshot.batteryId = batteryId;

        batteryConfigurationSnapshot.chargingVoltage = {
            true, chargingVoltage, FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV, chargingVoltage,
            ConfigCheckResult::NOT_CHECKED,
        };
        batteryConfigurationSnapshot.taperCurrent = {
            true, taperCurrent, FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA, taperCurrent,
            ConfigCheckResult::NOT_CHECKED,
        };
        batteryConfigurationSnapshot.taperVoltage = {
            true, taperVoltage, FIGHTPAD12SLIM_BQ27220_TAPER_VOLTAGE_MV, taperVoltage,
            ConfigCheckResult::NOT_CHECKED,
        };
        batteryConfigurationSnapshot.socFlagConfigA = {
            true, socFlagConfigA,
            static_cast<uint16_t>(socFlagConfigA | BQ27220_SOC_FLAG_PRIMARY_TERMINATION_MASK),
            socFlagConfigA, ConfigCheckResult::NOT_CHECKED,
        };

        batteryConfigurationSnapshot.batteryLow = {
            true, batteryLow, FIGHTPAD12SLIM_BQ27220_BATTERY_LOW_PERCENT_X100, batteryLow,
            ConfigCheckResult::NOT_CHECKED,
        };
        batteryConfigurationSnapshot.edv0 = {
            true, edv0, FIGHTPAD12SLIM_BQ27220_EDV0_MV, edv0,
            ConfigCheckResult::NOT_CHECKED,
        };
        batteryConfigurationSnapshot.edv1 = {
            true, edv1, FIGHTPAD12SLIM_BQ27220_EDV1_MV, edv1,
            ConfigCheckResult::NOT_CHECKED,
        };
        batteryConfigurationSnapshot.edv2 = {
            true, edv2, FIGHTPAD12SLIM_BQ27220_EDV2_MV, edv2,
            ConfigCheckResult::NOT_CHECKED,
        };
    } else {
        batteryConfigurationSnapshot.batteryIdValid = true;
        batteryConfigurationSnapshot.batteryId = batteryId;
        batteryConfigurationSnapshot.chargingVoltage.after = chargingVoltage;
        batteryConfigurationSnapshot.taperCurrent.after = taperCurrent;
        batteryConfigurationSnapshot.taperVoltage.after = taperVoltage;
        batteryConfigurationSnapshot.socFlagConfigA.after = socFlagConfigA;
        batteryConfigurationSnapshot.batteryLow.after = batteryLow;
        batteryConfigurationSnapshot.edv0.after = edv0;
        batteryConfigurationSnapshot.edv1.after = edv1;
        batteryConfigurationSnapshot.edv2.after = edv2;
    }

    batteryConfigurationSnapshot.ccOffsetValid = true;
    batteryConfigurationSnapshot.ccOffset = static_cast<int16_t>(ccOffsetRaw);
    batteryConfigurationSnapshot.boardOffsetValid = true;
    batteryConfigurationSnapshot.boardOffset = static_cast<int8_t>(boardOffsetRaw);
    batteryConfigurationSnapshot.ccGainValid = ccGainDecoded;
    batteryConfigurationSnapshot.ccGainRaw = bytesToU32(ccGainBytes);
    batteryConfigurationSnapshot.ccGainMicro = ccGainDecoded ?
        static_cast<uint32_t>(ccGain * 1000000.0 + 0.5) : 0;
    batteryConfigurationSnapshot.ccDeltaValid = ccDeltaDecoded;
    batteryConfigurationSnapshot.ccDeltaRaw = bytesToU32(ccDeltaBytes);
    batteryConfigurationSnapshot.ccDeltaRounded = ccDeltaDecoded ?
        static_cast<uint32_t>(ccDelta + 0.5) : 0;
    batteryConfigurationSnapshot.valid = true;

    if (afterRepair) {
        finalizeConfigurationSnapshot();
    }
    return true;
}

void FightpadBQ27220BatteryAddon::finalizeConfigurationSnapshot()
{
    auto finalizeWord = [](ConfigWordSnapshot& word) {
        if (!word.valid || word.after != word.target) {
            word.result = ConfigCheckResult::BAD;
        } else if (word.before == word.target) {
            word.result = ConfigCheckResult::OK;
        } else {
            word.result = ConfigCheckResult::FIXED;
        }
    };

    finalizeWord(batteryConfigurationSnapshot.batteryLow);
    finalizeWord(batteryConfigurationSnapshot.chargingVoltage);
    finalizeWord(batteryConfigurationSnapshot.taperCurrent);
    finalizeWord(batteryConfigurationSnapshot.taperVoltage);
    finalizeWord(batteryConfigurationSnapshot.socFlagConfigA);
    finalizeWord(batteryConfigurationSnapshot.edv0);
    finalizeWord(batteryConfigurationSnapshot.edv1);
    finalizeWord(batteryConfigurationSnapshot.edv2);

    const ConfigWordSnapshot* checkedWords[] = {
        &batteryConfigurationSnapshot.chargingVoltage,
        &batteryConfigurationSnapshot.taperCurrent,
        &batteryConfigurationSnapshot.taperVoltage,
        &batteryConfigurationSnapshot.socFlagConfigA,
        &batteryConfigurationSnapshot.batteryLow,
        &batteryConfigurationSnapshot.edv0,
        &batteryConfigurationSnapshot.edv1,
        &batteryConfigurationSnapshot.edv2,
    };

    ConfigCheckResult overall =
        (batteryConfigurationSnapshot.ramReinitializationRequired || cedvConfigNeedsRepair) ?
        ConfigCheckResult::FIXED : ConfigCheckResult::OK;
    for (const ConfigWordSnapshot* word : checkedWords) {
        if (word->result == ConfigCheckResult::BAD) {
            overall = ConfigCheckResult::BAD;
            break;
        }
        if (word->result == ConfigCheckResult::FIXED) {
            overall = ConfigCheckResult::FIXED;
        }
    }

    if (!batteryConfigurationSnapshot.batteryIdValid ||
        !batteryConfigurationSnapshot.ccOffsetValid ||
        !batteryConfigurationSnapshot.boardOffsetValid ||
        !batteryConfigurationSnapshot.ccGainValid ||
        !batteryConfigurationSnapshot.ccDeltaValid) {
        overall = ConfigCheckResult::BAD;
    }

    batteryConfigurationSnapshot.valid = true;
    batteryConfigurationSnapshot.overallResult = overall;
}

bool FightpadBQ27220BatteryAddon::updateDataMemoryWordBits(uint16_t address, uint16_t clearMask, uint16_t setMask, ReadStatus verifyFailureStatus)
{
    uint16_t value = 0;
    if (!readDataMemoryWord(address, value)) {
        return false;
    }

    value = static_cast<uint16_t>((value & static_cast<uint16_t>(~clearMask)) | setMask);
    return writeDataMemoryWord(address, value, verifyFailureStatus);
}

bool FightpadBQ27220BatteryAddon::writeDataMemoryWord(uint16_t address, uint16_t value, ReadStatus verifyFailureStatus)
{
    batteryDataMemoryDebugValid = false;

    const uint8_t addressBytes[2] = {
        static_cast<uint8_t>(address & 0xFF),
        static_cast<uint8_t>((address >> 8) & 0xFF),
    };
    if (!writeRegisterBytes(BQ27220_COMMAND_DATA_MEMORY_ADDRESS, addressBytes, sizeof(addressBytes))) {
        return false;
    }
    sleep_ms(10);

    uint8_t oldBytes[2] = {0, 0};
    if (!readRegisterBytes(BQ27220_COMMAND_BLOCK_DATA, oldBytes, sizeof(oldBytes))) {
        return false;
    }

    uint8_t oldChecksum = 0;
    if (!readRegisterBytes(BQ27220_COMMAND_BLOCK_DATA_CHECKSUM, &oldChecksum, 1)) {
        return false;
    }

    const uint8_t newBytes[2] = {
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    uint8_t newChecksum = oldChecksum;

    if (oldBytes[0] != newBytes[0] || oldBytes[1] != newBytes[1]) {
        const uint8_t macData[4] = {
            addressBytes[0],
            addressBytes[1],
            newBytes[0],
            newBytes[1],
        };
        if (!writeRegisterBytes(BQ27220_COMMAND_DATA_MEMORY_ADDRESS, macData, sizeof(macData))) {
            return false;
        }

        const uint8_t macDataSum = static_cast<uint8_t>(macData[0] + macData[1] + macData[2] + macData[3]);
        newChecksum = static_cast<uint8_t>(0xFF - macDataSum);
        const uint8_t commitBytes[2] = {newChecksum, BQ27220_SHORT_MAC_DATA_LENGTH};
        if (!writeRegisterBytes(BQ27220_COMMAND_BLOCK_DATA_CHECKSUM, commitBytes, sizeof(commitBytes))) {
            return false;
        }
        sleep_ms(120);
    }

    if (!writeRegisterBytes(BQ27220_COMMAND_DATA_MEMORY_ADDRESS, addressBytes, sizeof(addressBytes))) {
        return false;
    }
    sleep_ms(10);

    uint8_t verifyBytes[2] = {0, 0};
    if (!readRegisterBytes(BQ27220_COMMAND_BLOCK_DATA, verifyBytes, sizeof(verifyBytes))) {
        return false;
    }

    batteryDataMemoryDebugAddress = address;
    batteryDataMemoryDebugOldValue = static_cast<uint16_t>((static_cast<uint16_t>(oldBytes[0]) << 8) | oldBytes[1]);
    batteryDataMemoryDebugTargetValue = value;
    batteryDataMemoryDebugVerifyValue = static_cast<uint16_t>((static_cast<uint16_t>(verifyBytes[0]) << 8) | verifyBytes[1]);
    batteryDataMemoryDebugOldChecksum = oldChecksum;
    batteryDataMemoryDebugNewChecksum = newChecksum;
    batteryDataMemoryDebugLength = BQ27220_SHORT_MAC_DATA_LENGTH;
    batteryDataMemoryDebugValid = true;

    if (verifyBytes[0] != newBytes[0] || verifyBytes[1] != newBytes[1]) {
        batteryReadStatus = verifyFailureStatus;
        return false;
    }

    return true;
}

bool FightpadBQ27220BatteryAddon::readRegisterBytes(uint8_t command, uint8_t* data, uint8_t length)
{
    if (length == 0) {
        return true;
    }

    startCondition();
    if (!writeByte((BQ27220_I2C_ADDRESS << 1) | 0, ReadStatus::ADDRESS_WRITE_NACK)) {
        stopCondition();
        return false;
    }

    if (!writeByte(command, ReadStatus::COMMAND_NACK)) {
        stopCondition();
        return false;
    }

    startCondition();
    if (!writeByte((BQ27220_I2C_ADDRESS << 1) | 1, ReadStatus::ADDRESS_READ_NACK)) {
        stopCondition();
        return false;
    }

    for (uint8_t index = 0; index < length; index++) {
        if (!readByte(data[index], index < (length - 1))) {
            stopCondition();
            return false;
        }
    }

    stopCondition();
    return true;
}

bool FightpadBQ27220BatteryAddon::writeRegisterBytes(uint8_t command, const uint8_t* data, uint8_t length)
{
    startCondition();
    if (!writeByte((BQ27220_I2C_ADDRESS << 1) | 0, ReadStatus::ADDRESS_WRITE_NACK)) {
        stopCondition();
        return false;
    }

    if (!writeByte(command, ReadStatus::COMMAND_NACK)) {
        stopCondition();
        return false;
    }

    for (uint8_t index = 0; index < length; index++) {
        if (!writeByte(data[index], ReadStatus::COMMAND_NACK)) {
            stopCondition();
            return false;
        }
    }

    stopCondition();
    return true;
}

bool FightpadBQ27220BatteryAddon::readStateOfCharge(uint8_t& percent)
{
    uint16_t value = 0;
    if (!readWord(BQ27220_COMMAND_STATE_OF_CHARGE, value)) {
        return false;
    }

    if (value > 100) {
        batteryReadStatus = ReadStatus::VALUE_OUT_OF_RANGE;
        return false;
    }

    percent = static_cast<uint8_t>(value);
    return true;
}

bool FightpadBQ27220BatteryAddon::readVoltage(uint16_t& millivolts)
{
    return readWord(BQ27220_COMMAND_VOLTAGE, millivolts);
}

bool FightpadBQ27220BatteryAddon::readCurrent(int16_t& milliamps)
{
    uint16_t value = 0;
    if (!readWord(BQ27220_COMMAND_CURRENT, value)) {
        return false;
    }

    int32_t signedValue = value;
    if (value & 0x8000) {
        signedValue -= 0x10000;
    }
    milliamps = static_cast<int16_t>(signedValue);
    return true;
}

bool FightpadBQ27220BatteryAddon::readBatteryStatus(uint16_t& status)
{
    return readWord(BQ27220_COMMAND_BATTERY_STATUS, status);
}

bool FightpadBQ27220BatteryAddon::readAverageCurrent(int16_t& milliamps)
{
    uint16_t value = 0;
    if (!readWord(BQ27220_COMMAND_AVERAGE_CURRENT, value)) {
        return false;
    }

    int32_t signedValue = value;
    if (value & 0x8000) {
        signedValue -= 0x10000;
    }
    milliamps = static_cast<int16_t>(signedValue);
    return true;
}

bool FightpadBQ27220BatteryAddon::readRemainingCapacity(uint16_t& capacityMah)
{
    return readWord(BQ27220_COMMAND_REMAINING_CAPACITY, capacityMah);
}

bool FightpadBQ27220BatteryAddon::readFullChargeCapacity(uint16_t& capacityMah)
{
    return readWord(BQ27220_COMMAND_FULL_CHARGE_CAPACITY, capacityMah);
}

uint8_t FightpadBQ27220BatteryAddon::percentToBars(uint8_t percent) const
{
    if (percent == 0) {
        return 0;
    }
    return ((percent - 1) / 25) + 1;
}

bool FightpadBQ27220BatteryAddon::readWord(uint8_t command, uint16_t& value)
{
    startCondition();
    if (!writeByte((BQ27220_I2C_ADDRESS << 1) | 0, ReadStatus::ADDRESS_WRITE_NACK)) {
        stopCondition();
        return false;
    }

    if (!writeByte(command, ReadStatus::COMMAND_NACK)) {
        stopCondition();
        return false;
    }

    startCondition();
    if (!writeByte((BQ27220_I2C_ADDRESS << 1) | 1, ReadStatus::ADDRESS_READ_NACK)) {
        stopCondition();
        return false;
    }

    uint8_t low = 0;
    uint8_t high = 0;
    if (!readByte(low, true) || !readByte(high, false)) {
        stopCondition();
        return false;
    }

    stopCondition();
    value = static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
    return true;
}

bool FightpadBQ27220BatteryAddon::writeByte(uint8_t value, ReadStatus nackStatus)
{
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        if (value & mask) {
            releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
        } else {
            driveLow(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
        }

        if (!setSclHigh()) {
            driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
            releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
            batteryReadStatus = ReadStatus::BUS_TIMEOUT;
            return false;
        }
        driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
        delayHalfPeriod();
    }

    releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    if (!setSclHigh()) {
        driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
        batteryReadStatus = ReadStatus::BUS_TIMEOUT;
        return false;
    }

    const bool ack = !gpio_get(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
    delayHalfPeriod();
    if (!ack) {
        batteryReadStatus = nackStatus;
    }
    return ack;
}

bool FightpadBQ27220BatteryAddon::readByte(uint8_t& value, bool ack)
{
    value = 0;
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);

    for (uint8_t bit = 0; bit < 8; bit++) {
        value <<= 1;
        if (!setSclHigh()) {
            driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
            batteryReadStatus = ReadStatus::BUS_TIMEOUT;
            return false;
        }

        if (gpio_get(FIGHTPAD12SLIM_BQ27220_SDA_PIN)) {
            value |= 1;
        }

        driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
        delayHalfPeriod();
    }

    if (ack) {
        driveLow(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    } else {
        releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    }

    const bool clockedAck = setSclHigh();
    driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    delayHalfPeriod();
    if (!clockedAck) {
        batteryReadStatus = ReadStatus::BUS_TIMEOUT;
    }
    return clockedAck;
}

void FightpadBQ27220BatteryAddon::startCondition()
{
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    setSclHigh();
    driveLow(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    delayHalfPeriod();
    driveLow(FIGHTPAD12SLIM_BQ27220_SCL_PIN);
    delayHalfPeriod();
}

void FightpadBQ27220BatteryAddon::stopCondition()
{
    driveLow(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    delayHalfPeriod();
    setSclHigh();
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SDA_PIN);
    delayHalfPeriod();
    sleep_us(I2C_BUS_FREE_DELAY_US);
}

bool FightpadBQ27220BatteryAddon::setSclHigh()
{
    releaseHigh(FIGHTPAD12SLIM_BQ27220_SCL_PIN);

    for (uint16_t elapsedUs = 0; elapsedUs < I2C_SCL_HIGH_TIMEOUT_US; elapsedUs++) {
        if (gpio_get(FIGHTPAD12SLIM_BQ27220_SCL_PIN)) {
            delayHalfPeriod();
            return true;
        }
        sleep_us(1);
    }

    return false;
}

void FightpadBQ27220BatteryAddon::driveLow(int pin) const
{
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
}

void FightpadBQ27220BatteryAddon::releaseHigh(int pin) const
{
    gpio_set_dir(pin, GPIO_IN);
}

void FightpadBQ27220BatteryAddon::delayHalfPeriod() const
{
    sleep_us(FIGHTPAD12SLIM_BQ27220_I2C_DELAY_US);
}
