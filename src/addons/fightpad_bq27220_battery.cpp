#include "addons/fightpad_bq27220_battery.h"

#include "pico/stdlib.h"

namespace
{
    static constexpr uint8_t BQ27220_I2C_ADDRESS = 0x55;
    static constexpr uint8_t BQ27220_COMMAND_CONTROL = 0x00;
    static constexpr uint8_t BQ27220_COMMAND_VOLTAGE = 0x08;
    static constexpr uint8_t BQ27220_COMMAND_CURRENT = 0x0C;
    static constexpr uint8_t BQ27220_COMMAND_FULL_CHARGE_CAPACITY = 0x12;
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
    static constexpr uint16_t BQ27220_DATA_CHARGING_VOLTAGE = 0x91FD;
    static constexpr uint16_t BQ27220_DATA_TAPER_CURRENT = 0x9201;
    static constexpr uint16_t BQ27220_DATA_BATTERY_LOW_PERCENT = 0x9251;
    static constexpr uint16_t BQ27220_DATA_SOC_FLAG_CONFIG_A = 0x927F;
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
    static constexpr uint8_t BQ27220_OPERATION_STATUS_CFGUPDATE_MASK = 0x04;
    static constexpr uint8_t BQ27220_SHORT_MAC_DATA_LENGTH = 0x06;
    static constexpr uint16_t I2C_SCL_HIGH_TIMEOUT_US = 10000;
    static constexpr uint16_t I2C_BUS_FREE_DELAY_US = 80;

    bool batteryPercentValid = false;
    uint8_t batteryPercent = 0;
    uint8_t batteryLevelBars = 0;
    bool batteryVoltageValid = false;
    uint16_t batteryVoltageMillivolts = 0;
    bool batteryCurrentValid = false;
    int16_t batteryCurrentMilliamps = 0;
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
    bool timeReached(uint32_t now, uint32_t target)
    {
        return static_cast<int32_t>(now - target) >= 0;
    }

