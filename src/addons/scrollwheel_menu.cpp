#include "addons/scrollwheel_menu.h"
#include "gamepad.h"
#include "helper.h"

#include <cstdio>
#include <cstring>

// ── Menu data tables ─────────────────────────────────────────────────────

const SWMenuItem kMenuMain[] = {
    { "RP2350B FW Version", SWMenuLevel::INFO, 0 },
    { "ESP32C6 Status",     SWMenuLevel::INFO, 0 },
    { "RGB Customize",      SWMenuLevel::RGB_SUB, 0 },
};
const uint8_t kMenuMainCount = sizeof(kMenuMain) / sizeof(kMenuMain[0]);

const SWMenuItem kMenuRgbSub[] = {
    { "Top Board RGB",    SWMenuLevel::COLOR, 0 },
    { "Bottom Board RGB", SWMenuLevel::COLOR, 0 },
    { "Button RGB",       SWMenuLevel::COLOR, 0 },
};
const uint8_t kMenuRgbSubCount = sizeof(kMenuRgbSub) / sizeof(kMenuRgbSub[0]);

const SWMenuItem kMenuColors[] = {
    { "Red",    SWMenuLevel::INFO, 0 },
    { "Orange", SWMenuLevel::INFO, 0 },
    { "Yellow", SWMenuLevel::INFO, 0 },
    { "Green",  SWMenuLevel::INFO, 0 },
    { "Cyan",   SWMenuLevel::INFO, 0 },
    { "Blue",   SWMenuLevel::INFO, 0 },
    { "Purple", SWMenuLevel::INFO, 0 },
    { "White",  SWMenuLevel::INFO, 0 },
};
const uint8_t kMenuColorsCount = sizeof(kMenuColors) / sizeof(kMenuColors[0]);

// ── Cross-core state ─────────────────────────────────────────────────────

volatile ScrollWheelMenuState g_menuState = { false, 0, 0, 0 };
volatile bool g_menuStateDirty = false;
volatile bool g_scrollWheelMenuActive = false;
volatile bool g_scrollWheelButtonBusy = false;

// ── Pin helpers ──────────────────────────────────────────────────────────

void ScrollWheelMenuAddon::initPin(int pin) {
    if (!isValidPin(pin)) return;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_set_input_enabled(pin, true);
    gpio_pull_up(pin);
}

bool ScrollWheelMenuAddon::readPin(int pin) const {
    if (!isValidPin(pin)) return false;
#if SCROLLWHEEL_ACTIVE_LOW
    return !gpio_get(pin);
#else
    return gpio_get(pin);
#endif
}

// ── GPAddon interface ─────────────────────────────────────────────────────

bool ScrollWheelMenuAddon::available() {
    return SCROLLWHEEL_MENU_ENABLED &&
           isValidPin(SCROLLWHEEL_PIN_SW) &&
           isValidPin(SCROLLWHEEL_PIN_A) &&
           isValidPin(SCROLLWHEEL_PIN_B);
}

void ScrollWheelMenuAddon::setup() {
    initPin(SCROLLWHEEL_PIN_SW);
    initPin(SCROLLWHEEL_PIN_A);
    initPin(SCROLLWHEEL_PIN_B);

    prevA = readPin(SCROLLWHEEL_PIN_A);
    prevB = readPin(SCROLLWHEEL_PIN_B);
    btnState       = BTN_IDLE;
    btnTimer        = 0;
    debouncedButton = false;
    btnFromLong     = false;

    g_menuState.active = false;
    g_menuState.level = 0;
    g_menuState.index = 0;
    g_menuState.scrollOffset = 0;
    g_menuStateDirty = false;
    g_scrollWheelMenuActive = false;

    printf("[ScrollWheel] Setup OK. Pins: SW=%d A=%d B=%d\n",
           SCROLLWHEEL_PIN_SW, SCROLLWHEEL_PIN_A, SCROLLWHEEL_PIN_B);
}

