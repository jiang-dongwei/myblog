#include "addons/scrollwheel_menu.h"
#include "gamepad.h"
#include "helper.h"
#include "storagemanager.h"
#include "eventmanager.h"
#include "GPStorageSaveEvent.h"

#include <cstdio>
#include <cstring>

// ── Menu data tables ─────────────────────────────────────────────────────

static constexpr uint8_t RGB_SUB_KEY_FLASH_INDEX = 0;
static constexpr uint8_t RGB_SUB_ALL_OFF_INDEX = 4;
static constexpr uint8_t BRIGHTNESS_LEVEL_COUNT = 3;
static constexpr uint32_t GAMEPLAY_INPUT_RELEASE_MS = 30;

namespace {
enum class GameplayInputLockState : uint8_t {
    UNLOCKED,
    CAPTURED,
    DRAIN_UNTIL_RELEASE,
};

GameplayInputLockState gameplayInputLockState = GameplayInputLockState::UNLOCKED;
bool gameplayReleaseTimerRunning = false;
uint32_t gameplayReleaseStartMs = 0;
} // namespace

const SWMenuItem kMenuMain[] = {
    { "RP2350B FW Version", SWMenuLevel::INFO, 0 },
    { "ESP32C6 Status",     SWMenuLevel::INFO, 0 },
#if SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED
    { "Battery Info",       SWMenuLevel::BATTERY_INFO, 0 },
#endif
    { "RGB Customize",      SWMenuLevel::RGB_SUB, 0 },
};
const uint8_t kMenuMainCount = sizeof(kMenuMain) / sizeof(kMenuMain[0]);

const SWMenuItem kMenuRgbSub[] = {
    { "Key Flash",           SWMenuLevel::COLOR,  0 },
    { "Key Effect",          SWMenuLevel::BUTTON_EFFECT,  0 },
    { "Base Effect",         SWMenuLevel::AMBIENT_EFFECT, 0 },
    { "Brightness",          SWMenuLevel::BRIGHTNESS, 0 },
    { "All OFF",             SWMenuLevel::INFO,   0 },  // immediate action, no sub-level
};
const uint8_t kMenuRgbSubCount = sizeof(kMenuRgbSub) / sizeof(kMenuRgbSub[0]);

// COLOR items use `targetIndex` to carry the AnimationStation `colors`
// vector index so that g_menuRgbTop/Bottom/Button values match proto
// AnimationOptions.staticColorIndex / buttonColorIndex encoding.
//   0=Black(OFF), 1=White, 2=Red, 3=Orange, 4=Yellow, 5=LimeGreen,
//   6=Green, 7=Seafoam, 8=Aqua(Cyan), 9=SkyBlue, 10=Blue, 11=Purple.
const SWMenuItem kMenuColors[] = {
    { "OFF",    SWMenuLevel::INFO, 0  },  // ColorBlack
    { "Red",    SWMenuLevel::INFO, 2  },  // ColorRed
    { "Orange", SWMenuLevel::INFO, 3  },  // ColorOrange
    { "Yellow", SWMenuLevel::INFO, 4  },  // ColorYellow
    { "Green",  SWMenuLevel::INFO, 6  },  // ColorGreen
    { "Cyan",   SWMenuLevel::INFO, 8  },  // ColorAqua
    { "Blue",   SWMenuLevel::INFO, 10 },  // ColorBlue
    { "Purple", SWMenuLevel::INFO, 11 },  // ColorPurple
    { "White",  SWMenuLevel::INFO, 1  },  // ColorWhite
};
const uint8_t kMenuColorsCount = sizeof(kMenuColors) / sizeof(kMenuColors[0]);

// Button LED effect picker. Existing render indices remain stable:
// 0=Static Color, 1=Rainbow, 2=Chase, 4=Breathing, 6=Gradient.
const SWMenuItem kMenuButtonEffects[] = {
    { "Static Color", SWMenuLevel::COLOR_BTN,        0 },
    { "Gradient",     SWMenuLevel::INFO,             6 },
    { "Breathing",    SWMenuLevel::COLOR_BTN_BREATH, 4 },
    { "Rainbow",      SWMenuLevel::INFO,             1 },
    { "Chase",        SWMenuLevel::INFO,             2 },
};
const uint8_t kMenuButtonEffectsCount = sizeof(kMenuButtonEffects) / sizeof(kMenuButtonEffects[0]);

