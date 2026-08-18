#ifndef _FIGHTPAD_BLE_PROFILE_H_
#define _FIGHTPAD_BLE_PROFILE_H_

#include <cstdint>

enum class FightpadBluetoothProfile : uint8_t {
    Generic = 0,
    Xbox = 1,
    Keyboard = 2,
    // UART Profile value 3 is retained for compatibility; it now selects the PS4-compatible BLE Profile.
    PS5PC = 3,
    Switch = 4,
};

enum class FightpadBluetoothProfileAckResult : uint8_t {
    ActiveUnchanged = 0,
    Restarting = 1,
    ApplyingAtBoot = 2,
    InvalidFallback = 3,
    UnsupportedVersion = 4,
    InternalError = 5,
};

static constexpr uint8_t FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION = 1;
static constexpr uint8_t FIGHTPAD_BLE_PROFILE_FLAG_APPLY_NOW = 0x01;
static constexpr uint8_t FIGHTPAD_BLE_PROFILE_FLAG_FORCE_REPAIR = 0x02;

static constexpr bool isValidFightpadBluetoothProfile(uint8_t value)
{
    return value <= static_cast<uint8_t>(FightpadBluetoothProfile::Switch);
}

static constexpr FightpadBluetoothProfile normalizeFightpadBluetoothProfile(uint32_t value)
{
    return isValidFightpadBluetoothProfile(static_cast<uint8_t>(value)) && value <= 0xFFu
        ? static_cast<FightpadBluetoothProfile>(value)
        : FightpadBluetoothProfile::Generic;
}

static inline const char* getFightpadBluetoothProfileLabel(FightpadBluetoothProfile profile)
{
    switch (profile) {
    case FightpadBluetoothProfile::Xbox:     return "Xbox";
    case FightpadBluetoothProfile::Keyboard: return "Keyboard";
    case FightpadBluetoothProfile::PS5PC:    return "PlayStation";
    case FightpadBluetoothProfile::Switch:   return "Switch";
    case FightpadBluetoothProfile::Generic:
    default:                                 return "Standard Gamepad";
    }
}

#endif
