#include "ButtonLayoutScreen.h"
#include "addons/fightpad_bq27220_battery.h"
#include "addons/fightpad_esp32_proxy.h"
#include "buttonlayouts.h"
#include "drivermanager.h"
#include "drivers/ps4/PS4Driver.h"
#include "drivers/xbone/XBOneDriver.h"
#include "drivers/xinput/XInputDriver.h"
#include "drivers/p5general/P5GeneralDriver.h"

namespace
{
    static constexpr uint8_t BATTERY_STATUS_COLUMN = 12;
    static constexpr uint8_t BATTERY_ICON_X = 99;
    static constexpr uint8_t BATTERY_ICON_Y = 0;
    static constexpr uint8_t BATTERY_ICON_BODY_WIDTH = 27;
    static constexpr uint8_t BATTERY_ICON_BODY_HEIGHT = 9;
    static constexpr uint8_t BATTERY_ICON_CAP_WIDTH = 2;
    static constexpr uint8_t BATTERY_ICON_CAP_HEIGHT = 4;
    static constexpr uint8_t BATTERY_CELL_WIDTH = 5;
    static constexpr uint8_t BATTERY_CELL_HEIGHT = 5;
    static constexpr uint8_t BATTERY_CELL_GAP = 1;
    static constexpr uint8_t BATTERY_PERCENT_TEXT_COLUMN = 12;
    static constexpr uint8_t BATTERY_PERCENT_TEXT_ROW = 0;
    static constexpr uint8_t BATTERY_PERCENT_PIXEL_X = BATTERY_PERCENT_TEXT_COLUMN * 6;
    static constexpr uint8_t BATTERY_PERCENT_PIXEL_Y = BATTERY_PERCENT_TEXT_ROW * 8;
    static constexpr uint8_t BATTERY_PERCENT_PIXEL_WIDTH = 26;
    static constexpr uint8_t BATTERY_PERCENT_PIXEL_HEIGHT = 8;

    const char* getActiveBluetoothControllerLabel(FightpadBluetoothProfile profile)
    {
        switch (profile) {
            case FightpadBluetoothProfile::Xbox:     return "XINPUT";
            case FightpadBluetoothProfile::PS5PC:    return "PS4";
            case FightpadBluetoothProfile::Switch:   return "SWITCH";
            case FightpadBluetoothProfile::Keyboard: return "HID-KB";
            case FightpadBluetoothProfile::Generic:
            default:                                 return "USBHID";
        }
    }

    InputMode getActiveControllerDisplayMode(InputMode usbInputMode)
    {
        FightpadBluetoothProfile bluetoothProfile;
        if (!getFightpadESP32ActiveBluetoothProfile(bluetoothProfile)) {
            return usbInputMode;
        }

        switch (bluetoothProfile) {
            case FightpadBluetoothProfile::Xbox:     return INPUT_MODE_XINPUT;
            case FightpadBluetoothProfile::PS5PC:    return INPUT_MODE_PS4;
            case FightpadBluetoothProfile::Switch:   return INPUT_MODE_SWITCH;
            case FightpadBluetoothProfile::Keyboard: return INPUT_MODE_KEYBOARD;
            case FightpadBluetoothProfile::Generic:
            default:                                 return INPUT_MODE_GENERIC;
        }
    }

    std::string getFightpadBatteryDiagnosticText()
    {
#if FIGHTPAD12SLIM_BQ27220_ENABLED
        if (FightpadBQ27220BatteryAddon::isBatteryPercentValid() &&
            FightpadBQ27220BatteryAddon::isBatteryVoltageValid() &&
            FightpadBQ27220BatteryAddon::isBatteryCurrentValid() &&
            FightpadBQ27220BatteryAddon::isBatteryFullChargeCapacityValid() &&
            FightpadBQ27220BatteryAddon::getReadStatus() == FightpadBQ27220BatteryAddon::ReadStatus::OK) {
            return "";
        }

        switch (FightpadBQ27220BatteryAddon::getReadStatus()) {
            case FightpadBQ27220BatteryAddon::ReadStatus::NOT_STARTED:
                return "B:WAIT";
            case FightpadBQ27220BatteryAddon::ReadStatus::BUS_TIMEOUT:
                return "B:BUS!";
            case FightpadBQ27220BatteryAddon::ReadStatus::ADDRESS_WRITE_NACK:
                return "B:NO55";
            case FightpadBQ27220BatteryAddon::ReadStatus::COMMAND_NACK:
                return "B:CMD!";
            case FightpadBQ27220BatteryAddon::ReadStatus::ADDRESS_READ_NACK:
                return "B:RD!";
            case FightpadBQ27220BatteryAddon::ReadStatus::VALUE_OUT_OF_RANGE:
                return "B:VAL!";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_UPDATE_FAILED:
                return "B:CFG!";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_FULL_ACCESS_FAILED:
                return "B:SEC!";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_FAILED:
                return "B:VER!";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_TAPER_FAILED:
                return "B:V01";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_FCC_FAILED:
                return "B:V9D";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_DESIGN_CAPACITY_FAILED:
                return "B:V9F";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_DESIGN_VOLTAGE_FAILED:
                return "B:VA3";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_CHARGING_VOLTAGE_FAILED:
                return "B:VFD";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_EDV0_FAILED:
                return "B:VB4";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_EDV1_FAILED:
                return "B:VB7";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_EDV2_FAILED:
                return "B:VBA";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_VOLTAGE_0_DOD_FAILED:
                return "B:VBD";
            case FightpadBQ27220BatteryAddon::ReadStatus::CONFIG_VERIFY_VOLTAGE_100_DOD_FAILED:
                return "B:VD1";
            case FightpadBQ27220BatteryAddon::ReadStatus::OK:
            default:
                return "BAT:--%";
        }
#else
        return "";
#endif
    }

