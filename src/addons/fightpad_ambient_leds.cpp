#include "addons/fightpad_ambient_leds.h"

#include "addons/fightpad_bq27220_battery.h"
#include "addons/fightpad_esp32_proxy.h"
#include "addons/scrollwheel_menu.h"

#include "gamepad.h"
#include "storagemanager.h"
#include <cmath>

volatile uint8_t fightpadAmbientDiagControls = 0;
volatile uint8_t fightpadAmbientDiagDataOut = 0;
volatile uint8_t fightpadAmbientDiagEffect = 0;
volatile uint8_t fightpadAmbientDiagEnabled = 0;
volatile uint32_t fightpadAmbientDiagTicks = 0;
volatile uint32_t fightpadAmbientDiagShows = 0;

// Color array for cycling through effects
static const RGB effectColors[] = {
    ColorRed,       // 0: Red breathing
    ColorOrange,    // 1: Orange breathing
    ColorYellow,    // 2: Yellow breathing
    ColorLimeGreen, // 3: Lime green breathing
    ColorGreen,     // 4: Green breathing
    ColorAqua,      // 5: Aqua breathing
    ColorBlue,      // 6: Blue breathing
    ColorPurple,    // 7: Purple breathing
};

static float getMenuEffectBrightness() {
    switch (g_menuBrightnessLevel) {
        case 1: return 0.3f;
        case 2: return 0.1f;
        case 0:
        default: return 0.5f;
    }
}

static void applySelectorState(uint8_t selector, bool& enabled, uint8_t& effectIndex) {
    constexpr uint8_t effectCount = sizeof(effectColors) / sizeof(effectColors[0]);

    if (selector == 0) {
        enabled = false;
        return;
    }

    enabled = true;
    effectIndex = (selector - 1) % effectCount;
}

static RGB makeControlDiagnosticColor(uint8_t controls) {
    uint8_t red = (controls & 0x01) ? 0xFF : 0x00;
    uint8_t green = (controls & 0x02) ? 0xFF : 0x00;
    uint8_t blue = (controls & 0x04) ? 0xFF : 0x00;

    if ((red | green | blue) == 0) {
        return RGB(0x10, 0x10, 0x10);
    }

    return RGB(red, green, blue);
}

static uint8_t readRawControlBits() {
    uint8_t controls = 0;

    if (isValidPin(FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN) && gpio_get(FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN)) {
        controls |= 0x01;
    }

    if (isValidPin(FIGHTPAD12SLIM_AMBIENT_PREV_PIN) && gpio_get(FIGHTPAD12SLIM_AMBIENT_PREV_PIN)) {
        controls |= 0x02;
    }

    if (isValidPin(FIGHTPAD12SLIM_AMBIENT_NEXT_PIN) && gpio_get(FIGHTPAD12SLIM_AMBIENT_NEXT_PIN)) {
        controls |= 0x04;
    }

    return controls;
}

static void setFlashUntil(uint32_t *flashTimes, int ledIndex, uint32_t until) {
    if (ledIndex >= 0 && ledIndex < FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT) {
        flashTimes[ledIndex] = until;
    }
}

// ── Menu → RGB mapping ──────────────────────────────────────────────────
// g_menuRgbTop/Bottom/Button store AnimationStation `colors` vector indices
// (0=Black..15=Violet).  Direct lookup — index space is shared with proto
// AnimationOptions.staticColorIndex / buttonColorIndex.
static RGB menuIndexToColor(uint8_t idx) {
    // Index 0xFF = not set (caller checks before calling this).
    if (idx < ::colors.size())
        return ::colors[idx];
    return ColorBlack;  // fallback
}

static RGB chaseColorFor(uint16_t ledIndex, uint16_t ledCount, int16_t wheelFrame) {
    uint16_t phase = static_cast<uint16_t>(wheelFrame);
    if (ledCount > 0) {
        phase += static_cast<uint16_t>((ledIndex * 256U) / ledCount);
    }
    return RGB::wheel(static_cast<uint8_t>(phase));
}

bool FightpadAmbientLEDAddon::available() {
    if (FIGHTPAD12SLIM_AMBIENT_CONTROL_DIAGNOSTIC) {
        return true;
    }

    return FIGHTPAD12SLIM_AMBIENT_ENABLED &&
           isValidPin(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN) &&
           FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT > 0 &&
           FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT <= 100;
}

void FightpadAmbientLEDAddon::setup() {
    int controlPins[] = {
        FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN,
        FIGHTPAD12SLIM_AMBIENT_PREV_PIN,
        FIGHTPAD12SLIM_AMBIENT_NEXT_PIN,
    };

    for (int pin : controlPins) {
        if (isValidPin(pin)) {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_IN);
            gpio_set_input_enabled(pin, true);
            gpio_pull_up(pin);
        }
    }