    bool isConfigVerifyStatus(FightpadBQ27220BatteryAddon::ReadStatus status)
    {
        switch (status) {
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_FAILED:
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_TAPER_FAILED:
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
    batteryLevelBars = 0;
    batteryVoltageValid = false;
    batteryVoltageMillivolts = 0;
    batteryCurrentValid = false;
    batteryCurrentMilliamps = 0;
    batteryFullChargeCapacityValid = false;
    batteryFullChargeCapacityMah = 0;
    batteryConfigAttempted = false;
    batteryConfigApplied = false;
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
    nextPollTimeMs = getMillis() + FIGHTPAD12SLIM_BQ27220_BOOT_DELAY_MS;

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
    batteryVoltageValid = false;
    batteryCurrentValid = false;
    batteryFullChargeCapacityValid = false;

    uint8_t percent = 0;
    if (readStateOfCharge(percent)) {
        batteryPercent = percent;
        batteryLevelBars = percentToBars(percent);
        batteryPercentValid = true;
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

    uint16_t capacityMah = 0;
    if (readFullChargeCapacity(capacityMah)) {
        batteryFullChargeCapacityMah = capacityMah;
        batteryFullChargeCapacityValid = true;
    }

    if (batteryPercentValid && batteryVoltageValid && batteryCurrentValid && batteryFullChargeCapacityValid) {
#if FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
        if (batteryConfigApplied) {
            batteryReadStatus = ReadStatus::OK;
        }
#else
        batteryReadStatus = ReadStatus::OK;
#endif
    }

    nextPollTimeMs = now + FIGHTPAD12SLIM_BQ27220_POLL_INTERVAL_MS;
}

bool FightpadBQ27220BatteryAddon::isBatteryPercentValid()
{
    return batteryPercentValid;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryPercent()
{
    return batteryPercent;
}

uint8_t FightpadBQ27220BatteryAddon::getBatteryLevelBars()
{
    return batteryLevelBars;
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

bool FightpadBQ27220BatteryAddon::isBatteryFullChargeCapacityValid()
{
    return batteryFullChargeCapacityValid;
}

uint16_t FightpadBQ27220BatteryAddon::getBatteryFullChargeCapacityMah()
{
    return batteryFullChargeCapacityMah;
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

    uint16_t cedvConfig = 0;
    if (!readDataMemoryWord(BQ27220_DATA_CEDV_GAUGING_CONFIG, cedvConfig)) {
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
    const bool cedvConfigCurrent =
        (cedvConfig & BQ27220_CEDV_MANAGED_MASK) == targetCedvBits;
    configurationCurrent = !requiresReinitialization && cedvConfigCurrent;
    return true;
#endif
}

bool FightpadBQ27220BatteryAddon::configureBatteryGauge(bool reinitialize)
{
#if !FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
    return true;
#else
    bool enteredConfig = false;
    bool ok = enterFullAccessMode();
    ok = ok && writeControlWord(BQ27220_CONTROL_ENTER_CONFIG_UPDATE);

    if (ok) {
        enteredConfig = waitForConfigUpdateMode(true);
        ok = enteredConfig;
    }

    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_CHARGING_VOLTAGE, FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_CHARGING_VOLTAGE_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_TAPER_CURRENT, FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA, ReadStatus::CONFIG_VERIFY_TAPER_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_TAPER_VOLTAGE, FIGHTPAD12SLIM_BQ27220_TAPER_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_BATTERY_LOW_PERCENT, FIGHTPAD12SLIM_BQ27220_BATTERY_LOW_PERCENT_X100, ReadStatus::CONFIG_VERIFY_FAILED);
    }
    if (ok) {
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
    if (ok) {
        ok = updateDataMemoryWordBits(BQ27220_DATA_SOC_FLAG_CONFIG_A, 0, BQ27220_SOC_FLAG_PRIMARY_TERMINATION_MASK, ReadStatus::CONFIG_VERIFY_FAILED);
    }
    // Restore the FCC baseline only after the gauge RAM has returned to defaults.
    if (ok && reinitialize) {
        ok = writeDataMemoryWord(BQ27220_DATA_FULL_CHARGE_CAPACITY, FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH, ReadStatus::CONFIG_VERIFY_FCC_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_DESIGN_CAPACITY, FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH, ReadStatus::CONFIG_VERIFY_DESIGN_CAPACITY_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_DESIGN_VOLTAGE, FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_DESIGN_VOLTAGE_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_FIXED_EDV0, FIGHTPAD12SLIM_BQ27220_EDV0_MV, ReadStatus::CONFIG_VERIFY_EDV0_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_FIXED_EDV1, FIGHTPAD12SLIM_BQ27220_EDV1_MV, ReadStatus::CONFIG_VERIFY_EDV1_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_FIXED_EDV2, FIGHTPAD12SLIM_BQ27220_EDV2_MV, ReadStatus::CONFIG_VERIFY_EDV2_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_VOLTAGE_0_DOD, FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_VOLTAGE_0_DOD_FAILED);
    }
    if (ok) {
        ok = writeDataMemoryWord(BQ27220_DATA_VOLTAGE_100_DOD, FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV, ReadStatus::CONFIG_VERIFY_VOLTAGE_100_DOD_FAILED);
    }

    bool exitOk = true;
    if (enteredConfig) {
        const uint16_t exitCommand = reinitialize ?
            BQ27220_CONTROL_EXIT_CONFIG_UPDATE_REINIT :
            BQ27220_CONTROL_EXIT_CONFIG_UPDATE;
        exitOk = writeControlWord(exitCommand);
        if (exitOk) {
            exitOk = waitForConfigUpdateMode(false);
        }
    }

    if (!ok || !exitOk) {
        if (!isConfigVerifyStatus(batteryReadStatus) &&
            batteryReadStatus != ReadStatus::CONFIG_FULL_ACCESS_FAILED) {
            batteryReadStatus = ReadStatus::CONFIG_UPDATE_FAILED;
        }
        return false;
    }

    return true;
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
    const uint32_t deadline = getMillis() + 1000;
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

bool FightpadBQ27220BatteryAddon::readDataMemoryWord(uint16_t address, uint16_t& value)
{
    const uint8_t addressBytes[2] = {
        static_cast<uint8_t>(address & 0xFF),
        static_cast<uint8_t>((address >> 8) & 0xFF),
    };
    if (!writeRegisterBytes(BQ27220_COMMAND_DATA_MEMORY_ADDRESS, addressBytes, sizeof(addressBytes))) {
        return false;
    }
    sleep_ms(10);

    uint8_t data[2] = {0, 0};
    if (!readRegisterBytes(BQ27220_COMMAND_BLOCK_DATA, data, sizeof(data))) {
        return false;
    }

    value = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    return true;
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