// ── Menu helpers ─────────────────────────────────────────────────────────

const SWMenuItem* ScrollWheelMenuAddon::currentMenuTable() const {
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:    return kMenuMain;
        case SWMenuLevel::RGB_SUB: return kMenuRgbSub;
        case SWMenuLevel::COLOR:   return kMenuColors;
        default:                   return kMenuMain;
    }
}

uint8_t ScrollWheelMenuAddon::currentItemCount() const {
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:    return kMenuMainCount;
        case SWMenuLevel::RGB_SUB: return kMenuRgbSubCount;
        case SWMenuLevel::COLOR:   return kMenuColorsCount;
        default:                   return kMenuMainCount;
    }
}

static void markMenuDirty() {
    g_menuStateDirty = true;
}

static void clampScrollOffset() {
    uint8_t count = 0;
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:    count = kMenuMainCount; break;
        case SWMenuLevel::RGB_SUB: count = kMenuRgbSubCount; break;
        case SWMenuLevel::COLOR:   count = kMenuColorsCount; break;
        default: return;
    }
    if (g_menuState.scrollOffset > g_menuState.index)
        g_menuState.scrollOffset = g_menuState.index;
    if (g_menuState.index >= g_menuState.scrollOffset + 6)
        g_menuState.scrollOffset = g_menuState.index - 5;
}

// ── Navigation ───────────────────────────────────────────────────────────

void ScrollWheelMenuAddon::navUp() {
    if (!g_menuState.active) return;
    if (g_menuState.index > 0)
        g_menuState.index--;
    else
        g_menuState.index = currentItemCount() - 1;
    clampScrollOffset();
    markMenuDirty();
}

void ScrollWheelMenuAddon::navDown() {
    if (!g_menuState.active) return;
    uint8_t count = currentItemCount();
    if (g_menuState.index < count - 1)
        g_menuState.index++;
    else
        g_menuState.index = 0;
    clampScrollOffset();
    markMenuDirty();
}

void ScrollWheelMenuAddon::navSelect() {
    if (!g_menuState.active) return;

    SWMenuLevel currentLevel = static_cast<SWMenuLevel>(g_menuState.level);
    uint8_t idx = g_menuState.index;

    if (currentLevel == SWMenuLevel::INFO) {
        if (g_menuState.infoSource == 0) {
            g_menuState.level = static_cast<uint8_t>(SWMenuLevel::MAIN);
            g_menuState.index = mainIndex;
        } else {
            g_menuState.level = static_cast<uint8_t>(SWMenuLevel::RGB_SUB);
            g_menuState.index = rgbSubIndex;
        }
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        return;
    }

    const SWMenuItem* table = currentMenuTable();
    const SWMenuItem& item = table[idx];
    SWMenuLevel target = item.targetLevel;

    if (target == SWMenuLevel::INFO) {
        if (currentLevel == SWMenuLevel::MAIN) {
            mainIndex = idx;
            g_menuState.infoSource = 0;
        } else {
            g_menuState.infoSource = 1;
        }
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::INFO);
        g_menuState.index = idx;
        g_menuState.scrollOffset = 0;
    } else {
        if (currentLevel == SWMenuLevel::MAIN)
            mainIndex = idx;
        else if (currentLevel == SWMenuLevel::RGB_SUB)
            rgbSubIndex = idx;
        g_menuState.level = static_cast<uint8_t>(target);
        g_menuState.index = 0;
        g_menuState.scrollOffset = 0;
    }
    markMenuDirty();
}

void ScrollWheelMenuAddon::navToggle() {
    if (!g_menuState.active) {
        g_menuState.active = true;
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::MAIN);
        g_menuState.index = 0;
        g_menuState.scrollOffset = 0;
        g_scrollWheelMenuActive = true;
        markMenuDirty();
        printf("[ScrollWheel] Menu ENTER\n");
    } else {
        g_menuState.active = false;
        g_scrollWheelMenuActive = false;
        markMenuDirty();
        printf("[ScrollWheel] Menu EXIT\n");
    }
}