#if FIGHTPAD12SLIM_AMBIENT_DRIVE_BOOST_EN
    if (FIGHTPAD12SLIM_AMBIENT_ENABLED && isValidPin(FIGHTPAD12SLIM_BOOST_EN_PIN)) {
        gpio_init(FIGHTPAD12SLIM_BOOST_EN_PIN);
#if FIGHTPAD12SLIM_AMBIENT_BOOST_EXTERNAL_PULLUP
        gpio_set_dir(FIGHTPAD12SLIM_BOOST_EN_PIN, GPIO_IN);
        gpio_disable_pulls(FIGHTPAD12SLIM_BOOST_EN_PIN);
#else
        gpio_set_dir(FIGHTPAD12SLIM_BOOST_EN_PIN, GPIO_OUT);
        gpio_put(FIGHTPAD12SLIM_BOOST_EN_PIN, FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL ? 1 : 0);
#endif
        boostPowerEnabled = true;
    }
#endif

#if FIGHTPAD12SLIM_AMBIENT_POWER_ONLY_DIAGNOSTIC
    if (FIGHTPAD12SLIM_AMBIENT_ENABLED) {
        lastControls = readControls();
        fightpadAmbientDiagControls = lastControls;
        return;
    }
#endif

#if FIGHTPAD12SLIM_AMBIENT_GPIO40_INIT_DIAGNOSTIC
    if (!FIGHTPAD12SLIM_AMBIENT_ENABLED && isValidPin(FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN)) {
        gpio_init(FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN);
        gpio_set_dir(FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN, GPIO_OUT);
        gpio_put(FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN, 0);
    }
#endif

#if FIGHTPAD12SLIM_AMBIENT_PIO_SETUP_DIAGNOSTIC
    if (!FIGHTPAD12SLIM_AMBIENT_ENABLED && isValidPin(FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN)) {
#if FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE
        pio_set_gpio_base(FIGHTPAD12SLIM_AMBIENT_PIO, FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE);
#endif
        neopico.Setup(
            FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN,
            FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT,
            static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT),
            FIGHTPAD12SLIM_AMBIENT_PIO,
            FIGHTPAD12SLIM_AMBIENT_PIO_SM);
#if FIGHTPAD12SLIM_AMBIENT_ZERO_SHOW_DIAGNOSTIC
        neopico.Show();
#endif
    }
#endif

#if FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_DIAGNOSTIC
    if (FIGHTPAD12SLIM_AMBIENT_ENABLED && isValidPin(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN)) {
        gpio_init(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN);
        gpio_set_dir(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN, GPIO_OUT);
        fightpadAmbientDiagDataOut = 0;
        gpio_put(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN, 0);
        lastControls = readControls();
        fightpadAmbientDiagControls = lastControls;
        return;
    }
#endif

    if (!FIGHTPAD12SLIM_AMBIENT_ENABLED) {
        lastControls = readControls();
        fightpadAmbientDiagControls = lastControls;
        return;
    }

#if FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE
    pio_set_gpio_base(FIGHTPAD12SLIM_AMBIENT_PIO, FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE);
#endif

    neopico.Setup(
        FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN,
        FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT,
        static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT),
        FIGHTPAD12SLIM_AMBIENT_PIO,
        FIGHTPAD12SLIM_AMBIENT_PIO_SM);

    neopico_gp22.Setup(
        BOARD_LEDS_PIN,
        FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT,
        static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT),
        FIGHTPAD12SLIM_AMBIENT_PIO,
        FIGHTPAD12SLIM_AMBIENT_GP22_PIO_SM);

    lastControls = readControls();
#if FIGHTPAD12SLIM_AMBIENT_DIP_SELECTOR_MODE
    lastSelector = lastControls & (CONTROL_ONOFF | CONTROL_PREV | CONTROL_NEXT);
    applySelectorState(lastSelector, enabled, effectIndex);
#else
    enabled = g_menuRgbPowerEnabled;
#endif

    fightpadAmbientDiagEnabled = enabled ? 1 : 0;
    fightpadAmbientDiagEffect = effectIndex;
    render(getMillis());
    show();

#if FIGHTPAD12SLIM_AMBIENT_STARTUP_BLINK_DIAGNOSTIC
    for (int i = 0; i < 6; i++) {
        clearFrame();
        if ((i % 2) == 0) {
            fill(ColorRed, FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);
        }
        show();
        sleep_ms(250);
    }

    render(getMillis());
    show();
#endif
}