// Ambient LED effect picker. Existing indices remain stable:
// 0=Static Color, 1=Gradient, 2=Chase, 4=Rainbow, 5=Breathing.
const SWMenuItem kMenuAmbientEffects[] = {
    { "Static Color", SWMenuLevel::COLOR_AMB,        0 },
    { "Gradient",     SWMenuLevel::INFO,             1 },
    { "Chase",        SWMenuLevel::INFO,             2 },
    { "Breathing",    SWMenuLevel::COLOR_AMB_BREATH, 5 },
    { "Rainbow",      SWMenuLevel::INFO,             4 },
};
const uint8_t kMenuAmbientEffectsCount = sizeof(kMenuAmbientEffects) / sizeof(kMenuAmbientEffects[0]);

const SWMenuItem kMenuBrightness[] = {
    { "Bright", SWMenuLevel::INFO, 0 },
    { "Normal", SWMenuLevel::INFO, 1 },
    { "Dim",    SWMenuLevel::INFO, 2 },
};
const uint8_t kMenuBrightnessCount = sizeof(kMenuBrightness) / sizeof(kMenuBrightness[0]);

// ── Cross-core state ─────────────────────────────────────────────────────

volatile ScrollWheelMenuState g_menuState = { false, 0, 0, 0 };
volatile bool g_menuStateDirty = false;
volatile bool g_scrollWheelMenuActive = false;
volatile bool g_scrollWheelButtonBusy = false;
volatile bool g_scrollWheelButtonLongPressed = false;
std::atomic<uint32_t> g_scrollWheelLastActivityMs { 0 };
volatile uint8_t g_menuRgbTop    = 0xFF;
volatile uint8_t g_menuRgbBottom = 0xFF;
volatile uint8_t g_menuRgbButton = 0xFF;
volatile uint8_t g_menuRgbTarget = 0;
volatile uint8_t g_menuButtonEffect  = 0xFF;
volatile uint8_t g_menuAmbientEffect = 0xFF;
volatile uint8_t g_menuBrightnessLevel = 0;
volatile bool g_menuRgbPowerEnabled = true;

bool isScrollWheelGameplayInputLocked() {
    return gameplayInputLockState != GameplayInputLockState::UNLOCKED;
}

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
    initPin(SCROLLWHEEL_PIN_BACK);

    prevButtonRaw = readPin(SCROLLWHEEL_PIN_SW);
    prevA = readPin(SCROLLWHEEL_PIN_A);
    prevB = readPin(SCROLLWHEEL_PIN_B);
    prevBack = readPin(SCROLLWHEEL_PIN_BACK);
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
    gameplayInputLockState = GameplayInputLockState::UNLOCKED;
    gameplayReleaseTimerRunning = false;
    gameplayReleaseStartMs = 0;
    g_scrollWheelLastActivityMs.store(getMillis(), std::memory_order_release);

    // Restore menu color overrides from flash (0xFF = never set).
    FightpadAmbientLEDOptions& opts = Storage::getInstance().getFightpadAmbientLEDOptions();
    if (opts.topBoardColorIndex != 0xFF)
        g_menuRgbTop = static_cast<uint8_t>(opts.topBoardColorIndex);
    if (opts.bottomBoardColorIndex != 0xFF)
        g_menuRgbBottom = static_cast<uint8_t>(opts.bottomBoardColorIndex);
    if (opts.buttonFlashColorIndex != 0xFF)
        g_menuRgbButton = static_cast<uint8_t>(opts.buttonFlashColorIndex);
    if (opts.buttonEffectIndex != 0xFF)
        g_menuButtonEffect = static_cast<uint8_t>(opts.buttonEffectIndex);
    if (opts.ambientEffectIndex != 0xFF)
        g_menuAmbientEffect = static_cast<uint8_t>(opts.ambientEffectIndex);
    g_menuBrightnessLevel = (opts.brightnessLevel < BRIGHTNESS_LEVEL_COUNT)
        ? static_cast<uint8_t>(opts.brightnessLevel)
        : 0;

    // "All OFF" is already persisted as three black colors plus default
    // static effects.  Reconstruct the runtime rail request without changing
    // the protobuf layout, so existing saved configurations stay compatible.
    g_menuRgbPowerEnabled = !(
        g_menuRgbTop == 0 &&
        g_menuRgbBottom == 0 &&
        g_menuRgbButton == 0 &&
        g_menuButtonEffect == 0xFF &&
        g_menuAmbientEffect == 0xFF);

    printf("[ScrollWheel] Setup OK. Pins: SW=%d A=%d B=%d\n",
           SCROLLWHEEL_PIN_SW, SCROLLWHEEL_PIN_A, SCROLLWHEEL_PIN_B);
}