    std::string getFightpadBatterySecurityText()
    {
#if FIGHTPAD12SLIM_BQ27220_ENABLED
        if (!FightpadBQ27220BatteryAddon::isBatterySecurityStatusValid()) {
            return "";
        }

        std::string text = "S:";
        text += FightpadBQ27220BatteryAddon::getBatterySecurityStatusCode();
        return text;
#else
        return "";
#endif
    }

    void drawFightpadBatteryIcon(GPGFX* renderer)
    {
#if FIGHTPAD12SLIM_BQ27220_ENABLED
        const uint8_t bars = FightpadBQ27220BatteryAddon::getBatteryLevelBars();

        for (uint8_t y = 0; y < BATTERY_ICON_BODY_HEIGHT; y++) {
            for (uint8_t x = 0; x < (BATTERY_ICON_BODY_WIDTH + BATTERY_ICON_CAP_WIDTH); x++) {
                renderer->drawPixel(BATTERY_ICON_X + x, BATTERY_ICON_Y + y, 0);
            }
        }

        for (uint8_t y = 0; y < BATTERY_ICON_BODY_HEIGHT; y++) {
            for (uint8_t x = 0; x < BATTERY_ICON_BODY_WIDTH; x++) {
                const bool border = x == 0 || y == 0 || x == (BATTERY_ICON_BODY_WIDTH - 1) || y == (BATTERY_ICON_BODY_HEIGHT - 1);
                if (border) {
                    renderer->drawPixel(BATTERY_ICON_X + x, BATTERY_ICON_Y + y, 1);
                }
            }
        }

        for (uint8_t y = 0; y < BATTERY_ICON_CAP_HEIGHT; y++) {
            for (uint8_t x = 0; x < BATTERY_ICON_CAP_WIDTH; x++) {
                renderer->drawPixel(BATTERY_ICON_X + BATTERY_ICON_BODY_WIDTH + x, BATTERY_ICON_Y + 2 + y, 1);
            }
        }

        for (uint8_t cell = 0; cell < 4; cell++) {
            const uint8_t cellX = BATTERY_ICON_X + 2 + cell * (BATTERY_CELL_WIDTH + BATTERY_CELL_GAP);

            for (uint8_t y = 0; y < BATTERY_CELL_HEIGHT; y++) {
                for (uint8_t x = 0; x < BATTERY_CELL_WIDTH; x++) {
                    const bool cellBorder = x == 0 || y == 0 || x == (BATTERY_CELL_WIDTH - 1) || y == (BATTERY_CELL_HEIGHT - 1);
                    renderer->drawPixel(cellX + x, BATTERY_ICON_Y + 2 + y, (cell < bars || cellBorder) ? 1 : 0);
                }
            }
        }
#endif
    }
    void drawFightpadBatteryPercent(GPGFX* renderer)
    {
#if FIGHTPAD12SLIM_BQ27220_ENABLED
        std::string percentText = std::to_string(FightpadBQ27220BatteryAddon::getBatteryPercent()) + "%";
        while (percentText.length() < 4) {
            percentText = " " + percentText;
        }

        for (uint8_t y = 0; y < BATTERY_PERCENT_PIXEL_HEIGHT; y++) {
            for (uint8_t x = 0; x < BATTERY_PERCENT_PIXEL_WIDTH; x++) {
                renderer->drawPixel(BATTERY_PERCENT_PIXEL_X + x, BATTERY_PERCENT_PIXEL_Y + y, 0);
            }
        }

        renderer->drawText(BATTERY_PERCENT_TEXT_COLUMN, BATTERY_PERCENT_TEXT_ROW, percentText);
#endif
    }
    std::string getFixedWidthNumber(uint32_t value, uint8_t width)
    {
        std::string text = std::to_string(value);
        while (text.length() < width) {
            text = "0" + text;
        }
        return text;
    }

