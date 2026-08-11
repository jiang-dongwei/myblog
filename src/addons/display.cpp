/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#include "addons/display.h"
#include "addons/fightpad_bq27220_battery.h"
#include "addons/fightpad_esp32_proxy.h"
#include "addons/scrollwheel_menu.h"
#include "GamepadState.h"
#include "enums.h"
#include "storagemanager.h"
#include "pico/stdlib.h"
#include "pico/version.h"

#include "drivermanager.h"
#include "usbdriver.h"
#include "version.h"
#include "config.pb.h"
#include "class/hid/hid.h"

#include <cstdio>
#include <cstring>

bool DisplayAddon::available() {
    const DisplayOptions& options = Storage::getInstance().getDisplayOptions();
    bool result = false;

    // create the gfx interface
    gpDisplay = new GPGFX();
    gpOptions = gpDisplay->getAvailableDisplay(GPGFX_DisplayType::DISPLAY_TYPE_NONE);
    if ( gpOptions.displayType != GPGFX_DisplayType::DISPLAY_TYPE_NONE ) {
        if ( options.enabled ) {
            result = true;
        } else {
            // Power off our display if its available but disabled in config
            gpOptions.size = options.size;
            gpOptions.orientation = options.flip;
            gpOptions.inverted = options.invert;
            gpOptions.font.fontData = GP_Font_Standard;
            gpOptions.font.width = 6;
            gpOptions.font.height = 8;
            gpOptions.contrast = options.contrast;
            gpDisplay->init(gpOptions);
            setDisplayPower(0);
            delete gpDisplay;
            result = false;
        }
    } else { // No display, delete our GPGFX
        delete gpDisplay;
    }
    return result;
}

void DisplayAddon::setup() {
    const DisplayOptions& options = Storage::getInstance().getDisplayOptions();

    // Setup GPGFX Options
    if (gpOptions.displayType != GPGFX_DisplayType::DISPLAY_TYPE_NONE) {
        gpOptions.size = options.size;
        gpOptions.orientation = options.flip;
        gpOptions.inverted = options.invert;
        gpOptions.font.fontData = GP_Font_Standard;
        gpOptions.font.width = 6;
        gpOptions.font.height = 8;
        gpOptions.contrast = options.contrast;
    } else {
        return;
    }

    // Setup GPGFX
    gpDisplay->init(gpOptions);

    displaySaverTimer = options.displaySaverTimeout;
    displaySaverTimeout = displaySaverTimer;
    prevMillis = getMillis();
    lastScrollWheelActivityMs = g_scrollWheelLastActivityMs.load(std::memory_order_acquire);
    configMode = DriverManager::getInstance().isConfigMode();
    turnOffWhenSuspended = options.turnOffWhenSuspended;
    displaySaverMode = options.displaySaverMode;

    prevValues = Storage::getInstance().GetGamepad()->debouncedGpio;

    // set current display mode
    if (!configMode) {
        if (Storage::getInstance().getDisplayOptions().splashMode != static_cast<SplashMode>(SPLASH_MODE_NONE)) {
            currDisplayMode = DisplayMode::SPLASH;
        } else {
            currDisplayMode = DisplayMode::BUTTONS;
        }
    } else {
        currDisplayMode = DisplayMode::CONFIG_INSTRUCTION;
    }
    gpScreen = nullptr;
    updateDisplayScreen();
    setMenuMappings();

    EventManager::getInstance().registerEventHandler(GP_EVENT_PROFILE_CHANGE, GPEVENT_CALLBACK(this->handleProfileChange(event)));
    EventManager::getInstance().registerEventHandler(GP_EVENT_RESTART, GPEVENT_CALLBACK(this->handleSystemRestart(event)));
    EventManager::getInstance().registerEventHandler(GP_EVENT_MENU_NAVIGATE, GPEVENT_CALLBACK(this->handleMenuNavigation(event)));
    EventManager::getInstance().registerEventHandler(GP_EVENT_SYSTEM_ERROR, GPEVENT_CALLBACK(this->handleSystemError(event)));
}