// ── Menu helpers ─────────────────────────────────────────────────────────

const SWMenuItem* ScrollWheelMenuAddon::currentMenuTable() const {
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:           return kMenuMain;
        case SWMenuLevel::RGB_SUB:        return kMenuRgbSub;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_BTN:
        case SWMenuLevel::COLOR_AMB:
        case SWMenuLevel::COLOR_BTN_BREATH:
        case SWMenuLevel::COLOR_AMB_BREATH:
                                             return kMenuColors;
        case SWMenuLevel::BUTTON_EFFECT:  return kMenuButtonEffects;
        case SWMenuLevel::AMBIENT_EFFECT: return kMenuAmbientEffects;
        case SWMenuLevel::BRIGHTNESS:     return kMenuBrightness;
        case SWMenuLevel::BATTERY_INFO:   return kMenuMain;
        default:                          return kMenuMain;
    }
}

uint8_t ScrollWheelMenuAddon::currentItemCount() const {
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:           return kMenuMainCount;
        case SWMenuLevel::RGB_SUB:        return kMenuRgbSubCount;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_BTN:
        case SWMenuLevel::COLOR_AMB:
        case SWMenuLevel::COLOR_BTN_BREATH:
        case SWMenuLevel::COLOR_AMB_BREATH:
                                             return kMenuColorsCount;
        case SWMenuLevel::BUTTON_EFFECT:  return kMenuButtonEffectsCount;
        case SWMenuLevel::AMBIENT_EFFECT: return kMenuAmbientEffectsCount;
        case SWMenuLevel::BRIGHTNESS:     return kMenuBrightnessCount;
        case SWMenuLevel::BATTERY_INFO:   return SW_BATTERY_PAGE_COUNT;
        default:                          return kMenuMainCount;
    }
}

static void markMenuDirty() {
    g_menuStateDirty = true;
}

// Write the three color-override variables + effect indices to config
// and trigger a flash commit.  Follows the same pattern as
// StaticColor::SaveIndexOptions() + AnimationStation::HandleEvent().
static void persistConfig() {
    FightpadAmbientLEDOptions& opts = Storage::getInstance().getFightpadAmbientLEDOptions();
    opts.topBoardColorIndex    = g_menuRgbTop;
    opts.bottomBoardColorIndex = g_menuRgbBottom;
    opts.buttonFlashColorIndex = g_menuRgbButton;
    opts.buttonEffectIndex     = g_menuButtonEffect;
    opts.ambientEffectIndex    = g_menuAmbientEffect;
    opts.brightnessLevel       = g_menuBrightnessLevel;
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(false));
}