    std::string getFixedWidthHex(uint32_t value, uint8_t width)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string text(width, '0');
        for (int8_t index = static_cast<int8_t>(width) - 1; index >= 0; index--) {
            text[index] = digits[value & 0x0F];
            value >>= 4;
        }
        return text;
    }

    std::string getSignedDiagnosticNumber(int16_t value)
    {
        std::string text = std::to_string(value);
        if (value >= 0) {
            text = "+" + text;
        }
        return text;
    }

    void drawFightpadBatteryDiagnosticValues(GPGFX* renderer)
    {
#if FIGHTPAD12SLIM_BQ27220_ENABLED
        std::string line1 = "SOC:";
        if (FightpadBQ27220BatteryAddon::isBatteryPercentValid()) {
            line1 += getFixedWidthNumber(FightpadBQ27220BatteryAddon::getBatteryPercent(), 3);
            line1 += "%";
        } else {
            line1 += "---%";
        }

        line1 += " V:";
        if (FightpadBQ27220BatteryAddon::isBatteryVoltageValid()) {
            line1 += std::to_string(FightpadBQ27220BatteryAddon::getBatteryVoltageMillivolts());
        } else {
            line1 += "----";
        }

        std::string line2 = "I:";
        if (FightpadBQ27220BatteryAddon::isBatteryCurrentValid()) {
            line2 += getSignedDiagnosticNumber(FightpadBQ27220BatteryAddon::getBatteryCurrentMilliamps());
        } else {
            line2 += "-----";
        }

        std::string lineFCC = "FCC:";
        if (FightpadBQ27220BatteryAddon::isBatteryFullChargeCapacityValid()) {
            lineFCC += std::to_string(FightpadBQ27220BatteryAddon::getBatteryFullChargeCapacityMah());
        } else {
            lineFCC += "-----";
        }

        renderer->drawText(0, 0, line1);
        renderer->drawText(0, 1, line2);
        renderer->drawText(0, 7, lineFCC);

#if FIGHTPAD12SLIM_BQ27220_DATA_MEMORY_DIAGNOSTIC_DISPLAY
        if (FightpadBQ27220BatteryAddon::isBatteryDataMemoryDebugValid()) {
            std::string line3 = "DM:";
            line3 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugAddress(), 4);
            line3 += " O:";
            line3 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugOldValue(), 4);

            std::string line4 = "N:";
            line4 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugTargetValue(), 4);
            line4 += " R:";
            line4 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugVerifyValue(), 4);

            std::string line5 = "C:";
            line5 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugOldChecksum(), 2);
            line5 += ">";
            line5 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugNewChecksum(), 2);
            line5 += " L:";
            line5 += getFixedWidthHex(FightpadBQ27220BatteryAddon::getBatteryDataMemoryDebugLength(), 2);

            renderer->drawText(0, 2, line3);
            renderer->drawText(0, 3, line4);
            renderer->drawText(0, 4, line5);
        }
#endif

        const std::string diagnosticText = getFightpadBatteryDiagnosticText();
        if (!diagnosticText.empty()) {
            renderer->drawText(0, 7, diagnosticText);
        }

        const std::string securityText = getFightpadBatterySecurityText();
        if (!securityText.empty()) {
            renderer->drawText(7, 7, securityText);
        }