bool DisplayAddon::updateDisplayScreen() {
    if ( gpScreen != nullptr ) {
        gpScreen->shutdown();
        delete gpScreen; // Virtual deconstructor
        gpScreen = nullptr;
    }
    switch(currDisplayMode) {
        case CONFIG_INSTRUCTION:
            gpScreen = new ConfigScreen(gpDisplay);
            break;
        case SPLASH:
            gpScreen = new SplashScreen(gpDisplay);
            break;
        case MAIN_MENU:
            gpScreen = new MainMenuScreen(gpDisplay);
            break;
        case BUTTONS:
            gpScreen = new ButtonLayoutScreen(gpDisplay);
            break;
        case PIN_VIEWER:
            gpScreen = new PinViewerScreen(gpDisplay);
            break;
        case DISPLAY_SAVER:
            gpScreen = new DisplaySaverScreen(gpDisplay);
            break;
        case STATS:
            gpScreen = new StatsScreen(gpDisplay);
            break;
        case SYSTEM_ERROR:
            gpScreen = new SystemErrorScreen(gpDisplay, errorMessage);
            break;
        case RESTART:
            gpScreen = new RestartScreen(gpDisplay, bootMode);
            break;
        default:
            gpScreen = nullptr;
            break;
    };

    if (gpScreen == nullptr )
        return false;

    gpScreen->init();
    prevDisplayMode = currDisplayMode;
    nextDisplayMode = currDisplayMode;
    return true;
}

bool DisplayAddon::isDisplayPowerOff()
{
    Gamepad * gamepad = Storage::getInstance().GetGamepad();
    const uint32_t now = getMillis();

    if (turnOffWhenSuspended && get_usb_suspended()) {
        if (displayIsPowerOn)
            setDisplayPower(0);
        return true;
    }

#if FIGHTPAD12SLIM_OLED_IDLE_SLEEP_ENABLED
    const uint32_t activityMs = g_scrollWheelLastActivityMs.load(std::memory_order_acquire);
    const bool bluetoothWakeActive = bluetoothStatusActivityValid &&
        (now - lastBluetoothStatusActivityMs) < FIGHTPAD12SLIM_OLED_IDLE_SLEEP_TIMEOUT_MS;
    if (activityMs != lastScrollWheelActivityMs) {
        lastScrollWheelActivityMs = activityMs;
        displaySaverTimer = displaySaverTimeout;
        prevMillis = now;
    }

    if (!bluetoothWakeActive &&
        (now - activityMs) >= FIGHTPAD12SLIM_OLED_IDLE_SLEEP_TIMEOUT_MS) {
        setDisplayPower(0);
        prevMillis = now;
        return true;
    }
#endif

    if (!displayIsPowerOn)
        setDisplayPower(1);

    if (!displaySaverTimeout) return false;

    float diffTime = now - prevMillis;
    displaySaverTimer -= diffTime;
    if (!!displaySaverTimeout && (gamepad->state.buttons || gamepad->state.dpad)) {
        displaySaverTimer = displaySaverTimeout;
        setDisplayPower(1);
    } else if (!!displaySaverTimeout && displaySaverTimer <= 0) {
        if (displaySaverMode == DisplaySaverMode::DISPLAY_SAVER_DISPLAY_OFF) {
            setDisplayPower(0);
        } else {
            if (currDisplayMode != DISPLAY_SAVER) {
                currDisplayMode = DISPLAY_SAVER;
                updateDisplayScreen();
            }
        }
    }

    prevMillis = now;

    return ((!!displaySaverTimeout && displaySaverTimer <= 0) && (displaySaverMode == DisplaySaverMode::DISPLAY_SAVER_DISPLAY_OFF));
}

void DisplayAddon::setDisplayPower(uint8_t status)
{
    if (displayIsPowerOn != status) {
        displayIsPowerOn = status;
        gpDisplay->getDriver()->setPower(status);
    }
}