void FightpadAmbientLEDAddon::process() {
    uint32_t now = getMillis();
    updateButtonFlash(now);
#if !FIGHTPAD12SLIM_AMBIENT_DIP_SELECTOR_MODE
    enabled = g_menuRgbPowerEnabled;
#endif
    fightpadAmbientDiagEnabled = enabled ? 1 : 0;
    fightpadAmbientDiagEffect = effectIndex;
    fightpadAmbientDiagTicks++;

    if (!FIGHTPAD12SLIM_AMBIENT_ENABLED) {
        return;
    }

#if FIGHTPAD12SLIM_AMBIENT_CONTROL_DIAGNOSTIC
    {
        uint8_t rawControls = readRawControlBits();
        uint8_t activeHighControls = rawControls;
        uint8_t activeLowControls = (~rawControls) & 0x07;

        fightpadAmbientDiagControls = activeLowControls;
        clearFrame();

        uint32_t lowerValue = makeControlDiagnosticColor(activeHighControls).value(
            static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT),
            FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);

        for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT; led++) {
            frame[led] = lowerValue;
        }

        for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT; led++) {
            frame_gp22[led] = lowerValue;
        }

        show();
        lastFrameTime = now;
        return;
    }
#endif

#if FIGHTPAD12SLIM_AMBIENT_POWER_ONLY_DIAGNOSTIC
    return;
#endif

#if FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_DIAGNOSTIC
    if (isValidPin(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN)) {
        if ((now - lastFrameTime) >= FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_MS) {
            fightpadAmbientDiagDataOut ^= 1;
            gpio_put(FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN, fightpadAmbientDiagDataOut != 0);
            fightpadAmbientDiagShows++;
            lastFrameTime = now;
        }
        return;
    }
#endif

    // LED rendering: driven exclusively by g_menuRgb* variables
    // (set via scrollwheel menu, persisted to flash).  No DIP-switch
    // color cycling — GPIO30-32 belong to the scrollwheel encoder now.
    if ((now - lastFrameTime) < UPDATE_INTERVAL_MS) {
        return;
    }

    render(now);
    show();
    lastFrameTime = now;
}

uint8_t FightpadAmbientLEDAddon::readControls() {
    uint8_t controls = 0;

    // GP30 is read unconditionally — the release-edge detection in
    // handleControlEdges() together with the g_scrollWheelMenuActive
    // guard in process() is sufficient to prevent long-press LED toggle.
    if (isPressed(FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN)) {
        controls |= CONTROL_ONOFF;
    }

    if (isPressed(FIGHTPAD12SLIM_AMBIENT_PREV_PIN)) {
        controls |= CONTROL_PREV;
    }

    if (isPressed(FIGHTPAD12SLIM_AMBIENT_NEXT_PIN)) {
        controls |= CONTROL_NEXT;
    }

    return controls;
}

bool FightpadAmbientLEDAddon::isPressed(int pin) {
    if (!isValidPin(pin)) {
        return false;
    }
#if FIGHTPAD12SLIM_AMBIENT_CONTROLS_ACTIVE_LOW
    return !gpio_get(pin);
#else
    return gpio_get(pin);
#endif
}

void FightpadAmbientLEDAddon::handleControlEdges(uint8_t controls, uint32_t now) {
#if FIGHTPAD12SLIM_AMBIENT_DIP_SELECTOR_MODE
    uint8_t selector = controls & (CONTROL_ONOFF | CONTROL_PREV | CONTROL_NEXT);
    if (selector != lastSelector && (now - lastControlTime) >= CONTROL_DEBOUNCE_MS) {
        // DIP selector mode: 3-bit direct effect select, 000 = OFF, 001 = effect 0.
        applySelectorState(selector, enabled, effectIndex);
        lastSelector = selector;
        fightpadAmbientDiagEnabled = enabled ? 1 : 0;
        fightpadAmbientDiagEffect = effectIndex;
        lastControlTime = now;
        render(now);
        show();
        lastFrameTime = now;
    }
    return;
#endif

    uint8_t pressed  = controls & ~lastControls;
    uint8_t released = lastControls & ~controls;

    // Prefer release edges for ON/OFF (GP30), so a long press that
    // activates the scrollwheel menu never toggles the LED on its
    // rising edge before g_scrollWheelMenuActive becomes true.
    // Also suppress ON/OFF when the release follows a long press
    // (menu enter or exit) — g_scrollWheelButtonLongPressed is set
    // at the 3 s mark and remains true until the button is released.
    extern volatile bool g_scrollWheelButtonLongPressed;
    bool hasOnOff = (released & CONTROL_ONOFF) != 0
                    && !g_scrollWheelButtonLongPressed;

    // PREV/NEXT stay on press edge for responsive effect cycling.
    bool hasPrevNext = (pressed & (CONTROL_PREV | CONTROL_NEXT)) != 0;

    if (!hasOnOff && !hasPrevNext) {
        return;
    }

    if ((now - lastControlTime) < CONTROL_DEBOUNCE_MS) {
        return;
    }

    if (hasOnOff) {
        enabled = !enabled;
    }

    if (pressed & CONTROL_PREV) {
        previousEffect();
    }

    if (pressed & CONTROL_NEXT) {
        nextEffect();
    }

    fightpadAmbientDiagEnabled = enabled ? 1 : 0;
    fightpadAmbientDiagEffect = effectIndex;
    lastControlTime = now;
    render(now);
    show();
    lastFrameTime = now;
}