#endif
    }
}
void ButtonLayoutScreen::init() {
    isInputHistoryEnabled = Storage::getInstance().getDisplayOptions().inputHistoryEnabled;
    inputHistoryX = Storage::getInstance().getDisplayOptions().inputHistoryRow;
    inputHistoryY = Storage::getInstance().getDisplayOptions().inputHistoryCol;
    inputHistoryLength = Storage::getInstance().getDisplayOptions().inputHistoryLength;
    bannerDelayStart = getMillis();
    gamepad = Storage::getInstance().GetGamepad();
    inputMode = DriverManager::getInstance().getInputMode();

    EventManager::getInstance().registerEventHandler(GP_EVENT_PROFILE_CHANGE, GPEVENT_CALLBACK(this->handleProfileChange(event)));
    EventManager::getInstance().registerEventHandler(GP_EVENT_USBHOST_MOUNT, GPEVENT_CALLBACK(this->handleUSB(event)));
    EventManager::getInstance().registerEventHandler(GP_EVENT_USBHOST_UNMOUNT, GPEVENT_CALLBACK(this->handleUSB(event)));
    
    footer = "";
    historyString = "";
    inputHistory.clear();

    setViewport((isInputHistoryEnabled ? 8 : 0), 0, (isInputHistoryEnabled ? 56 : getRenderer()->getDriver()->getMetrics()->height), getRenderer()->getDriver()->getMetrics()->width);

	// load layout (drawElement pushes element to the display list)
    uint16_t elementCtr = 0;
    LayoutManager::LayoutList currLayoutLeft = LayoutManager::getInstance().getLayoutA();
    LayoutManager::LayoutList currLayoutRight = LayoutManager::getInstance().getLayoutB();
    for (elementCtr = 0; elementCtr < currLayoutLeft.size(); elementCtr++) {
        pushElement(currLayoutLeft[elementCtr]);
    }
    for (elementCtr = 0; elementCtr < currLayoutRight.size(); elementCtr++) {
        pushElement(currLayoutRight[elementCtr]);
    }

	// Start directly on the normal BUTTONS header. Profile changes that occur
	// later still enable the temporary banner in update().
	bannerDisplay = false;
    prevProfileNumber = gamepad->getOptions().profileNumber;

    prevLayoutLeft = Storage::getInstance().getDisplayOptions().buttonLayout;
    prevLayoutRight = Storage::getInstance().getDisplayOptions().buttonLayoutRight;
    prevLeftOptions = Storage::getInstance().getDisplayOptions().buttonLayoutCustomOptions.paramsLeft;
    prevRightOptions = Storage::getInstance().getDisplayOptions().buttonLayoutCustomOptions.paramsRight;
    prevOrientation = Storage::getInstance().getDisplayOptions().buttonLayoutOrientation;

    // we cannot look at macro options enabled, pull the pins
    
    // macro display now uses our pin functions, so we need to check if pins are enabled...
    macroEnabled = false;
    hasTurboAssigned = false;
    // Macro Button initialized by void Gamepad::setup()
    GpioMappingInfo* pinMappings = Storage::getInstance().getProfilePinMappings();
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        switch( pinMappings[pin].action ) {
            case GpioAction::BUTTON_PRESS_MACRO:
            case GpioAction::BUTTON_PRESS_MACRO_1:
            case GpioAction::BUTTON_PRESS_MACRO_2:
            case GpioAction::BUTTON_PRESS_MACRO_3:
            case GpioAction::BUTTON_PRESS_MACRO_4:
            case GpioAction::BUTTON_PRESS_MACRO_5:
            case GpioAction::BUTTON_PRESS_MACRO_6:
                macroEnabled = true;
                break;
            case GpioAction::BUTTON_PRESS_TURBO:
                hasTurboAssigned = true;
                break;
            default:
                break;
        }
    }

    // determine which fields will be displayed on the status bar
    showInputMode = Storage::getInstance().getDisplayOptions().inputMode;
    showTurboMode = Storage::getInstance().getDisplayOptions().turboMode && hasTurboAssigned;
    showDpadMode = Storage::getInstance().getDisplayOptions().dpadMode;
    showSocdMode = Storage::getInstance().getDisplayOptions().socdMode;
    showMacroMode = Storage::getInstance().getDisplayOptions().macroMode;
    showProfileMode = Storage::getInstance().getDisplayOptions().profileMode;

#if FIGHTPAD12SLIM_BQ27220_ENABLED
    // Keep the Fightpad12Slim header focused on the input mode and battery status.
    showTurboMode = false;
    showDpadMode = false;
    showSocdMode = false;
#endif

    getRenderer()->clearScreen();
}

void ButtonLayoutScreen::shutdown() {
    clearElements();

    EventManager::getInstance().unregisterEventHandler(GP_EVENT_PROFILE_CHANGE, GPEVENT_CALLBACK(this->handleProfileChange(event)));
    EventManager::getInstance().unregisterEventHandler(GP_EVENT_USBHOST_MOUNT, GPEVENT_CALLBACK(this->handleUSB(event)));
    EventManager::getInstance().unregisterEventHandler(GP_EVENT_USBHOST_UNMOUNT, GPEVENT_CALLBACK(this->handleUSB(event)));
}

int8_t ButtonLayoutScreen::update() {
    bool configMode = DriverManager::getInstance().isConfigMode();
    uint8_t profileNumber = getGamepad()->getOptions().profileNumber;

    // In Web Config, B1 returns to the instruction screen. Check B1's own
    // release edge before doing any layout/history work so other held button
    // bits cannot block the exit.
    if (configMode) {
        uint16_t buttonState = getGamepad()->state.buttons;
        bool b1Released = (prevButtonState & GAMEPAD_MASK_B1) &&
                          !(buttonState & GAMEPAD_MASK_B1);
        prevButtonState = buttonState;
        if (b1Released) {
            return DisplayMode::CONFIG_INSTRUCTION;
        }
    }
    
    // Check if we've updated button layouts while in config mode
    if (configMode) {
        uint8_t layoutLeft = Storage::getInstance().getDisplayOptions().buttonLayout;
        uint8_t layoutRight = Storage::getInstance().getDisplayOptions().buttonLayoutRight;
        uint8_t buttonLayoutOrientation = Storage::getInstance().getDisplayOptions().buttonLayoutOrientation;
        bool inputHistoryEnabled = Storage::getInstance().getDisplayOptions().inputHistoryEnabled;
        if ((prevLayoutLeft != layoutLeft) || (prevLayoutRight != layoutRight) || (isInputHistoryEnabled != inputHistoryEnabled) || compareCustomLayouts() || (prevOrientation != buttonLayoutOrientation)) {
            shutdown();
            init();
        }
    }

    // main logic loop
    if (prevProfileNumber != profileNumber) {
        bannerDelayStart = getMillis();
        prevProfileNumber = profileNumber;
        bannerDisplay = true;
    }

    // main logic loop
	generateHeader();
    if (isInputHistoryEnabled)
		processInputHistory();
	return -1;
}