void DisplayAddon::setMenuMappings()
{
    mapMenuToggle = new GamepadButtonMapping(0);
    mapMenuSelect = new GamepadButtonMapping(0);
    GpioMappingInfo* pinMappings = Storage::getInstance().getProfilePinMappings();
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
        switch (pinMappings[pin].action) {
            case GpioAction::MENU_NAVIGATION_TOGGLE: mapMenuToggle->pinMask |= 1 << pin; break;
            case GpioAction::MENU_NAVIGATION_SELECT: mapMenuSelect->pinMask |= 1 << pin; break;
            default:    break;
        }
    }
}

void DisplayAddon::process() {
    if (gpDisplay->getDriver() == nullptr) {
        return;
    }

    const uint32_t now = getMillis();
    FightpadESP32BluetoothStatusEvent bluetoothEvent = {};
    const bool hasBluetoothEvent = getFightpadESP32BluetoothStatusEvent(bluetoothEvent);
    if (hasBluetoothEvent && bluetoothEvent.sequence != lastBluetoothStatusSequence) {
        lastBluetoothStatusSequence = bluetoothEvent.sequence;
        lastBluetoothStatusActivityMs = now;
        bluetoothStatusActivityValid = true;
        displaySaverTimer = displaySaverTimeout;
        prevMillis = now;
    }

    const bool bluetoothOverlayActive = hasBluetoothEvent &&
        isFightpadESP32BluetoothStatusEventActive(bluetoothEvent, now);
    const bool splashScreenActive = !configMode &&
        currDisplayMode == DisplayMode::SPLASH;

    // A Bluetooth transition is a temporary overlay, not a display-mode or
    // scrollwheel-menu transition.  It can wake a sleeping OLED, and removing
    // the overlay naturally reveals the exact page that was active before it.
    // Keep the board splash uninterrupted; status reception and timeout still
    // advance in the background, so only a status that remains active after
    // the splash may become visible.
    if (!bluetoothOverlayActive && !configMode && isDisplayPowerOff()) {
        return;
    }

    if (bluetoothOverlayActive && !splashScreenActive) {
        setDisplayPower(1);
        gpDisplay->clearScreen();
        gpDisplay->drawText(2, 2, "Bluetooth Status");

        switch (bluetoothEvent.status) {
        case FightpadESP32BluetoothStatus::Connecting:
            gpDisplay->drawText(4, 4, "Connecting...");
            break;
        case FightpadESP32BluetoothStatus::Connected:
            gpDisplay->drawText(6, 4, "Connected");
            break;
        case FightpadESP32BluetoothStatus::Pairing:
            gpDisplay->drawText(5, 4, "Pairing...");
            break;
        case FightpadESP32BluetoothStatus::Disconnected:
        default:
            gpDisplay->drawText(4, 4, "Disconnected");
            break;
        }

        gpDisplay->render();
        return;
    }

    // ── Scrollwheel menu takeover ────────────────────────────────────
    if (g_scrollWheelMenuActive) {
        drawScrollWheelMenu();
        return;
    }

    // Core0 requested a new display mode
    if (nextDisplayMode != currDisplayMode ) {
        currDisplayMode = nextDisplayMode;
        updateDisplayScreen();
    }

    int8_t screenReturn = gpScreen->update();
    gpScreen->draw();

    if (!configMode && screenReturn < 0) {
        Mask_t values = Storage::getInstance().GetGamepad()->debouncedGpio;
        if (prevValues != values) {
            if ((values & mapMenuToggle->pinMask) || (values & mapMenuSelect->pinMask)) {
                if (currDisplayMode != DisplayMode::MAIN_MENU) {
                    screenReturn = DisplayMode::MAIN_MENU;
                }
            }
            prevValues = values;
        }
    }

    // -1 = we do not change state
    if (screenReturn >= 0) {
        // Screen wants to change to something else
        if (screenReturn != currDisplayMode) {
            currDisplayMode = (DisplayMode)screenReturn;
            updateDisplayScreen();
        }
    }
}

// ── Scrollwheel menu renderer ────────────────────────────────────────────
// WARNING: GPGFX_TinySSD1306::drawText(x, y, ...) treats (x, y) as
// CHARACTER coordinates, NOT pixels.  Font is 6×8 → each unit of x is
// 6 pixels, each unit of y is 8 pixels.  Screen = 128×64 px = 21 cols × 8 rows.

