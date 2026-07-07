#include "addons/fightpad_ambient_leds.h"

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
// Maps kMenuColors index to an RGB constant.
// 0 = OFF (black), 1 = Red .. 8 = White.
static RGB menuIndexToColor(uint8_t idx) {
    if (idx == 0) return RGB(0, 0, 0);  // OFF
    static const RGB colors[] = {
        ColorRed,       // 1: Red
        ColorOrange,    // 2: Orange
        ColorYellow,    // 3: Yellow
        ColorGreen,     // 4: Green
        ColorAqua,      // 5: Cyan
        ColorBlue,      // 6: Blue
        ColorPurple,    // 7: Purple
        ColorWhite,     // 8: White
    };
    constexpr uint8_t count = sizeof(colors) / sizeof(colors[0]);
    uint8_t shifted = idx - 1;
    return (shifted < count) ? colors[shifted] : ColorWhite;
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
    uint8_t controls = readControls();
    updateButtonFlash(now);
    fightpadAmbientDiagControls = controls;
    fightpadAmbientDiagEnabled = enabled ? 1 : 0;
    fightpadAmbientDiagEffect = effectIndex;
    fightpadAmbientDiagTicks++;

    if (!FIGHTPAD12SLIM_AMBIENT_ENABLED) {
        lastControls = controls;
        return;
    }

    // When scrollwheel menu is active, skip DIP control logic
    // but continue LED rendering below. This lets the menu take
    // over GPIO30-32 while keeping ambient lighting visible.
    extern volatile bool g_scrollWheelMenuActive;
    if (g_scrollWheelMenuActive) {
        lastControls = controls;
        // Still run LED rendering
        if ((now - lastFrameTime) >= UPDATE_INTERVAL_MS) {
            render(now);
            show();
            lastFrameTime = now;
        }
        return;
    }

#if FIGHTPAD12SLIM_AMBIENT_CONTROL_DIAGNOSTIC
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
    lastControls = activeLowControls;
    lastFrameTime = now;
    return;
#endif

#if FIGHTPAD12SLIM_AMBIENT_POWER_ONLY_DIAGNOSTIC
    lastControls = controls;
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
        lastControls = controls;
        return;
    }
#endif

    handleControlEdges(controls, now);
    lastControls = controls;

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
    clearFrame();

    if (!enabled) {
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
    float brightness = getBreathBrightness(now);

    // Bottom board (GP40, 19 LEDs): menu override or DIP cycling color
    extern volatile uint8_t g_menuRgbBottom;
    RGB bottomColor = (g_menuRgbBottom != 0xFF)
        ? menuIndexToColor(g_menuRgbBottom)
        : effectColors[effectIndex % EFFECT_COUNT];

    // Top board (GP22, 12 LEDs): menu override or DIP cycling color
    extern volatile uint8_t g_menuRgbTop;
    RGB topColor = (g_menuRgbTop != 0xFF)
        ? menuIndexToColor(g_menuRgbTop)
        : effectColors[effectIndex % EFFECT_COUNT];

    // Button flash color: menu override or default white
    extern volatile uint8_t g_menuRgbButton;
    RGB flashColor = (g_menuRgbButton != 0xFF)
        ? menuIndexToColor(g_menuRgbButton)
        : ColorWhite;

    uint32_t flashValue = flashColor.value(
        static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT), 1.0f);

    // Fill bottom frame (GP40)
    uint32_t bottomValue = bottomColor.value(
        static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT), brightness);
    for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT; led++) {
        frame[led] = bottomValue;
    }

    // Fill top frame (GP22): breathing color base, flash on top
    uint32_t topValue = topColor.value(
        static_cast<LEDFormat>(FIGHTPAD12SLIM_AMBIENT_LED_FORMAT), brightness);
    for (int led = 0; led < FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT; led++) {
        if (now < gp22FlashUntil[led]) {
            frame_gp22[led] = flashValue;
        } else {
            frame_gp22[led] = topValue;
        }
    }
}

void FightpadAmbientLEDAddon::show() {
    neopico.SetFrame(frame);
    neopico.Show();

    neopico_gp22.SetFrame(frame_gp22);
    neopico_gp22.Show();

    fightpadAmbientDiagShows++;
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
    // Keep the breathing range wide enough to remain obvious on hardware.
    const uint32_t cycle_ms = 2400;
    uint32_t phase = now % cycle_ms;
    float phase_rad = (phase / (float)cycle_ms) * 2.0f * 3.14159265f;
    float sine_val = sinf(phase_rad);
    float brightness = 0.5f + (sine_val * 0.5f);

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