// ── GP30: 5-state button FSM (0x1abin/MultiButton classic) ──────────────
//
// Architecture (from user spec):
//   1. Raw GPIO → 30 ms sliding-window filter → debouncedButton
//   2. FSM reads ONLY debouncedButton, never raw GPIO
//   3. Five states: IDLE, DEBOUNCE_PRESS, PRESS, LONG, DEBOUNCE_RELEASE
//   4. Timer reset on every state transition
//   5. LONG state is isolated: release → DEBOUNCE_RELEASE → IDLE
//      (no short-press callback ever fires after long press)

void ScrollWheelMenuAddon::updateButton(uint32_t now) {
    // ── Step 1: 30 ms digital filter → debouncedButton ───────────────
    bool raw = readPin(SCROLLWHEEL_PIN_SW);
    static bool    filterPrev  = false;
    static uint32_t filterStart = 0;

    if (raw != filterPrev) {
        filterStart = now;
        filterPrev  = raw;
    }
    if ((now - filterStart) >= 30) {
        debouncedButton = raw;
    }

    // ── Step 2: 5-state FSM (reads ONLY debouncedButton) ─────────────
    switch (btnState) {

    case BTN_IDLE:
        if (debouncedButton) {
            btnTimer  = now;
            btnState  = BTN_DEBOUNCE_PRESS;
            g_scrollWheelButtonBusy = true;   // lock GP30 from DIP immediately
        }
        break;

    case BTN_DEBOUNCE_PRESS:
        if (!debouncedButton) {
            btnState = BTN_IDLE;                // bounce rollback
            g_scrollWheelButtonBusy = false;    // release lock
        } else if ((now - btnTimer) >= 30) {
            btnTimer = now;
            btnState = BTN_PRESS;
        }
        break;

    case BTN_PRESS:
        if (!debouncedButton) {
            btnTimer = now;
            btnState = BTN_DEBOUNCE_RELEASE;
        } else if ((now - btnTimer) >= SCROLLWHEEL_LONG_PRESS_MS) {
            btnTimer     = now;
            btnFromLong  = true;
            btnState     = BTN_LONG;
            navToggle();
        }
        break;

    case BTN_LONG:
        if (!debouncedButton) {
            btnTimer = now;
            btnState = BTN_DEBOUNCE_RELEASE;
        }
        break;

    case BTN_DEBOUNCE_RELEASE:
        if (debouncedButton) {
            btnState = btnFromLong ? BTN_LONG : BTN_PRESS;
        } else if ((now - btnTimer) >= 30) {
            if (!btnFromLong) {
                navSelect();
            }
            btnFromLong    = false;
            btnState       = BTN_IDLE;
            g_scrollWheelButtonBusy = false;    // release lock
        }
        break;
    }
}

// ── Main process ─────────────────────────────────────────────────────────

void ScrollWheelMenuAddon::process() {
    uint32_t now = getMillis();

    // GP30: 5-state FSM with digital filter
    updateButton(now);

    // GP31/GP32: simple edge detection for rotary navigation
    bool aRaw = readPin(SCROLLWHEEL_PIN_A);
    bool bRaw = readPin(SCROLLWHEEL_PIN_B);

    if (g_menuState.active) {
        bool aPressed = aRaw && !prevA;
        bool bPressed = bRaw && !prevB;

        if (aPressed || bPressed) {
            uint32_t elapsed = now - lastRotaryTime;
            if (elapsed >= SCROLLWHEEL_ROTARY_DEBOUNCE_MS) {
                if (aPressed) navUp();
                if (bPressed) navDown();
                lastRotaryTime = now;
            }
        }
    }

    prevA = aRaw;
    prevB = bRaw;
}