static constexpr uint8_t SW_MAX_ROWS = 8;   // 64 px / 8 px per char row

static const char* batteryConfigResultLabel(FightpadBQ27220BatteryAddon::ConfigCheckResult result) {
    switch (result) {
        case FightpadBQ27220BatteryAddon::ConfigCheckResult::OK:    return "OK";
        case FightpadBQ27220BatteryAddon::ConfigCheckResult::FIXED: return "FIX";
        case FightpadBQ27220BatteryAddon::ConfigCheckResult::BAD:   return "BAD";
        case FightpadBQ27220BatteryAddon::ConfigCheckResult::NOT_CHECKED:
        default:                                                    return "WAIT";
    }
}

static void drawBatteryWordCheck(GPGFX* display, uint8_t row, const char* label,
    const FightpadBQ27220BatteryAddon::ConfigWordSnapshot& word) {
    char line[22] = {};
    if (!word.valid) {
        std::snprintf(line, sizeof(line), "%s:---- BAD", label);
    } else if (word.result == FightpadBQ27220BatteryAddon::ConfigCheckResult::FIXED) {
        std::snprintf(line, sizeof(line), "%s:%u>%u FIX", label,
            static_cast<unsigned int>(word.before), static_cast<unsigned int>(word.after));
    } else if (word.result == FightpadBQ27220BatteryAddon::ConfigCheckResult::BAD && word.before != word.target) {
        std::snprintf(line, sizeof(line), "%s:%u/%u BAD", label,
            static_cast<unsigned int>(word.before), static_cast<unsigned int>(word.target));
    } else {
        std::snprintf(line, sizeof(line), "%s:%u %s", label,
            static_cast<unsigned int>(word.after),
            batteryConfigResultLabel(word.result));
    }
    display->drawText(0, row, line);
}

static void drawBatteryHexWordCheck(GPGFX* display, uint8_t row, const char* label,
    const FightpadBQ27220BatteryAddon::ConfigWordSnapshot& word) {
    char line[22] = {};
    if (!word.valid) {
        std::snprintf(line, sizeof(line), "%s:---- BAD", label);
    } else if (word.result == FightpadBQ27220BatteryAddon::ConfigCheckResult::FIXED) {
        std::snprintf(line, sizeof(line), "%s:%04X>%04X FIX", label,
            static_cast<unsigned int>(word.before), static_cast<unsigned int>(word.after));
    } else if (word.result == FightpadBQ27220BatteryAddon::ConfigCheckResult::BAD && word.before != word.target) {
        std::snprintf(line, sizeof(line), "%s:%04X/%04X BAD", label,
            static_cast<unsigned int>(word.before), static_cast<unsigned int>(word.target));
    } else {
        std::snprintf(line, sizeof(line), "%s:%04X %s", label,
            static_cast<unsigned int>(word.after),
            batteryConfigResultLabel(word.result));
    }
    display->drawText(0, row, line);
}