void FightpadAmbientLEDAddon::updateButtonFlash(uint32_t now) {
    Gamepad * gamepad = Storage::getInstance().GetGamepad();
    if (gamepad == nullptr) {
        return;
    }

    uint32_t currentButtons = gamepad->state.buttons;
    uint8_t currentDpad = gamepad->state.dpad;
    uint32_t newButtons = currentButtons & ~lastGamepadButtons;
    uint8_t newDpad = currentDpad & ~lastGamepadDpad;
    uint32_t flashUntil = now + BUTTON_FLASH_MS;

    if (newButtons & GAMEPAD_MASK_B1) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_B1, flashUntil);
    if (newButtons & GAMEPAD_MASK_B2) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_B2, flashUntil);
    if (newButtons & GAMEPAD_MASK_B3) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_B3, flashUntil);
    if (newButtons & GAMEPAD_MASK_B4) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_B4, flashUntil);
    if (newButtons & GAMEPAD_MASK_L1) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_L1, flashUntil);
    if (newButtons & GAMEPAD_MASK_R1) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_R1, flashUntil);
    if (newButtons & GAMEPAD_MASK_L2) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_L2, flashUntil);
    if (newButtons & GAMEPAD_MASK_R2) setFlashUntil(gp22FlashUntil, LEDS_BUTTON_R2, flashUntil);
    if (newDpad & GAMEPAD_MASK_UP) setFlashUntil(gp22FlashUntil, LEDS_DPAD_UP, flashUntil);
    if (newDpad & GAMEPAD_MASK_DOWN) setFlashUntil(gp22FlashUntil, LEDS_DPAD_DOWN, flashUntil);
    if (newDpad & GAMEPAD_MASK_LEFT) setFlashUntil(gp22FlashUntil, LEDS_DPAD_LEFT, flashUntil);
    if (newDpad & GAMEPAD_MASK_RIGHT) setFlashUntil(gp22FlashUntil, LEDS_DPAD_RIGHT, flashUntil);

    lastGamepadButtons = currentButtons;
    lastGamepadDpad = currentDpad;
}

void FightpadAmbientLEDAddon::previousEffect() {
#if FIGHTPAD12SLIM_AMBIENT_PIXEL_SCAN_DIAGNOSTIC
    effectIndex = (effectIndex == 0) ? (FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT - 1) : (effectIndex - 1);
#else
    effectIndex = (effectIndex == 0) ? (EFFECT_COUNT - 1) : (effectIndex - 1);
#endif
}

void FightpadAmbientLEDAddon::nextEffect() {
#if FIGHTPAD12SLIM_AMBIENT_PIXEL_SCAN_DIAGNOSTIC
    effectIndex = (effectIndex + 1) % FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT;
#else
    effectIndex = (effectIndex + 1) % EFFECT_COUNT;
#endif
}

void FightpadAmbientLEDAddon::render(uint32_t now) {
    bluetoothStatusLightRequired = false;
    clearFrame();

    // Low-battery protection is a transient render override.  It does not
    // change the user's enabled/effect/color settings, so the current effect
    // resumes automatically after a valid SOC sample rises above the cutoff.
    if (FightpadBQ27220BatteryAddon::isLowBatteryLightCutoffActive()) {
        return;
    }

    FightpadESP32BluetoothStatusEvent bluetoothEvent = {};
    const bool bluetoothStatusActive =
        getFightpadESP32BluetoothStatusEvent(bluetoothEvent) &&
        isFightpadESP32BluetoothStatusEventActive(bluetoothEvent, now);

    if (!enabled && !bluetoothStatusActive) {
        return;
    }

    // Bluetooth status is a transient GP40-only render override.  It does not
    // alter menu/Flash values or the normal Base animation state.  GP22 keeps
    // rendering normally when RGB is enabled and remains black during All OFF.
    if (bluetoothStatusActive) {
        renderBluetoothStatusAmbient(
            static_cast<uint8_t>(bluetoothEvent.status),
            now - bluetoothEvent.receivedAtMs);
        bluetoothStatusLightRequired =
            bluetoothEvent.status != FightpadESP32BluetoothStatus::Disconnected;
        if (enabled) {
            renderButtons(now);
        }
        return;
    }

#if FIGHTPAD12SLIM_AMBIENT_FORCE_SOLID_DIAGNOSTIC
    fill(ColorRed, FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);
    return;
#elif FIGHTPAD12SLIM_AMBIENT_PIXEL_SCAN_DIAGNOSTIC
    uint8_t led = effectIndex % FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT;
    frame[led] = ColorWhite.value(
        static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT),
        FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);
    if (led < FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT) {
        frame_gp22[led] = ColorWhite.value(
            static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT),
            FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);
    }
    return;