static void clampScrollOffset() {
    uint8_t count = 0;
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:           count = kMenuMainCount; break;
        case SWMenuLevel::RGB_SUB:        count = kMenuRgbSubCount; break;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_BTN:
        case SWMenuLevel::COLOR_AMB:
        case SWMenuLevel::COLOR_BTN_BREATH:
        case SWMenuLevel::COLOR_AMB_BREATH:
                                             count = kMenuColorsCount; break;
        case SWMenuLevel::BUTTON_EFFECT:  count = kMenuButtonEffectsCount; break;
        case SWMenuLevel::AMBIENT_EFFECT: count = kMenuAmbientEffectsCount; break;
        case SWMenuLevel::BRIGHTNESS:     count = kMenuBrightnessCount; break;
        case SWMenuLevel::BATTERY_INFO:   count = SW_BATTERY_PAGE_COUNT; break;
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

    // Battery Info uses index as a page number.  A short press advances to
    // the next page; the rotary inputs use navUp/navDown for either direction.
    if (currentLevel == SWMenuLevel::BATTERY_INFO) {
        g_menuState.index = static_cast<uint8_t>((idx + 1) % SW_BATTERY_PAGE_COUNT);
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        return;
    }

    // RGB_SUB "All OFF": immediate action — turn off all LEDs.
    // Reset colors to black and effects to default (Static Color).
    if (currentLevel == SWMenuLevel::RGB_SUB && idx == RGB_SUB_ALL_OFF_INDEX) {
        g_menuRgbTop    = 0;
        g_menuRgbBottom = 0;
        g_menuRgbButton = 0;
        g_menuButtonEffect  = 0xFF;
        g_menuAmbientEffect = 0xFF;
        g_menuRgbPowerEnabled = false;
        persistConfig();
        markMenuDirty();
        return;
    }

    // BRIGHTNESS is a terminal list. Apply and persist immediately, then stay
    // on the list so the three levels can be compared in real time.
    if (currentLevel == SWMenuLevel::BRIGHTNESS) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t brightnessLevel = table[idx].targetIndex;
        g_menuBrightnessLevel = (brightnessLevel < BRIGHTNESS_LEVEL_COUNT)
            ? brightnessLevel
            : 0;
        persistConfig();
        markMenuDirty();
        return;
    }

    // COLOR is a terminal list level — short press applies the selected
    // color's AnimationStation index (stored in targetIndex) to the target
    // and stays on the same list so the user can try different colors
    // without re-entering.  Exit via long press or BACK.
    if (currentLevel == SWMenuLevel::COLOR) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;  // AnimationStation colors index
        switch (g_menuRgbTarget) {
            case 0: g_menuRgbTop    = colorIdx; break;
            case 1: g_menuRgbBottom = colorIdx; break;
            case 2: g_menuRgbButton = colorIdx; break;
            default: break;
        }
        if (colorIdx != 0)
            g_menuRgbPowerEnabled = true;
        // Stay in COLOR — do not navigate back.
        persistConfig();
        markMenuDirty();
        return;
    }

    // COLOR_BTN: color picker under Button LED Effect → Static Color.
    // Short press applies the color to GP22 (top) and sets effect to Static Color.
    if (currentLevel == SWMenuLevel::COLOR_BTN) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;
        g_menuRgbTop       = colorIdx;
        g_menuButtonEffect = 0;               // enable Static Color effect
        if (colorIdx != 0)
            g_menuRgbPowerEnabled = true;
        persistConfig();
        markMenuDirty();
        return;
    }

    // COLOR_AMB: color picker under Ambient LED Effect → Static Color.
    // Short press applies the color to GP40 (bottom) and sets effect to Static Color.
    if (currentLevel == SWMenuLevel::COLOR_AMB) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;
        g_menuRgbBottom      = colorIdx;
        g_menuAmbientEffect  = 0;             // enable Static Color effect
        if (colorIdx != 0)
            g_menuRgbPowerEnabled = true;
        persistConfig();
        markMenuDirty();
        return;
    }

    // COLOR_BTN_BREATH: color picker under Button LED Effect -> Breathing.
    if (currentLevel == SWMenuLevel::COLOR_BTN_BREATH) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;
        g_menuRgbTop       = colorIdx;
        g_menuButtonEffect = 4;               // enable Breathing effect
        if (colorIdx != 0)
            g_menuRgbPowerEnabled = true;
        persistConfig();
        markMenuDirty();
        return;
    }

    // COLOR_AMB_BREATH: color picker under Ambient LED Effect -> Breathing.
    if (currentLevel == SWMenuLevel::COLOR_AMB_BREATH) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;
        g_menuRgbBottom      = colorIdx;
        g_menuAmbientEffect  = 5;             // enable Breathing effect
        if (colorIdx != 0)
            g_menuRgbPowerEnabled = true;
        persistConfig();
        markMenuDirty();
        return;
    }

    // BUTTON_EFFECT / AMBIENT_EFFECT: terminal effect items (targetLevel==INFO)
    // apply the effect immediately.  Non-terminal items (e.g. Static Color →
    // COLOR_BTN) fall through to the general navigation below.
    if (currentLevel == SWMenuLevel::BUTTON_EFFECT ||
        currentLevel == SWMenuLevel::AMBIENT_EFFECT) {
        const SWMenuItem* table = currentMenuTable();
        const SWMenuItem& item = table[idx];
        if (item.targetLevel == SWMenuLevel::INFO) {
            uint8_t effectIdx = item.targetIndex;
            // Terminal effects apply immediately. Chase owns its dynamic color
            // source, so drop any saved static-color override for that chain.
            if (currentLevel == SWMenuLevel::BUTTON_EFFECT) {
                g_menuButtonEffect = effectIdx;
                if (effectIdx == 2)
                    g_menuRgbTop = 0xFF; // Chase uses dynamic colors, not static color override.
            } else {
                g_menuAmbientEffect = effectIdx;
                if (effectIdx == 2)
                    g_menuRgbBottom = 0xFF; // Chase uses dynamic colors, not static color override.
            }
            g_menuRgbPowerEnabled = true;
            persistConfig();
            markMenuDirty();
            return;
        }
        // Non-terminal: fall through to general navigation.
    }

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
        else if (currentLevel == SWMenuLevel::RGB_SUB) {
            rgbSubIndex = idx;
            if (idx == RGB_SUB_KEY_FLASH_INDEX)
                g_menuRgbTarget = 2;  // Button RGB → flash color
        }
        g_menuState.level = static_cast<uint8_t>(target);
        g_menuState.index = (target == SWMenuLevel::BRIGHTNESS)
            ? g_menuBrightnessLevel
            : 0;
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