static void drawBatteryRuntimePage(GPGFX* display) {
    char line[22] = {};
    FightpadBQ27220BatteryAddon::ConfigurationSnapshot config = {};
    const bool configValid = FightpadBQ27220BatteryAddon::getConfigurationSnapshot(config);

    display->drawText(0, 0, "Battery Info 1/4");
    if (FightpadBQ27220BatteryAddon::isBatteryPercentValid() &&
        FightpadBQ27220BatteryAddon::isBatteryVoltageValid()) {
        std::snprintf(line, sizeof(line), "SOC:%03u%% V:%u",
            static_cast<unsigned int>(FightpadBQ27220BatteryAddon::getBatteryPercent()),
            static_cast<unsigned int>(FightpadBQ27220BatteryAddon::getBatteryVoltageMillivolts()));
    } else {
        std::snprintf(line, sizeof(line), "SOC:---%% V:----");
    }
    display->drawText(0, 1, line);

    if (FightpadBQ27220BatteryAddon::isBatteryCurrentValid()) {
        std::snprintf(line, sizeof(line), "I:%+dmA",
            static_cast<int>(FightpadBQ27220BatteryAddon::getBatteryCurrentMilliamps()));
    } else {
        std::snprintf(line, sizeof(line), "I:-----mA");
    }
    display->drawText(0, 2, line);

    if (FightpadBQ27220BatteryAddon::isBatteryRemainingCapacityValid()) {
        std::snprintf(line, sizeof(line), "RM:%umAh",
            static_cast<unsigned int>(FightpadBQ27220BatteryAddon::getBatteryRemainingCapacityMah()));
    } else {
        std::snprintf(line, sizeof(line), "RM:----mAh");
    }
    display->drawText(0, 3, line);

    if (FightpadBQ27220BatteryAddon::isBatteryFullChargeCapacityValid()) {
        std::snprintf(line, sizeof(line), "FCC:%umAh",
            static_cast<unsigned int>(FightpadBQ27220BatteryAddon::getBatteryFullChargeCapacityMah()));
    } else {
        std::snprintf(line, sizeof(line), "FCC:----mAh");
    }
    display->drawText(0, 4, line);

    std::snprintf(line, sizeof(line), "CFG:%s",
        configValid ? batteryConfigResultLabel(config.overallResult) : "WAIT");
    display->drawText(0, 5, line);
    display->drawText(0, 6,
        FightpadBQ27220BatteryAddon::getReadStatus() == FightpadBQ27220BatteryAddon::ReadStatus::OK ?
        "READ:OK" : "READ:ERR");
    display->drawText(0, 7, "< page >");
}

static void drawBatteryConfigPage(GPGFX* display) {
    char line[22] = {};
    FightpadBQ27220BatteryAddon::ConfigurationSnapshot config = {};
    display->drawText(0, 0, "BQ CONFIG 2/4");

    if (!FightpadBQ27220BatteryAddon::getConfigurationSnapshot(config)) {
        display->drawText(0, 2, "Config read pending");
        display->drawText(0, 7, "< page >");
        return;
    }

    if (config.batteryIdValid) {
        std::snprintf(line, sizeof(line), "ID:%02X RAM:%s",
            static_cast<unsigned int>(config.batteryId),
            config.ramReinitializationRequired ? "INIT" : "KEEP");
    } else {
        std::snprintf(line, sizeof(line), "ID:-- RAM:?");
    }
    display->drawText(0, 1, line);
    drawBatteryWordCheck(display, 2, "LOW", config.batteryLow);
    drawBatteryWordCheck(display, 3, "E0", config.edv0);
    drawBatteryWordCheck(display, 4, "E1", config.edv1);
    drawBatteryWordCheck(display, 5, "E2", config.edv2);
    std::snprintf(line, sizeof(line), "CFG:%s", batteryConfigResultLabel(config.overallResult));
    display->drawText(0, 6, line);
    display->drawText(0, 7, "< page >");
}

static void drawBatteryCalibrationPage(GPGFX* display) {
    char line[22] = {};
    FightpadBQ27220BatteryAddon::ConfigurationSnapshot config = {};
    display->drawText(0, 0, "BQ CAL 3/4");

    if (!FightpadBQ27220BatteryAddon::getConfigurationSnapshot(config)) {
        display->drawText(0, 2, "Cal read pending");
        display->drawText(0, 7, "< page >");
        return;
    }

    std::snprintf(line, sizeof(line), "CCO:%+d BO:%+d",
        static_cast<int>(config.ccOffsetValid ? config.ccOffset : 0),
        static_cast<int>(config.boardOffsetValid ? config.boardOffset : 0));
    display->drawText(0, 1, line);

    if (config.ccGainValid) {
        std::snprintf(line, sizeof(line), "GAIN:%u.%06u",
            static_cast<unsigned int>(config.ccGainMicro / 1000000u),
            static_cast<unsigned int>(config.ccGainMicro % 1000000u));
    } else {
        std::snprintf(line, sizeof(line), "GAIN:BAD");
    }
    display->drawText(0, 2, line);
    std::snprintf(line, sizeof(line), "G:%08lX", static_cast<unsigned long>(config.ccGainRaw));
    display->drawText(0, 3, line);

    if (config.ccDeltaValid) {
        std::snprintf(line, sizeof(line), "DELTA:%lu", static_cast<unsigned long>(config.ccDeltaRounded));
    } else {
        std::snprintf(line, sizeof(line), "DELTA:BAD");
    }
    display->drawText(0, 4, line);
    std::snprintf(line, sizeof(line), "D:%08lX", static_cast<unsigned long>(config.ccDeltaRaw));
    display->drawText(0, 5, line);
    display->drawText(0, 6, config.currentCalibrated ? "CAL:OK" : "CAL:UNCAL");
    std::snprintf(line, sizeof(line), "R:%umR I:%umA",
        static_cast<unsigned int>(FIGHTPAD12SLIM_BQ27220_SENSE_RESISTOR_MILLIOHMS),
        static_cast<unsigned int>(FIGHTPAD12SLIM_BQ27220_CALIBRATION_CURRENT_MA));
    display->drawText(0, 7, line);
}