void ButtonLayoutScreen::generateHeader() {
	// Limit to 21 chars with 6x8 font for now
	statusBar.clear();
	Storage& storage = Storage::getInstance();

	// Display Profile # banner
	if ( bannerDisplay ) {
		if (((getMillis() - bannerDelayStart) / 1000) < bannerDelay) {
			if (bannerMessage.empty()) {
				statusBar.assign(storage.currentProfileLabel(), strlen(storage.currentProfileLabel()));
				if (statusBar.empty()) {
					statusBar = "     Profile #";
					statusBar +=  std::to_string(getGamepad()->getOptions().profileNumber);
				} else {
					statusBar.insert(statusBar.begin(), (21-statusBar.length())/2, ' ');
				}
			} else {
				statusBar = bannerMessage;
			}
			return;
		} else {
			bannerDisplay = false;
            bannerMessage.clear();
		}
	}

    if (showInputMode) {
        FightpadBluetoothProfile bluetoothProfile;
        if (getFightpadESP32ActiveBluetoothProfile(bluetoothProfile)) {
            statusBar += getActiveBluetoothControllerLabel(bluetoothProfile);
        } else {
        // Display standard header
        switch (inputMode)
        {
            case INPUT_MODE_PS3:    statusBar += "PS3"; break;
            case INPUT_MODE_GENERIC: statusBar += "USBHID"; break;
            case INPUT_MODE_SWITCH: statusBar += "SWITCH"; break;
            case INPUT_MODE_MDMINI: statusBar += "GEN/MD"; break;
            case INPUT_MODE_NEOGEO: statusBar += "NGMINI"; break;
            case INPUT_MODE_PCEMINI: statusBar += "PCE/TG"; break;
            case INPUT_MODE_EGRET: statusBar += "EGRET"; break;
            case INPUT_MODE_ASTRO: statusBar += "ASTRO"; break;
            case INPUT_MODE_PSCLASSIC: statusBar += "PSC"; break;
            case INPUT_MODE_XBOXORIGINAL: statusBar += "OGXBOX"; break;
            case INPUT_MODE_SWITCH_PRO: statusBar += "SWPRO"; break;
            case INPUT_MODE_PS4:
                statusBar += "PS4";
                if(((PS4Driver*)DriverManager::getInstance().getDriver())->getAuthSent() == true )
                    statusBar += ":AS";
                else
                    statusBar += "   ";
                break;
            case INPUT_MODE_PS5:
                statusBar += "PS5";
                if(((PS4Driver*)DriverManager::getInstance().getDriver())->getAuthSent() == true )
                    statusBar += ":AS";
                else
                    statusBar += "   ";
                break;
            case INPUT_MODE_P5GENERAL:
                statusBar += "P5G";
                if(((P5GeneralDriver*)DriverManager::getInstance().getDriver())->getAuthSent() == true )
                    statusBar += ":AS";
                else
                    statusBar += "   ";
                break;
            case INPUT_MODE_XBONE:
                statusBar += "XBON";
                if(((XBOneDriver*)DriverManager::getInstance().getDriver())->getAuthSent() == true )
                    statusBar += "E";
                else
                    statusBar += "*";
                break;
            case INPUT_MODE_XINPUT:
                statusBar += "X";
                if(((XInputDriver*)DriverManager::getInstance().getDriver())->getAuthSent() == true )
                    statusBar += "B360";
                else
                    statusBar += "INPUT";
                break;
            case INPUT_MODE_KEYBOARD: statusBar += "HID-KB"; break;
            case INPUT_MODE_CONFIG: statusBar += "CONFIG"; break;
        }
        }
    }

    if (showTurboMode) {
        const TurboOptions& turboOptions = storage.getAddonOptions().turboOptions;
        if ( turboOptions.enabled ) {
            statusBar += " T";
            if ( turboOptions.shotCount < 10 ) // padding
                statusBar += "0";
            statusBar += std::to_string(turboOptions.shotCount);
        } else {
            statusBar += "    "; // no turbo, don't show Txx setting
        }
    }

	const GamepadOptions & options = gamepad->getOptions();

    if (showDpadMode) {
        switch (gamepad->getActiveDpadMode())
        {
            case DPAD_MODE_DIGITAL:      statusBar += " D"; break;
            case DPAD_MODE_LEFT_ANALOG:  statusBar += " L"; break;
            case DPAD_MODE_RIGHT_ANALOG: statusBar += " R"; break;
        }
    }

    if (showSocdMode) {
        switch (Gamepad::resolveSOCDMode(options))
        {
            case SOCD_MODE_NEUTRAL:               statusBar += " SOCD-N"; break;
            case SOCD_MODE_UP_PRIORITY:           statusBar += " SOCD-U"; break;
            case SOCD_MODE_SECOND_INPUT_PRIORITY: statusBar += " SOCD-L"; break;
            case SOCD_MODE_FIRST_INPUT_PRIORITY:  statusBar += " SOCD-F"; break;
            case SOCD_MODE_BYPASS:                statusBar += " SOCD-X"; break;
        }
    }

    if (showMacroMode && macroEnabled) statusBar += " M";

    if (showProfileMode) {
        statusBar += " ";

        std::string profile;
        profile.assign(storage.currentProfileLabel(), strlen(storage.currentProfileLabel()));
        if (profile.empty()) {
            statusBar += std::to_string(options.profileNumber);
        } else {
            statusBar += profile;
        }
    }

    trim(statusBar);
}