#endif

#if FIGHTPAD12SLIM_AMBIENT_RENDER_TOGGLE_DIAGNOSTIC
    renderCounter++;
    if ((renderCounter & 1U) != 0) {
        fill(ColorRed, FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);
    }
    return;
#endif

    // ── Normal render path ──────────────────────────────────────────────
    renderAmbient(now);
    renderButtons(now);
}

void FightpadAmbientLEDAddon::renderBluetoothStatusAmbient(
    uint8_t statusValue,
    uint32_t elapsedMs)
{
    const auto status = static_cast<FightpadESP32BluetoothStatus>(statusValue);
    LEDFormat fmt = static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT);
    const uint8_t count = FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT;

    switch (status) {
    case FightpadESP32BluetoothStatus::Connecting:
    case FightpadESP32BluetoothStatus::Pairing:
        {
#if FIGHTPAD12SLIM_ESP32_BT_STATUS_DUAL_CHASE
            const uint8_t chaseHead = static_cast<uint8_t>((elapsedMs / 50U) % count);
            const uint8_t chaseHeads[2] = {
                chaseHead,
                static_cast<uint8_t>((chaseHead + count / 2U) % count),
            };
            static const float tailBrightness[3] = {0.80f, 0.25f, 0.05f};
            for (uint8_t segment = 0; segment < 2; segment++) {
                for (uint8_t i = 0; i < 3; i++) {
                    const uint8_t index = static_cast<uint8_t>(
                        (chaseHeads[segment] + count - i) % count);
                    frame[index] = ColorBlue.value(fmt, tailBrightness[i]);
                }
            }
#else
            const uint8_t chasePixel = static_cast<uint8_t>((elapsedMs / 50U) % count);
            static const float gradient[5] = {0.05f, 0.25f, 0.80f, 0.25f, 0.05f};
            for (uint8_t i = 0; i < 5; i++) {
                const uint8_t index = static_cast<uint8_t>((chasePixel + i) % count);
                frame[index] = ColorBlue.value(fmt, gradient[i]);
            }
#endif
        }
        break;

    case FightpadESP32BluetoothStatus::Connected:
        {
            const uint32_t value = ColorBlue.value(fmt, getMenuEffectBrightness());
            for (uint8_t i = 0; i < count; i++) {
                frame[i] = value;
            }
        }
        break;

    case FightpadESP32BluetoothStatus::Disconnected:
    default:
        // clearFrame() already made the Base chain black.
        break;
    }
}

// ── Ambient LED effects (GP40, 19 LEDs) ───────────────────────────────

// Static theme rows — same as neopicoleds.cpp alCustomStaticTheme.
// Row index = themeIndex (0-4), column = color within the row (0-7).
static const RGB kAmbientThemeRows[5][8] = {
    {ColorRed,    ColorOrange, ColorYellow, ColorGreen,
     ColorBlue,   ColorIndigo, ColorViolet, ColorWhite},
    {ColorOrange, ColorRed,    ColorGreen,  ColorYellow,
     ColorIndigo, ColorBlue,   ColorWhite,  ColorViolet},
    {ColorYellow, ColorOrange, ColorRed,    ColorIndigo,
     ColorBlue,   ColorGreen,  ColorViolet, ColorWhite},
    {ColorGreen,  ColorOrange, ColorYellow, ColorRed,
     ColorWhite,  ColorIndigo, ColorViolet, ColorBlue},
    {ColorWhite,  ColorIndigo, ColorViolet, ColorOrange,
     ColorBlue,   ColorGreen,  ColorYellow, ColorRed},
};