static void drawBatteryChargePage(GPGFX* display) {
    char line[22] = {};
    FightpadBQ27220BatteryAddon::ConfigurationSnapshot config = {};
    const bool configValid = FightpadBQ27220BatteryAddon::getConfigurationSnapshot(config);

    display->drawText(0, 0, "BQ CHARGE 4/4");
    if (configValid) {
        drawBatteryWordCheck(display, 1, "CV", config.chargingVoltage);
        drawBatteryWordCheck(display, 2, "TC", config.taperCurrent);
        drawBatteryWordCheck(display, 3, "TV", config.taperVoltage);
        drawBatteryHexWordCheck(display, 4, "SF", config.socFlagConfigA);
    } else {
        display->drawText(0, 1, "CV:---- WAIT");
        display->drawText(0, 2, "TC:---- WAIT");
        display->drawText(0, 3, "TV:---- WAIT");
        display->drawText(0, 4, "SF:---- WAIT");
    }

    if (FightpadBQ27220BatteryAddon::isBatteryCurrentValid() &&
        FightpadBQ27220BatteryAddon::isBatteryAverageCurrentValid()) {
        std::snprintf(line, sizeof(line), "I:%+d AVG:%+d",
            static_cast<int>(FightpadBQ27220BatteryAddon::getBatteryCurrentMilliamps()),
            static_cast<int>(FightpadBQ27220BatteryAddon::getBatteryAverageCurrentMilliamps()));
    } else if (FightpadBQ27220BatteryAddon::isBatteryAverageCurrentValid()) {
        std::snprintf(line, sizeof(line), "I:----- AVG:%+d",
            static_cast<int>(FightpadBQ27220BatteryAddon::getBatteryAverageCurrentMilliamps()));
    } else if (FightpadBQ27220BatteryAddon::isBatteryCurrentValid()) {
        std::snprintf(line, sizeof(line), "I:%+d AVG:-----",
            static_cast<int>(FightpadBQ27220BatteryAddon::getBatteryCurrentMilliamps()));
    } else {
        std::snprintf(line, sizeof(line), "I:----- AVG:-----");
    }
    display->drawText(0, 5, line);

    if (FightpadBQ27220BatteryAddon::isBatteryStatusValid()) {
        std::snprintf(line, sizeof(line), "FC:%u TCA:%u",
            FightpadBQ27220BatteryAddon::isBatteryFullChargeDetected() ? 1u : 0u,
            FightpadBQ27220BatteryAddon::isBatteryTerminateChargeAlarm() ? 1u : 0u);
    } else {
        std::snprintf(line, sizeof(line), "FC:? TCA:?");
    }
    display->drawText(0, 6, line);
    display->drawText(0, 7, "< page >");
}