void ButtonLayoutScreen::drawScreen() {
#if FIGHTPAD12SLIM_BQ27220_ENABLED && FIGHTPAD12SLIM_BQ27220_DIAGNOSTIC_DISPLAY
    drawFightpadBatteryDiagnosticValues(getRenderer());
    return;
#endif

    if (bannerDisplay) {
        getRenderer()->drawRectangle(0, 0, 128, 7, true, true);
    	getRenderer()->drawText(0, 0, statusBar, true);
    } else {
#if FIGHTPAD12SLIM_BQ27220_ENABLED
#if FIGHTPAD12SLIM_BQ27220_DIAGNOSTIC_DISPLAY
        drawFightpadBatteryDiagnosticValues(getRenderer());
#else
        std::string displayStatus = statusBar;
        if (displayStatus.length() > BATTERY_STATUS_COLUMN) {
            displayStatus.resize(BATTERY_STATUS_COLUMN);
        }
        getRenderer()->drawText(0, 0, displayStatus);
        drawFightpadBatteryIcon(getRenderer());
#endif
#else
        getRenderer()->drawText(0, 0, statusBar);
#endif
	}
#if FIGHTPAD12SLIM_BQ27220_ENABLED
    if (!FIGHTPAD12SLIM_BQ27220_DIAGNOSTIC_DISPLAY) {
        getRenderer()->drawText(0, 7, footer);
        if (!bannerDisplay && FightpadBQ27220BatteryAddon::isBatteryPercentValid()) {
            drawFightpadBatteryPercent(getRenderer());
        }
    }
#else
    getRenderer()->drawText(0, 7, footer);
#endif
}

GPLever* ButtonLayoutScreen::addLever(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor, uint16_t inputType) {
    GPLever* lever = new GPLever();
    lever->setRenderer(getRenderer());
    lever->setPosition(startX, startY);
    lever->setStrokeColor(strokeColor);
    lever->setFillColor(fillColor);
    lever->setRadius(sizeX);
    lever->setInputType(inputType);
    lever->setViewport(this->getViewport());
    return (GPLever*)addElement(lever);
}

GPButton* ButtonLayoutScreen::addButton(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor, int16_t inputMask) {
    GPButton* button = new GPButton();
    button->setRenderer(getRenderer());
    button->setPosition(startX, startY);
    button->setStrokeColor(strokeColor);
    button->setFillColor(fillColor);
    button->setSize(sizeX, sizeY);
    button->setInputMask(inputMask);
    button->setViewport(this->getViewport());
    return (GPButton*)addElement(button);
}

GPShape* ButtonLayoutScreen::addShape(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor) {
    GPShape* shape = new GPShape();
    shape->setRenderer(getRenderer());
    shape->setPosition(startX, startY);
    shape->setStrokeColor(strokeColor);
    shape->setFillColor(fillColor);
    shape->setSize(sizeX,sizeY);
    shape->setViewport(this->getViewport());
    return (GPShape*)addElement(shape);
}

GPSprite* ButtonLayoutScreen::addSprite(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY) {
    GPSprite* sprite = new GPSprite();
    sprite->setRenderer(getRenderer());
    sprite->setPosition(startX, startY);
    sprite->setSize(sizeX,sizeY);
    sprite->setViewport(this->getViewport());
    return (GPSprite*)addElement(sprite);
}