void FightpadAmbientLEDAddon::renderAmbient(uint32_t now) {
    extern volatile uint8_t g_menuRgbBottom;
    extern volatile uint8_t g_menuAmbientEffect;

    // Base color (menu override or default white)
    RGB baseColor = (g_menuRgbBottom != 0xFF)
        ? menuIndexToColor(g_menuRgbBottom)
        : ColorWhite;
    LEDFormat fmt = static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT);
    const uint8_t count = FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT; // 19
    const float effectBrightness = getMenuEffectBrightness();

    uint8_t effect = g_menuAmbientEffect;
    // 0xFF = never set by menu → use default static color.
    if (effect == 0xFF) effect = 0;

    switch (effect) {
    default:
    case 0: // AL_CUSTOM_EFFECT_STATIC_COLOR — fixed selected color
        {
            uint32_t v = baseColor.value(fmt, effectBrightness);
            for (int i = 0; i < count; i++) frame[i] = v;
        }
        break;

    case 1: // AL_CUSTOM_EFFECT_GRADIENT — all LEDs same shifting rainbow
        {
            RGB wc = RGB::wheel(static_cast<uint8_t>(wheelFrame));
            uint32_t v = wc.value(fmt, effectBrightness);
            for (int i = 0; i < count; i++) frame[i] = v;

            // Advance & bounce wheel
            if (wheelReverse) {
                wheelFrame -= 2;
                if (wheelFrame < 0) { wheelFrame = 1; wheelReverse = false; }
            } else {
                wheelFrame += 2;
                if (wheelFrame > 255) { wheelFrame = 254; wheelReverse = true; }
            }
        }
        break;

    case 2: // AL_CUSTOM_EFFECT_CHASE — 5 lit LEDs running around the chain
        {
            // Advance chase head every ~200 ms
            if (now - ambientChaseLastMs >= 200) {
                ambientChaseLastMs = now;
                ambientChasePixel++;
                if (ambientChasePixel >= count) ambientChasePixel = 0;
                wheelFrame = static_cast<int16_t>((wheelFrame + 8) & 0xFF);
            }
            // All off first
            for (int i = 0; i < count; i++) frame[i] = 0;
            // Light 5 consecutive LEDs with a symmetric brightness gradient using
            // dynamic wheel colors: edges dim, center bright.
            static const float grad[5] = {0.05f, 0.25f, 0.80f, 0.25f, 0.05f};
            for (int i = 0; i < 5; i++) {
                int idx = (ambientChasePixel + i) % count;
                frame[idx] = chaseColorFor(idx, count, wheelFrame).value(fmt, grad[i]);
            }
        }
        break;

    case 3: // AL_CUSTOM_EFFECT_BREATHING_RAINBOW — brightness oscillation with color cycling
        {
            // Reset breath state on effect entry: always start dim→bright.
            if (lastAmbientEffect != 3) {
                breathBrightness  = 0.0f;
                breathDimming     = false;
                breathColorCycle  = 0;
                lastAmbientEffect = 3;
            }
            // Oscillate brightness (slow, smooth breathing)
            // The peak is halved to 0.5f, so halve the step as well to keep
            // the legacy breathing rhythm close to its previous speed.
            const float breathSpeed = 0.004f;
            if (breathDimming) {
                breathBrightness -= breathSpeed;
                if (breathBrightness <= 0.0f) {
                    breathBrightness = 0.0f;
                    breathDimming = false;
                    breathColorCycle++;
                }
            } else {
                breathBrightness += breathSpeed;
                if (breathBrightness >= 0.5f) {
                    breathBrightness = 0.5f;
                    breathDimming = true;
                    breathColorCycle++;
                }
            }

            // Cycle through 4 colors, 2 full breath cycles each
            RGB bc;
            if (breathColorCycle <= 1)
                bc = ColorMagenta;
            else if (breathColorCycle <= 3)
                bc = ColorRed;
            else if (breathColorCycle <= 5)
                bc = ColorGreen;
            else if (breathColorCycle <= 7)
                bc = ColorBlue;
            else
                { breathColorCycle = 0; bc = ColorMagenta; }

            uint32_t v = bc.value(fmt, breathBrightness);
            for (int i = 0; i < count; i++) frame[i] = v;
        }
        break;

    case 4: // AL_CUSTOM_EFFECT_RAINBOW — each LED a different rainbow phase
        {
            for (int i = 0; i < count; i++) {
                uint8_t phase = static_cast<uint8_t>(
                    wheelFrame + ((i * 256U) / count));
                RGB c = RGB::wheel(phase);
                frame[i] = c.value(fmt, effectBrightness);
            }
            // Advance & bounce wheel, matching Key Effect Rainbow.
            if (wheelReverse) {
                wheelFrame -= 1;
                if (wheelFrame < 0) { wheelFrame = 1; wheelReverse = false; }
            } else {
                wheelFrame += 1;
                if (wheelFrame > 255) { wheelFrame = 254; wheelReverse = true; }
            }
        }
        break;

    case 5: // AL_CUSTOM_EFFECT_BREATHING_COLOR — selected color breathing
        {
            float b = getBreathBrightness(now);
            uint32_t v = baseColor.value(fmt, b);
            for (int i = 0; i < count; i++) frame[i] = v;
        }
        break;
    }
    lastAmbientEffect = effect;
}