void DisplayAddon::drawScrollWheelMenu() {
    // Snapshot volatile state once
    ScrollWheelMenuState snap;
    snap.active       = g_menuState.active;
    snap.level        = g_menuState.level;
    snap.index        = g_menuState.index;
    snap.scrollOffset = g_menuState.scrollOffset;
    snap.infoSource   = g_menuState.infoSource;

    if (!snap.active) return;

    gpDisplay->clearScreen();
    SWMenuLevel level = static_cast<SWMenuLevel>(snap.level);

    // ── INFO page ──────────────────────────────────────────────────
    if (level == SWMenuLevel::INFO) {
        if (snap.infoSource == 0) {
            if (snap.index == 0) {
                char infoLine[22] = {};
                gpDisplay->drawText(0, 0, "RP2350B Firmware");

                std::snprintf(infoLine, sizeof(infoLine), "SDK: %.16s", PICO_SDK_VERSION_STRING);
                gpDisplay->drawText(0, 1, infoLine);

                std::snprintf(infoLine, sizeof(infoLine), "Plat: %.15s", GP2040PLATFORM);
                gpDisplay->drawText(0, 2, infoLine);

                std::snprintf(infoLine, sizeof(infoLine), "Board: %.14s", GP2040_BOARDCONFIG);
                gpDisplay->drawText(0, 3, infoLine);

                gpDisplay->drawText(0, 4, "CPU: Cortex-M33");
                gpDisplay->drawText(0, 7, "Back: press");
                gpDisplay->render();
                return;
            } else {
                FightpadESP32FirmwareInfo firmwareInfo = {};
                char infoLine[22] = {};
                gpDisplay->drawText(0, 0, "ESP32C6 Firmware");

                if (getFightpadESP32FirmwareInfo(firmwareInfo)) {
                    std::snprintf(infoLine, sizeof(infoLine), "SDK: %.16s", firmwareInfo.sdk);
                    gpDisplay->drawText(0, 1, infoLine);

                    std::snprintf(infoLine, sizeof(infoLine), "Plat: %.15s", firmwareInfo.platform);
                    gpDisplay->drawText(0, 2, infoLine);

                    gpDisplay->drawText(0, 3, "Board:");
                    std::snprintf(infoLine, sizeof(infoLine), "%.21s", firmwareInfo.board);
                    gpDisplay->drawText(0, 4, infoLine);

                    if (std::strlen(firmwareInfo.board) > 21) {
                        std::snprintf(infoLine, sizeof(infoLine), "%.21s", firmwareInfo.board + 21);
                        gpDisplay->drawText(0, 5, infoLine);
                    }

                    std::snprintf(infoLine, sizeof(infoLine), "CPU: %.16s", firmwareInfo.cpu);
                    gpDisplay->drawText(0, 6, infoLine);
                } else {
                    gpDisplay->drawText(0, 3, "Coming to soon");
                }

                gpDisplay->drawText(0, 7, "Back: press");
                gpDisplay->render();
                return;
            }
        } else {
            gpDisplay->drawText(0, 0, "RGB Color:");
            if (snap.index < kMenuColorsCount)
                gpDisplay->drawText(0, 1, kMenuColors[snap.index].label);
        }
        gpDisplay->drawText(0, 3, "Coming soon");
        gpDisplay->drawText(0, 7, "Back: press");
        gpDisplay->render();
        return;
    }

    if (level == SWMenuLevel::BATTERY_INFO) {
        switch (snap.index % SW_BATTERY_PAGE_COUNT) {
            case 0: drawBatteryRuntimePage(gpDisplay); break;
            case 1: drawBatteryConfigPage(gpDisplay); break;
            case 2: drawBatteryCalibrationPage(gpDisplay); break;
            case 3: drawBatteryChargePage(gpDisplay); break;
        }
        gpDisplay->render();
        return;
    }

    // ── Normal list menu ───────────────────────────────────────────
    const SWMenuItem* table = nullptr;
    uint8_t count = 0;
    switch (level) {
        case SWMenuLevel::MAIN:
            table = kMenuMain; count = kMenuMainCount; break;
        case SWMenuLevel::RGB_SUB:
            table = kMenuRgbSub; count = kMenuRgbSubCount; break;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_EFFECT:
        case SWMenuLevel::COLOR_EFFECT_BREATH:
            table = kMenuColors; count = kMenuColorsCount; break;
        case SWMenuLevel::LIGHT_EFFECT:
            table = kMenuLightEffects; count = kMenuLightEffectsCount; break;
        case SWMenuLevel::BRIGHTNESS:
            table = kMenuBrightness; count = kMenuBrightnessCount; break;
        case SWMenuLevel::CONTROLLER_TYPE:
            table = kMenuControllerTypes; count = kMenuControllerTypesCount; break;
        case SWMenuLevel::BATTERY_INFO:
            break;
        default: break;
    }
    if (table == nullptr || count == 0) {
        gpDisplay->drawText(0, 0, "No data");
        gpDisplay->render();
        return;
    }

    // Visible window in character rows
    uint8_t off = snap.scrollOffset;
    if (snap.index < off) off = snap.index;
    if (snap.index >= off + SW_MAX_ROWS) off = snap.index - SW_MAX_ROWS + 1;
    if (off + SW_MAX_ROWS > count && count > SW_MAX_ROWS)
        off = count - SW_MAX_ROWS;

    // Determine the currently-active value for this level so we can
    // mark it with "*" in the rightmost column.
    // 0xFF = nothing active / never set.
    uint8_t activeVal = 0xFF;
    switch (level) {
        case SWMenuLevel::COLOR:               activeVal = g_menuRgbButton;       break;
        case SWMenuLevel::COLOR_EFFECT:
        case SWMenuLevel::COLOR_EFFECT_BREATH: activeVal = g_menuRgbEffectColor;  break;
        case SWMenuLevel::LIGHT_EFFECT:        activeVal = g_menuLightEffect;     break;
        case SWMenuLevel::BRIGHTNESS:          activeVal = g_menuBrightnessLevel; break;
        case SWMenuLevel::CONTROLLER_TYPE:
            activeVal = static_cast<uint8_t>(Storage::getInstance().getGamepadOptions().inputMode);
            break;
        default: break;
    }

    for (uint8_t row = 0; row < SW_MAX_ROWS; row++) {
        uint8_t idx = off + row;
        if (idx >= count) break;

        bool isSelected = (idx == snap.index);
        bool isActive   = (activeVal != 0xFF && activeVal == table[idx].targetIndex);

        // ">" prefix marks cursor selection (1 char column)
        if (isSelected)
            gpDisplay->drawText(0, row, ">");

        // Label with 2-char indent so non-selected items align
        gpDisplay->drawText(isSelected ? 1 : 2, row, table[idx].label);

        // "*" in rightmost column marks the currently-active setting
        if (isActive)
            gpDisplay->drawText(20, row, "*");
    }

    gpDisplay->render();
}