GPWidget* ButtonLayoutScreen::pushElement(GPButtonLayout element) {
    if (element.elementType == GP_ELEMENT_LEVER) {
        return addLever(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2, element.parameters.stroke, element.parameters.fill, element.parameters.value);
    } else if ((element.elementType == GP_ELEMENT_BTN_BUTTON) || (element.elementType == GP_ELEMENT_DIR_BUTTON) || (element.elementType == GP_ELEMENT_PIN_BUTTON)) {
        GPButton* button = addButton(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2, element.parameters.stroke, element.parameters.fill, element.parameters.value);

        // set type of button
        button->setInputType(element.elementType);
        button->setInputDirection(false);
        button->setShape((GPShape_Type)element.parameters.shape);
        button->setAngle(element.parameters.angleStart);
        button->setAngleEnd(element.parameters.angleEnd);
        button->setClosed(element.parameters.closed);

        if (element.elementType == GP_ELEMENT_DIR_BUTTON) button->setInputDirection(true);

        return (GPWidget*)button;
    } else if (element.elementType == GP_ELEMENT_SPRITE) {
        return addSprite(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2);
    } else if (element.elementType == GP_ELEMENT_SHAPE) {
        GPShape* shape = addShape(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2, element.parameters.stroke, element.parameters.fill);
        shape->setShape((GPShape_Type)element.parameters.shape);
        shape->setAngle(element.parameters.angleStart);
        shape->setAngleEnd(element.parameters.angleEnd);
        shape->setClosed(element.parameters.closed);
        return shape;
    }
    return NULL;
}

void ButtonLayoutScreen::processInputHistory() {
	std::deque<std::string> pressed;

	// Get key states
	std::array<bool, INPUT_HISTORY_MAX_INPUTS> currentInput = {

		pressedUp(),
		pressedDown(),
		pressedLeft(),
		pressedRight(),

		pressedUpLeft(),
		pressedUpRight(),
		pressedDownLeft(),
		pressedDownRight(),

		getProcessedGamepad()->pressedB1(),
		getProcessedGamepad()->pressedB2(),
		getProcessedGamepad()->pressedB3(),
		getProcessedGamepad()->pressedB4(),
		getProcessedGamepad()->pressedL1(),
		getProcessedGamepad()->pressedR1(),
		getProcessedGamepad()->pressedL2(),
		getProcessedGamepad()->pressedR2(),
		getProcessedGamepad()->pressedS1(),
		getProcessedGamepad()->pressedS2(),
		getProcessedGamepad()->pressedL3(),
		getProcessedGamepad()->pressedR3(),
		getProcessedGamepad()->pressedA1(),
		getProcessedGamepad()->pressedA2(),
	};

	const InputMode displayInputMode = getActiveControllerDisplayMode(inputMode);
	uint8_t mode = ((displayModeLookup.count(displayInputMode) > 0) ?
	                displayModeLookup.at(displayInputMode) : 0);

	// Check if any new keys have been pressed
	if (lastInput != currentInput) {
		// Iterate through array
		for (uint8_t x=0; x<INPUT_HISTORY_MAX_INPUTS; x++) {
			// Add any pressed keys to deque
			std::string inputChar(displayNames[mode][x]);
			if (currentInput[x] && (inputChar != "")) pressed.push_back(inputChar);
		}
		// Update the last keypress array
		lastInput = currentInput;
	}

	if (pressed.size() > 0) {
		std::string newInput;
		for(const auto &s : pressed) {
				if(!newInput.empty())
						newInput += "+";
				newInput += s;
		}

		inputHistory.push_back(newInput);
	}

	if (inputHistory.size() > (inputHistoryLength / 2) + 1) {
		inputHistory.pop_front();
	}

	std::string ret;

	for (auto it = inputHistory.crbegin(); it != inputHistory.crend(); ++it) {
		std::string newRet = ret;
		if (!newRet.empty())
			newRet = " " + newRet;

		newRet = *it + newRet;
		ret = newRet;

		if (ret.size() >= inputHistoryLength) {
			break;
		}
	}

	if(ret.size() >= inputHistoryLength) {
		historyString = ret.substr(ret.size() - inputHistoryLength);
	} else {
		historyString = ret;
	}

    footer = historyString;
}