// ── Button LED effects (GP22, 12 LEDs) ─────────────────────────────────

void FightpadAmbientLEDAddon::renderButtons(uint32_t now) {
    extern volatile uint8_t g_menuRgbTop;
    extern volatile uint8_t g_menuRgbButton;
    extern volatile uint8_t g_menuButtonEffect;

    // Base color (menu override or default white)
    RGB baseColor = (g_menuRgbTop != 0xFF)
        ? menuIndexToColor(g_menuRgbTop)
        : ColorWhite;
    // Flash color (menu override or default white)
    RGB flashColor = (g_menuRgbButton != 0xFF)
        ? menuIndexToColor(g_menuRgbButton)
        : ColorWhite;
    LEDFormat fmt = static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT);
    const uint8_t count = FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT; // 12
    const float effectBrightness = getMenuEffectBrightness();

    uint32_t flashV = flashColor.value(fmt, 0.8f);

    uint8_t effect = g_menuButtonEffect;
    // 0xFF = never set by menu → use default static color.
    if (effect == 0xFF) effect = 0;

    switch (effect) {
    default:
    case 0: // EFFECT_STATIC_COLOR — fixed selected color + per-LED flash
        {
            uint32_t baseV = baseColor.value(fmt, effectBrightness);
            for (int i = 0; i < count; i++) {
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : baseV;
            }
        }
        break;

    case 1: // EFFECT_RAINBOW — each LED a different rainbow phase, fixed brightness
        {
            for (int i = 0; i < count; i++) {
                // Space LEDs across the color wheel
                uint8_t phase = static_cast<uint8_t>(wheelFrame + i * 21);
                RGB c = RGB::wheel(phase);
                uint32_t baseV = c.value(fmt, effectBrightness);
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : baseV;
            }
            // Advance & bounce wheel
            if (wheelReverse) {
                wheelFrame -= 1;
                if (wheelFrame < 0) { wheelFrame = 1; wheelReverse = false; }
            } else {
                wheelFrame += 1;
                if (wheelFrame > 255) { wheelFrame = 254; wheelReverse = true; }
            }
        }
        break;

    case 2: // EFFECT_CHASE — 3 adjacent LEDs lit, brightness gradient
        {
            // Advance chase bright lead point every ~200 ms
            if (now - buttonChaseLastMs >= 200) {
                buttonChaseLastMs = now;
                buttonChasePixel++;
                if (buttonChasePixel >= count) buttonChasePixel = 0;
                wheelFrame = static_cast<int16_t>((wheelFrame + 8) & 0xFF);
            }
            // Base: all off, but flash overlays win
            for (int i = 0; i < count; i++) {
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : 0;
            }
            // New LEDs enter bright, then fade as the chase moves forward.
            static const float grad[3] = {0.60f, 0.25f, 0.05f};
            for (int i = 0; i < 3; i++) {
                int idx = (buttonChasePixel + count - i) % count;
                uint32_t cv = chaseColorFor(idx, count, wheelFrame).value(fmt, grad[i]);
                frame_gp22[idx] = (now < gp22FlashUntil[idx]) ? flashV : cv;
            }
        }
        break;

    case 3: // EFFECT_STATIC_THEME — legacy hidden menu option
        {
            constexpr int COLS = 8;
            int themeIdx = 0;
            for (int i = 0; i < count; i++) {
                uint32_t baseV = kAmbientThemeRows[themeIdx][i % COLS]
                    .value(fmt, 0.5f);
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : baseV;
            }
        }
        break;

    case 4: // EFFECT_BREATHING — selected color breathing + per-LED flash
        {
            float b = getBreathBrightness(now);
            uint32_t baseV = baseColor.value(fmt, b);
            for (int i = 0; i < count; i++) {
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : baseV;
            }
        }
        break;

    case 5: // EFFECT_BREATHING_RAINBOW — color cycling breathing + per-LED flash
        {
            float b = getBreathBrightness(now);
            uint8_t phase = static_cast<uint8_t>((now / 40U) & 0xFFU);
            RGB c = RGB::wheel(phase);
            uint32_t baseV = c.value(fmt, b);
            for (int i = 0; i < count; i++) {
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : baseV;
            }
        }
        break;

    case 6: // EFFECT_GRADIENT — all LEDs same shifting rainbow + per-LED flash
        {
            RGB c = RGB::wheel(static_cast<uint8_t>(buttonGradientFrame));
            uint32_t baseV = c.value(fmt, effectBrightness);
            for (int i = 0; i < count; i++) {
                frame_gp22[i] = (now < gp22FlashUntil[i]) ? flashV : baseV;
            }

            // Match Base Gradient's step and bounce without sharing its phase state.
            if (buttonGradientReverse) {
                buttonGradientFrame -= 2;
                if (buttonGradientFrame < 0) {
                    buttonGradientFrame = 1;
                    buttonGradientReverse = false;
                }
            } else {
                buttonGradientFrame += 2;
                if (buttonGradientFrame > 255) {
                    buttonGradientFrame = 254;
                    buttonGradientReverse = true;
                }
            }
        }
        break;

    }
}