const DisplayOptions& DisplayAddon::getDisplayOptions() {
    return Storage::getInstance().getDisplayOptions();
}

void DisplayAddon::handleProfileChange(GPEvent* e)
{
    delete mapMenuToggle;
    delete mapMenuSelect;
    mapMenuToggle = nullptr;
    mapMenuSelect = nullptr;
    setMenuMappings();
}

void DisplayAddon::handleSystemRestart(GPEvent* e) {
    nextDisplayMode = DisplayMode::RESTART;
    bootMode = (uint32_t)((GPRestartEvent*)e)->bootMode;
}

void DisplayAddon::handleMenuNavigation(GPEvent* e) {
    // Swap between main menu and buttons if we press toggle
    if (((GPMenuNavigateEvent*)e)->menuAction == GpioAction::MENU_NAVIGATION_TOGGLE) {
        if (currDisplayMode == BUTTONS) {
            nextDisplayMode = MAIN_MENU;
        } else if (currDisplayMode == MAIN_MENU) {
            nextDisplayMode = BUTTONS;
        }
    } else if (currDisplayMode == MAIN_MENU) {
        ((MainMenuScreen*)gpScreen)->updateEventMenuNavigation(((GPMenuNavigateEvent*)e)->menuAction);
    }
}

void DisplayAddon::handleSystemError(GPEvent* e) {
    currDisplayMode = SYSTEM_ERROR;
    errorMessage = ((GPSystemErrorEvent*) e)->errorMessage;
}