void ScrollWheelMenuAddon::navBack() {
    if (!g_menuState.active) return;

    SWMenuLevel currentLevel = static_cast<SWMenuLevel>(g_menuState.level);

    switch (currentLevel) {
    case SWMenuLevel::MAIN:
        // Back from MAIN = exit menu
        g_menuState.active = false;
        g_scrollWheelMenuActive = false;
        markMenuDirty();
        break;
    case SWMenuLevel::RGB_SUB:
        // Back to MAIN, restore selection
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::MAIN);
        g_menuState.index = mainIndex;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::BATTERY_INFO:
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::MAIN);
        g_menuState.index = mainIndex;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR:
    case SWMenuLevel::BUTTON_EFFECT:
    case SWMenuLevel::AMBIENT_EFFECT:
    case SWMenuLevel::BRIGHTNESS:
        // Back to RGB_SUB without applying changes
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::RGB_SUB);
        g_menuState.index = rgbSubIndex;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR_BTN:
        // Back to BUTTON_EFFECT (Static Color = index 0)
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::BUTTON_EFFECT);
        g_menuState.index = 0;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR_AMB:
        // Back to AMBIENT_EFFECT (Static Color = index 0)
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::AMBIENT_EFFECT);
        g_menuState.index = 0;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR_BTN_BREATH:
        // Back to BUTTON_EFFECT (Breathing = index 2)
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::BUTTON_EFFECT);
        g_menuState.index = 2;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR_AMB_BREATH:
        // Back to AMBIENT_EFFECT (Breathing = index 3)
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::AMBIENT_EFFECT);
        g_menuState.index = 3;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::INFO:
        if (g_menuState.infoSource == 0) {
            g_menuState.level = static_cast<uint8_t>(SWMenuLevel::MAIN);
            g_menuState.index = mainIndex;
        } else {
            g_menuState.level = static_cast<uint8_t>(SWMenuLevel::RGB_SUB);
            g_menuState.index = rgbSubIndex;
        }
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
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

    if (raw != prevButtonRaw) {
        prevButtonRaw = raw;
        g_scrollWheelLastActivityMs.store(now, std::memory_order_release);
    }

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
            g_scrollWheelButtonLongPressed = true;
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
            g_scrollWheelButtonLongPressed = false;
        }
        break;
    }
}