void FightpadAmbientLEDAddon::show() {
    // Final writer guard: setup/diagnostic paths can call show() without first
    // passing through render().  A disabled menu request and the <=7% battery
    // cutoff both send one final black frame before GP24 is pulled inactive.
    const bool outputEnabled = (enabled || bluetoothStatusLightRequired) &&
        !FightpadBQ27220BatteryAddon::isLowBatteryLightCutoffActive();
    if (!outputEnabled) {
        clearFrame();
    }

    // Once the rail is off there is nothing to transmit.  When a mode is
    // selected again, power the RGB controller first and let its supply settle
    // before the first restored frame is sent.
    if (!boostPowerEnabled) {
        if (!outputEnabled) {
            return;
        }
        setBoostPower(true);
    }

    neopico.SetFrame(frame);
    neopico.Show();

    neopico_gp22.SetFrame(frame_gp22);
    neopico_gp22.Show();

    fightpadAmbientDiagShows++;

    if (!outputEnabled) {
#if FIGHTPAD12SLIM_AMBIENT_DRIVE_BOOST_EN && \
    FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF && \
    FIGHTPAD12SLIM_AMBIENT_BOOST_SHUTDOWN_DELAY_US > 0
        // NeoPico::Show() blocks while filling the PIO FIFO, but the final few
        // pixels can still be shifting.  Keep the rail alive long enough for
        // both black frames and the WS2812 latch interval to complete.
        sleep_us(FIGHTPAD12SLIM_AMBIENT_BOOST_SHUTDOWN_DELAY_US);
#endif
        setBoostPower(false);
    }
}

void FightpadAmbientLEDAddon::setBoostPower(bool powerEnabled) {
#if FIGHTPAD12SLIM_AMBIENT_DRIVE_BOOST_EN && FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF
    if (FIGHTPAD12SLIM_AMBIENT_ENABLED && isValidPin(FIGHTPAD12SLIM_BOOST_EN_PIN)) {
#if FIGHTPAD12SLIM_AMBIENT_BOOST_EXTERNAL_PULLUP
        if (powerEnabled) {
            gpio_set_dir(FIGHTPAD12SLIM_BOOST_EN_PIN, GPIO_IN);
            gpio_disable_pulls(FIGHTPAD12SLIM_BOOST_EN_PIN);
        } else {
            gpio_put(FIGHTPAD12SLIM_BOOST_EN_PIN,
                FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL ? 0 : 1);
            gpio_set_dir(FIGHTPAD12SLIM_BOOST_EN_PIN, GPIO_OUT);
        }
#else
        gpio_put(FIGHTPAD12SLIM_BOOST_EN_PIN,
            powerEnabled
                ? (FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL ? 1 : 0)
                : (FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL ? 0 : 1));
#endif
        boostPowerEnabled = powerEnabled;
        if (powerEnabled) {
#if FIGHTPAD12SLIM_AMBIENT_BOOST_STARTUP_DELAY_MS > 0
            sleep_ms(FIGHTPAD12SLIM_AMBIENT_BOOST_STARTUP_DELAY_MS);
#endif
        }
        return;
    }
#endif

    // Boards without an independently controlled LED rail retain the original
    // frame-only OFF behaviour.
    boostPowerEnabled = true;
}

void FightpadAmbientLEDAddon::fill(RGB color, float brightness) {
    uint32_t value = color.value(static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT), brightness);
    for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT; led++) {
        frame[led] = value;
    }

    for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT; led++) {
        frame_gp22[led] = value;
    }
}

float FightpadAmbientLEDAddon::getBreathBrightness(uint32_t now) {
    // Keep the existing 2.4-second rhythm while limiting peak brightness.
    const uint32_t cycle_ms = 2400;
    uint32_t phase = now % cycle_ms;
    float phase_rad = (phase / (float)cycle_ms) * 2.0f * 3.14159265f;
    float sine_val = sinf(phase_rad);
    float brightness = 0.25f + (sine_val * 0.25f);

    if (brightness < 0.02f) {
        brightness = 0.02f;
    }

    return brightness;
}

void FightpadAmbientLEDAddon::clearFrame() {
    for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT; led++) {
        frame[led] = 0;
    }

    for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT; led++) {
        frame_gp22[led] = 0;
    }
}
