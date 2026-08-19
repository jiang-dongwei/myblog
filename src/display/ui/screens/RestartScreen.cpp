#include "RestartScreen.h"

#include "pico/stdlib.h"
#include "system.h"

void RestartScreen::init() {
    getRenderer()->clearScreen();
    //splashStartTime = getMillis();
}

void RestartScreen::shutdown() {
    clearElements();
}

void RestartScreen::drawScreen() {
    // Reuse the same persisted 128x64 image shown by SplashScreen. This keeps
    // the Web Config restart page aligned with the product logo selected by
    // the user (or the Fightpad board default on a freshly flashed device).
    getRenderer()->drawSprite(
        (uint8_t*)getDisplayOptions().splashImage.bytes,
        128, 64, 16, 0, 0, 1);

    // Reserve the bottom two text rows so mode and progress messages remain
    // readable regardless of the colors/pixels in the configured image.
    getRenderer()->drawRectangle(0, 48, 128, 64, 0, 1);

    switch ((System::BootMode)this->bootMode) {
        case System::BootMode::USB:
            getRenderer()->drawText(1, 6, "Rebooting to BOOTSEL");
            getRenderer()->drawText(2, 7, "and Mounting Drive");
            break;
        case System::BootMode::WEBCONFIG:
            getRenderer()->drawText(2, 6, "Booting WebConfig");
            getRenderer()->drawText(4, 7, "Please Wait");
            break;
        case System::BootMode::GAMEPAD:
        case System::BootMode::DEFAULT:
            getRenderer()->drawText(4, 6, "Gamepad Mode");
            getRenderer()->drawText(4, 7, "Please Wait");
            break;
    }
}

void RestartScreen::setBootMode(uint32_t mode) {
    this->bootMode = mode;
}

int8_t RestartScreen::update() {
    return -1; // -1 means no change in screen state
}