void ScrollWheelMenuAddon::updateGameplayInputLock(uint32_t now) {
    if (g_menuState.active) {
        gameplayInputLockState = GameplayInputLockState::CAPTURED;
        gameplayReleaseTimerRunning = false;
        return;
    }

    if (gameplayInputLockState == GameplayInputLockState::UNLOCKED) {
        return;
    }

    if (gameplayInputLockState == GameplayInputLockState::CAPTURED) {
        gameplayInputLockState = GameplayInputLockState::DRAIN_UNTIL_RELEASE;
        gameplayReleaseTimerRunning = false;
    }

    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    const uint32_t rawPressed =
        (~static_cast<uint32_t>(gpio_get_all())) & SCROLLWHEEL_GAMEPLAY_GPIO_MASK;
    const bool debouncedReleased = gamepad != nullptr &&
        ((static_cast<uint32_t>(gamepad->debouncedGpio) &
          SCROLLWHEEL_GAMEPLAY_GPIO_MASK) == 0);

    if (rawPressed == 0 && debouncedReleased) {
        if (!gameplayReleaseTimerRunning) {
            gameplayReleaseTimerRunning = true;
            gameplayReleaseStartMs = now;
        } else if ((now - gameplayReleaseStartMs) >= GAMEPLAY_INPUT_RELEASE_MS) {
            gameplayInputLockState = GameplayInputLockState::UNLOCKED;
            gameplayReleaseTimerRunning = false;
        }
    } else {
        gameplayReleaseTimerRunning = false;
    }
}

// ── Main process ─────────────────────────────────────────────────────────

void ScrollWheelMenuAddon::process() {
    uint32_t now = getMillis();

    // GP30: 5-state FSM with digital filter
    updateButton(now);

    // Capture immediately after GP30 opens the menu. This also covers the
    // edge case where an already-held GP19 closes it later in this same frame.
    updateGameplayInputLock(now);

    // GP31/GP32: simple edge detection for rotary navigation
    bool aRaw = readPin(SCROLLWHEEL_PIN_A);
    bool bRaw = readPin(SCROLLWHEEL_PIN_B);

    if (aRaw != prevA || bRaw != prevB) {
        g_scrollWheelLastActivityMs.store(now, std::memory_order_release);
    }

    if (g_menuState.active) {
        // INFO pages display static text; there is no list to scroll.
        SWMenuLevel level = static_cast<SWMenuLevel>(g_menuState.level);
        if (level != SWMenuLevel::INFO) {
            bool aPressed = aRaw && !prevA;
            bool bPressed = bRaw && !prevB;

            if (aPressed || bPressed) {
                uint32_t elapsed = now - lastRotaryTime;
                if (elapsed >= SCROLLWHEEL_ROTARY_DEBOUNCE_MS) {
                    if (aPressed) navDown();
                    if (bPressed) navUp();
                    lastRotaryTime = now;
                }
            }
        }
    }

    // GP19 BACK: press edge → navBack()
    if (g_menuState.active) {
        bool backRaw = readPin(SCROLLWHEEL_PIN_BACK);
        bool backPressed = backRaw && !prevBack;
        if (backPressed) {
            uint32_t elapsed = now - lastBackTime;
            if (elapsed >= SCROLLWHEEL_ROTARY_DEBOUNCE_MS) {
                navBack();
                lastBackTime = now;
            }
        }
        prevBack = backRaw;
    }

    prevA = aRaw;
    prevB = bRaw;

    // Update again after menu navigation so a GP19 exit starts draining in
    // this same Core0 loop iteration.
    updateGameplayInputLock(now);
}