bool ButtonLayoutScreen::compareCustomLayouts()
{
    ButtonLayoutParamsLeft leftOptions = Storage::getInstance().getDisplayOptions().buttonLayoutCustomOptions.paramsLeft;
    ButtonLayoutParamsRight rightOptions = Storage::getInstance().getDisplayOptions().buttonLayoutCustomOptions.paramsRight;

    bool leftChanged = ((leftOptions.layout != prevLeftOptions.layout) || (leftOptions.common.startX != prevLeftOptions.common.startX) || (leftOptions.common.startY != prevLeftOptions.common.startY) || (leftOptions.common.buttonPadding != prevLeftOptions.common.buttonPadding) || (leftOptions.common.buttonRadius != prevLeftOptions.common.buttonRadius));
    bool rightChanged = ((rightOptions.layout != prevRightOptions.layout) || (rightOptions.common.startX != prevRightOptions.common.startX) || (rightOptions.common.startY != prevRightOptions.common.startY) || (rightOptions.common.buttonPadding != prevRightOptions.common.buttonPadding) || (rightOptions.common.buttonRadius != prevRightOptions.common.buttonRadius));
    
    return (leftChanged || rightChanged);
}

bool ButtonLayoutScreen::pressedUp()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == GAMEPAD_MASK_UP);
        case DPAD_MODE_LEFT_ANALOG:  return getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MIN;
        case DPAD_MODE_RIGHT_ANALOG: return getProcessedGamepad()->state.ry == GAMEPAD_JOYSTICK_MIN;
    }

    return false;
}

bool ButtonLayoutScreen::pressedDown()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == GAMEPAD_MASK_DOWN);
        case DPAD_MODE_LEFT_ANALOG:  return getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MAX;
        case DPAD_MODE_RIGHT_ANALOG: return getProcessedGamepad()->state.ry == GAMEPAD_JOYSTICK_MAX;
    }

    return false;
}

bool ButtonLayoutScreen::pressedLeft()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == GAMEPAD_MASK_LEFT);
        case DPAD_MODE_LEFT_ANALOG:  return getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MIN;
        case DPAD_MODE_RIGHT_ANALOG: return getProcessedGamepad()->state.rx == GAMEPAD_JOYSTICK_MIN;
    }

    return false;
}

bool ButtonLayoutScreen::pressedRight()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == GAMEPAD_MASK_RIGHT);
        case DPAD_MODE_LEFT_ANALOG:  return getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MAX;
        case DPAD_MODE_RIGHT_ANALOG: return getProcessedGamepad()->state.rx == GAMEPAD_JOYSTICK_MAX;
    }

    return false;
}

bool ButtonLayoutScreen::pressedUpLeft()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == (GAMEPAD_MASK_UP | GAMEPAD_MASK_LEFT));
        case DPAD_MODE_LEFT_ANALOG:  return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MIN) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MIN);
        case DPAD_MODE_RIGHT_ANALOG: return (getProcessedGamepad()->state.rx == GAMEPAD_JOYSTICK_MIN) && (getProcessedGamepad()->state.ry == GAMEPAD_JOYSTICK_MIN);
    }

    return false;
}

bool ButtonLayoutScreen::pressedUpRight()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == (GAMEPAD_MASK_UP | GAMEPAD_MASK_RIGHT));
        case DPAD_MODE_LEFT_ANALOG:  return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MAX) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MIN);
        case DPAD_MODE_RIGHT_ANALOG: return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MAX) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MIN);
    }

    return false;
}

bool ButtonLayoutScreen::pressedDownLeft()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == (GAMEPAD_MASK_DOWN | GAMEPAD_MASK_LEFT));
        case DPAD_MODE_LEFT_ANALOG:  return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MIN) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MAX);
        case DPAD_MODE_RIGHT_ANALOG: return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MIN) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MAX);
    }

    return false;
}

bool ButtonLayoutScreen::pressedDownRight()
{
    switch (getGamepad()->getActiveDpadMode())
    {
        case DPAD_MODE_DIGITAL:      return ((getProcessedGamepad()->state.dpad & GAMEPAD_MASK_DPAD) == (GAMEPAD_MASK_DOWN | GAMEPAD_MASK_RIGHT));
        case DPAD_MODE_LEFT_ANALOG:  return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MAX) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MAX);
        case DPAD_MODE_RIGHT_ANALOG: return (getProcessedGamepad()->state.lx == GAMEPAD_JOYSTICK_MAX) && (getProcessedGamepad()->state.ly == GAMEPAD_JOYSTICK_MAX);
    }

    return false;
}

void ButtonLayoutScreen::handleProfileChange(GPEvent* e) {
    GPProfileChangeEvent* event = (GPProfileChangeEvent*)e;

    profileNumber = event->currentValue;
    prevProfileNumber = event->previousValue;
}

void ButtonLayoutScreen::handleUSB(GPEvent* e) {
    bannerDelayStart = getMillis();
    prevProfileNumber = profileNumber;

    if (e->eventType() == GP_EVENT_USBHOST_MOUNT) {
        bannerMessage = "    USB Connected";
    } else if (e->eventType() == GP_EVENT_USBHOST_UNMOUNT) {
        bannerMessage = "  USB Disconnnected";
    }
    bannerDisplay = true;
}

void ButtonLayoutScreen::trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](unsigned char c) { return !std::isspace(c); }));
}
