#ifndef _FIGHTPAD_BQ27220_BATTERY_H_
#define _FIGHTPAD_BQ27220_BATTERY_H_

#include "BoardConfig.h"
#include "gpaddon.h"

#include <stdint.h>

#ifndef FIGHTPAD12SLIM_BQ27220_ENABLED
#define FIGHTPAD12SLIM_BQ27220_ENABLED 0
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_SCL_PIN
#define FIGHTPAD12SLIM_BQ27220_SCL_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_SDA_PIN
#define FIGHTPAD12SLIM_BQ27220_SDA_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_GPOUT_PIN
#define FIGHTPAD12SLIM_BQ27220_GPOUT_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_BOOT_DELAY_MS
#define FIGHTPAD12SLIM_BQ27220_BOOT_DELAY_MS 2000
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_POLL_INTERVAL_MS
#define FIGHTPAD12SLIM_BQ27220_POLL_INTERVAL_MS 2000
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_I2C_DELAY_US
#define FIGHTPAD12SLIM_BQ27220_I2C_DELAY_US 5
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_DIAGNOSTIC_DISPLAY
#define FIGHTPAD12SLIM_BQ27220_DIAGNOSTIC_DISPLAY 0
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_DATA_MEMORY_DIAGNOSTIC_DISPLAY
#define FIGHTPAD12SLIM_BQ27220_DATA_MEMORY_DIAGNOSTIC_DISPLAY 0
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM
#define FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM 0
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH
#define FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH 650
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_DESIGN_ENERGY_MWH
#define FIGHTPAD12SLIM_BQ27220_DESIGN_ENERGY_MWH 2405
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV
#define FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV 3700
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA
#define FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA 25
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV
#define FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV 2750
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV
#define FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV 4200
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_EDV0_MV
#define FIGHTPAD12SLIM_BQ27220_EDV0_MV 3000
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_EDV1_MV
#define FIGHTPAD12SLIM_BQ27220_EDV1_MV 3300
#endif

#ifndef FIGHTPAD12SLIM_BQ27220_EDV2_MV
#define FIGHTPAD12SLIM_BQ27220_EDV2_MV 3500
#endif

#define FightpadBQ27220BatteryName "FightpadBQ27220Battery"

class FightpadBQ27220BatteryAddon : public GPAddon {
public:
    enum class ReadStatus : uint8_t {
        NOT_STARTED = 0,
        OK,
        BUS_TIMEOUT,
        ADDRESS_WRITE_NACK,
        COMMAND_NACK,
        ADDRESS_READ_NACK,
        VALUE_OUT_OF_RANGE,
        CONFIG_UPDATE_FAILED,
        CONFIG_FULL_ACCESS_FAILED,
        CONFIG_VERIFY_FAILED,
        CONFIG_VERIFY_TAPER_FAILED,
        CONFIG_VERIFY_FCC_FAILED,
        CONFIG_VERIFY_DESIGN_CAPACITY_FAILED,
        CONFIG_VERIFY_DESIGN_VOLTAGE_FAILED,
        CONFIG_VERIFY_CHARGING_VOLTAGE_FAILED,
        CONFIG_VERIFY_EDV0_FAILED,
        CONFIG_VERIFY_EDV1_FAILED,
        CONFIG_VERIFY_EDV2_FAILED,
        CONFIG_VERIFY_VOLTAGE_0_DOD_FAILED,
        CONFIG_VERIFY_VOLTAGE_100_DOD_FAILED,
    };

    virtual bool available();
    virtual void setup();
    virtual void preprocess() {}
    virtual void process();
    virtual void postprocess(bool sent) {}
    virtual void reinit() {}
    virtual std::string name() { return FightpadBQ27220BatteryName; }

    static bool isBatteryPercentValid();
    static uint8_t getBatteryPercent();
    static uint8_t getBatteryLevelBars();
    static bool isBatteryVoltageValid();
    static uint16_t getBatteryVoltageMillivolts();
    static bool isBatteryCurrentValid();
    static int16_t getBatteryCurrentMilliamps();
    static bool isBatteryFullChargeCapacityValid();
    static uint16_t getBatteryFullChargeCapacityMah();
    static ReadStatus getReadStatus();
    static bool isBatterySecurityStatusValid();
    static char getBatterySecurityStatusCode();
    static bool isBatteryDataMemoryDebugValid();
    static uint16_t getBatteryDataMemoryDebugAddress();
    static uint16_t getBatteryDataMemoryDebugOldValue();
    static uint16_t getBatteryDataMemoryDebugTargetValue();
    static uint16_t getBatteryDataMemoryDebugVerifyValue();
    static uint8_t getBatteryDataMemoryDebugOldChecksum();
    static uint8_t getBatteryDataMemoryDebugNewChecksum();
    static uint8_t getBatteryDataMemoryDebugLength();

private:
    void configurePins();
    bool configureBatteryGauge();
    bool enterFullAccessMode();
    bool sendUnsealKeys(uint16_t key1, uint16_t key2);
    bool sendFullAccessKeys();
    bool sendFullAccessKeysViaManufacturerAccess();
    bool waitForFullAccessMode();
    bool waitForConfigUpdateMode(bool enabled);
    bool readOperationStatus(uint8_t& statusLow, uint8_t& statusHigh);
    bool writeControlWord(uint16_t command);
    bool writeManufacturerAccessWord(uint16_t command);
    bool writeDataMemoryWord(uint16_t address, uint16_t value, ReadStatus verifyFailureStatus);
    bool readRegisterBytes(uint8_t command, uint8_t* data, uint8_t length);
    bool writeRegisterBytes(uint8_t command, const uint8_t* data, uint8_t length);
    bool readStateOfCharge(uint8_t& percent);
    bool readVoltage(uint16_t& millivolts);
    bool readCurrent(int16_t& milliamps);
    bool readFullChargeCapacity(uint16_t& capacityMah);
    uint8_t percentToBars(uint8_t percent) const;
    bool readWord(uint8_t command, uint16_t& value);
    bool writeByte(uint8_t value, ReadStatus nackStatus);
    bool readByte(uint8_t& value, bool ack);
    void startCondition();
    void stopCondition();
    bool setSclHigh();
    void driveLow(int pin) const;
    void releaseHigh(int pin) const;
    void delayHalfPeriod() const;

    bool pinsConfigured = false;
    bool batteryConfigAttempted = false;
    bool batteryConfigApplied = false;
    uint32_t nextPollTimeMs = 0;
};

#endif