#include "addons/scrollwheel_menu.h"
#include "gamepad.h"
#include "helper.h"
#include "storagemanager.h"
#include "eventmanager.h"
#include "GPStorageSaveEvent.h"

#include <cstdio>
#include <cstring>

// ── Menu data tables ─────────────────────────────────────────────────────

static constexpr uint8_t RGB_SUB_ALL_OFF_INDEX = 3;
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

uint8_t legacyButtonEffectToLightEffect(uint8_t effect) {
    switch (effect) {
        case 0: return LIGHT_EFFECT_STATIC_COLOR;
        case 6: return LIGHT_EFFECT_GRADIENT;
        case 4: return LIGHT_EFFECT_BREATHING;
        case 1: return LIGHT_EFFECT_RAINBOW;
        case 2: return LIGHT_EFFECT_CHASE;
        case 3: return LIGHT_EFFECT_STATIC_COLOR; // legacy Static Theme
        case 5: return LIGHT_EFFECT_RAINBOW;      // legacy Breathing Rainbow
        default: return LIGHT_EFFECT_UNSET;
    }
}

uint8_t legacyAmbientEffectToLightEffect(uint8_t effect) {
    switch (effect) {
        case 0: return LIGHT_EFFECT_STATIC_COLOR;
        case 1: return LIGHT_EFFECT_GRADIENT;
        case 5: return LIGHT_EFFECT_BREATHING;
        case 4: return LIGHT_EFFECT_RAINBOW;
        case 2: return LIGHT_EFFECT_CHASE;
        case 3: return LIGHT_EFFECT_RAINBOW; // legacy Breathing Rainbow
        default: return LIGHT_EFFECT_UNSET;
    }
}

uint8_t lightEffectToLegacyButtonEffect(uint8_t effect) {
    switch (effect) {
        case LIGHT_EFFECT_STATIC_COLOR: return 0;
        case LIGHT_EFFECT_GRADIENT:     return 6;
        case LIGHT_EFFECT_BREATHING:    return 4;
        case LIGHT_EFFECT_RAINBOW:      return 1;
        case LIGHT_EFFECT_CHASE:        return 2;
        default:                        return 0xFF;
    }
}

uint8_t lightEffectToLegacyAmbientEffect(uint8_t effect) {
    switch (effect) {
        case LIGHT_EFFECT_STATIC_COLOR: return 0;
        case LIGHT_EFFECT_GRADIENT:     return 1;
        case LIGHT_EFFECT_BREATHING:    return 5;
        case LIGHT_EFFECT_RAINBOW:      return 4;
        case LIGHT_EFFECT_CHASE:        return 2;
        default:                        return 0xFF;
    }
}
} // namespace

const SWMenuItem kMenuMain[] = {
    { "RP2350B FW Version", SWMenuLevel::INFO, 0 },
    { "ESP32C6 Status",     SWMenuLevel::INFO, 0 },
#if SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED
    { "Battery Info",       SWMenuLevel::BATTERY_INFO, 0 },
#endif
    { "RGB Customize",      SWMenuLevel::RGB_SUB, 0 },
    { "Controller Type",    SWMenuLevel::CONTROLLER_TYPE, 0 },
};
const uint8_t kMenuMainCount = sizeof(kMenuMain) / sizeof(kMenuMain[0]);

// Keep this list limited to input modes implemented by upstream GP2040-CE.
// Arcade Stick is an InputModeDeviceType, not a standalone USB InputMode.
const SWMenuItem kMenuControllerTypes[] = {
    { "XBOX",        SWMenuLevel::INFO, INPUT_MODE_XINPUT },
    { "PS3",         SWMenuLevel::INFO, INPUT_MODE_PS3 },
    { "PS4",         SWMenuLevel::INFO, INPUT_MODE_PS4 },
    { "PS5",         SWMenuLevel::INFO, INPUT_MODE_PS5 },
    { "SWITCH",      SWMenuLevel::INFO, INPUT_MODE_SWITCH },
    { "SWITCH PRO",  SWMenuLevel::INFO, INPUT_MODE_SWITCH_PRO },
    { "KEYBOARD",    SWMenuLevel::INFO, INPUT_MODE_KEYBOARD },
    { "GENERIC HID", SWMenuLevel::INFO, INPUT_MODE_GENERIC },
};
const uint8_t kMenuControllerTypesCount =
    sizeof(kMenuControllerTypes) / sizeof(kMenuControllerTypes[0]);

const SWMenuItem kMenuRgbSub[] = {
    { "Key Flash",           SWMenuLevel::COLOR,  0 },
    { "Light Effect",        SWMenuLevel::LIGHT_EFFECT, 0 },
    { "Brightness",          SWMenuLevel::BRIGHTNESS, 0 },
    { "All OFF",             SWMenuLevel::INFO,   0 },  // immediate action, no sub-level
};
const uint8_t kMenuRgbSubCount = sizeof(kMenuRgbSub) / sizeof(kMenuRgbSub[0]);

// COLOR items use `targetIndex` to carry the AnimationStation `colors`
// vector index so that the shared effect/flash values match proto
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

// Unified Key/Base effect picker. targetIndex uses SWLightEffect IDs; legacy
// Key/Base protobuf indices are translated only while loading and saving.
const SWMenuItem kMenuLightEffects[] = {
    { "Static Color", SWMenuLevel::COLOR_EFFECT,        LIGHT_EFFECT_STATIC_COLOR },
    { "Gradient",     SWMenuLevel::INFO,                LIGHT_EFFECT_GRADIENT },
    { "Breathing",    SWMenuLevel::COLOR_EFFECT_BREATH, LIGHT_EFFECT_BREATHING },
    { "Rainbow",      SWMenuLevel::INFO,                LIGHT_EFFECT_RAINBOW },
    { "Chase",        SWMenuLevel::INFO,                LIGHT_EFFECT_CHASE },
};
const uint8_t kMenuLightEffectsCount = sizeof(kMenuLightEffects) / sizeof(kMenuLightEffects[0]);

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
volatile uint8_t g_menuRgbEffectColor = 0xFF;
volatile uint8_t g_menuRgbButton = 0xFF;
volatile uint8_t g_menuLightEffect = LIGHT_EFFECT_UNSET;
volatile uint8_t g_menuBrightnessLevel = 0;
volatile bool g_menuRgbPowerEnabled = true;
volatile bool g_manualLightEffectsEnabled = true;
volatile bool g_scrollWheelRebootBlackout = false;

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
    g_scrollWheelRebootBlackout = false;
    gameplayInputLockState = GameplayInputLockState::UNLOCKED;
    gameplayReleaseTimerRunning = false;
    gameplayReleaseStartMs = 0;
    g_scrollWheelLastActivityMs.store(getMillis(), std::memory_order_release);

    // Restore and normalize the old two-effect/two-color persistence layout.
    // The unified runtime state gives Key precedence when legacy values differ.
    FightpadAmbientLEDOptions& opts = Storage::getInstance().getFightpadAmbientLEDOptions();
    const uint8_t storedTop = static_cast<uint8_t>(opts.topBoardColorIndex);
    const uint8_t storedBottom = static_cast<uint8_t>(opts.bottomBoardColorIndex);
    const uint8_t storedFlash = static_cast<uint8_t>(opts.buttonFlashColorIndex);
    const uint8_t storedButtonEffect = static_cast<uint8_t>(opts.buttonEffectIndex);
    const uint8_t storedAmbientEffect = static_cast<uint8_t>(opts.ambientEffectIndex);
    const bool persistedAllOff =
        storedTop == 0 &&
        storedBottom == 0 &&
        storedFlash == 0 &&
        storedButtonEffect == 0xFF &&
        storedAmbientEffect == 0xFF;

    const uint8_t sharedColor = (storedTop != 0xFF) ? storedTop : storedBottom;
    g_menuRgbEffectColor = sharedColor;
    g_menuRgbButton = storedFlash;

    if (persistedAllOff) {
        g_menuLightEffect = LIGHT_EFFECT_UNSET;
    } else {
        g_menuLightEffect = legacyButtonEffectToLightEffect(storedButtonEffect);
        if (g_menuLightEffect == LIGHT_EFFECT_UNSET) {
            g_menuLightEffect = legacyAmbientEffectToLightEffect(storedAmbientEffect);
        }
        if (g_menuLightEffect == LIGHT_EFFECT_UNSET) {
            g_menuLightEffect = LIGHT_EFFECT_STATIC_COLOR;
        }
    }
    g_menuBrightnessLevel = (opts.brightnessLevel < BRIGHTNESS_LEVEL_COUNT)
        ? static_cast<uint8_t>(opts.brightnessLevel)
        : 0;
    g_manualLightEffectsEnabled = opts.manualLightEffectsEnabled;

    // "All OFF" is persisted as three black colors plus two unset legacy
    // effects. Reconstruct the rail request without changing protobuf layout.
    g_menuRgbPowerEnabled = !persistedAllOff;

    printf("[ScrollWheel] Setup OK. Pins: SW=%d A=%d B=%d\n",
           SCROLLWHEEL_PIN_SW, SCROLLWHEEL_PIN_A, SCROLLWHEEL_PIN_B);
}

// ── Menu helpers ─────────────────────────────────────────────────────────

const SWMenuItem* ScrollWheelMenuAddon::currentMenuTable() const {
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:           return kMenuMain;
        case SWMenuLevel::RGB_SUB:        return kMenuRgbSub;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_EFFECT:
        case SWMenuLevel::COLOR_EFFECT_BREATH:
                                             return kMenuColors;
        case SWMenuLevel::LIGHT_EFFECT:   return kMenuLightEffects;
        case SWMenuLevel::BRIGHTNESS:     return kMenuBrightness;
        case SWMenuLevel::CONTROLLER_TYPE:return kMenuControllerTypes;
        case SWMenuLevel::BATTERY_INFO:   return kMenuMain;
        default:                          return kMenuMain;
    }
}

uint8_t ScrollWheelMenuAddon::currentItemCount() const {
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:           return kMenuMainCount;
        case SWMenuLevel::RGB_SUB:        return kMenuRgbSubCount;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_EFFECT:
        case SWMenuLevel::COLOR_EFFECT_BREATH:
                                             return kMenuColorsCount;
        case SWMenuLevel::LIGHT_EFFECT:   return kMenuLightEffectsCount;
        case SWMenuLevel::BRIGHTNESS:     return kMenuBrightnessCount;
        case SWMenuLevel::CONTROLLER_TYPE:return kMenuControllerTypesCount;
        case SWMenuLevel::BATTERY_INFO:   return SW_BATTERY_PAGE_COUNT;
        default:                          return kMenuMainCount;
    }
}

static void markMenuDirty() {
    g_menuStateDirty = true;
}

// Mirror the shared runtime color/effect into the two legacy Key/Base fields
// and trigger a flash commit. Follows the same pattern as
// StaticColor::SaveIndexOptions() + AnimationStation::HandleEvent().
static void persistConfig() {
    FightpadAmbientLEDOptions& opts = Storage::getInstance().getFightpadAmbientLEDOptions();
    opts.topBoardColorIndex    = g_menuRgbEffectColor;
    opts.bottomBoardColorIndex = g_menuRgbEffectColor;
    opts.buttonFlashColorIndex = g_menuRgbButton;
    opts.buttonEffectIndex     = lightEffectToLegacyButtonEffect(g_menuLightEffect);
    opts.ambientEffectIndex    = lightEffectToLegacyAmbientEffect(g_menuLightEffect);
    opts.brightnessLevel       = g_menuBrightnessLevel;
    opts.manualLightEffectsEnabled = g_manualLightEffectsEnabled;
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(false));
}

static void clampScrollOffset() {
    uint8_t count = 0;
    switch (static_cast<SWMenuLevel>(g_menuState.level)) {
        case SWMenuLevel::MAIN:           count = kMenuMainCount; break;
        case SWMenuLevel::RGB_SUB:        count = kMenuRgbSubCount; break;
        case SWMenuLevel::COLOR:
        case SWMenuLevel::COLOR_EFFECT:
        case SWMenuLevel::COLOR_EFFECT_BREATH:
                                             count = kMenuColorsCount; break;
        case SWMenuLevel::LIGHT_EFFECT:   count = kMenuLightEffectsCount; break;
        case SWMenuLevel::BRIGHTNESS:     count = kMenuBrightnessCount; break;
        case SWMenuLevel::CONTROLLER_TYPE:count = kMenuControllerTypesCount; break;
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
    // Outside the menu, a completed GP30 short press is the runtime switch for
    // normal Key/Base effects. The button FSM calls navSelect() only after a
    // debounced release, and never after BTN_LONG, so the 2-second menu gesture
    // remains mutually exclusive with this action.
    if (!g_menuState.active) {
        g_manualLightEffectsEnabled = !g_manualLightEffectsEnabled;
        persistConfig();
        return;
    }

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
    // Reset colors to black and persist both legacy effects as unset.
    if (currentLevel == SWMenuLevel::RGB_SUB && idx == RGB_SUB_ALL_OFF_INDEX) {
        g_menuRgbEffectColor = 0;
        g_menuRgbButton = 0;
        g_menuLightEffect = LIGHT_EFFECT_UNSET;
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

    // COLOR is the Key Flash terminal list — short press applies the selected
    // AnimationStation index (stored in targetIndex) to the flash color
    // and stays on the same list so the user can try different colors
    // without re-entering.  Exit via long press or BACK.
    if (currentLevel == SWMenuLevel::COLOR) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;  // AnimationStation colors index
        g_menuRgbButton = colorIdx;
        if (colorIdx != 0) {
            g_menuRgbPowerEnabled = true;
            g_manualLightEffectsEnabled = true;
        }
        // Stay in COLOR — do not navigate back.
        persistConfig();
        markMenuDirty();
        return;
    }

    // Shared effect color pickers apply to both GP22 and GP40. Static Color
    // and Breathing differ only in which unified effect is selected.
    if (currentLevel == SWMenuLevel::COLOR_EFFECT ||
        currentLevel == SWMenuLevel::COLOR_EFFECT_BREATH) {
        const SWMenuItem* table = currentMenuTable();
        uint8_t colorIdx = table[idx].targetIndex;
        g_menuRgbEffectColor = colorIdx;
        g_menuLightEffect = (currentLevel == SWMenuLevel::COLOR_EFFECT)
            ? LIGHT_EFFECT_STATIC_COLOR
            : LIGHT_EFFECT_BREATHING;
        if (colorIdx != 0) {
            g_menuRgbPowerEnabled = true;
            g_manualLightEffectsEnabled = true;
        }
        persistConfig();
        markMenuDirty();
        return;
    }

    // LIGHT_EFFECT terminal items apply immediately. Static Color and
    // Breathing fall through to their shared color pickers.
    if (currentLevel == SWMenuLevel::LIGHT_EFFECT) {
        const SWMenuItem* table = currentMenuTable();
        const SWMenuItem& item = table[idx];
        if (item.targetLevel == SWMenuLevel::INFO) {
            g_menuLightEffect = item.targetIndex;
            g_menuRgbPowerEnabled = true;
            g_manualLightEffectsEnabled = true;
            persistConfig();
            markMenuDirty();
            return;
        }
        // Non-terminal: fall through to general navigation.
    }

    // Controller Type directly reuses upstream InputMode values. A changed
    // USB mode needs a forced flash save and reboot so the host sees the new
    // descriptors on re-enumeration; selecting the active mode is a no-op.
    if (currentLevel == SWMenuLevel::CONTROLLER_TYPE) {
        const SWMenuItem* table = currentMenuTable();
        InputMode selectedMode = static_cast<InputMode>(table[idx].targetIndex);
        GamepadOptions& options = Storage::getInstance().getGamepadOptions();
        if (options.inputMode != selectedMode) {
            options.inputMode = selectedMode;
            // RAM-only visual reboot cue. FightpadAmbientLEDAddon sends a
            // final black frame during the existing 500ms save/reboot delay;
            // the saved effect remains intact and returns after reset.
            g_scrollWheelRebootBlackout = true;
            EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true, true));
        }
        markMenuDirty();
        return;
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
        }
        g_menuState.level = static_cast<uint8_t>(target);
        if (target == SWMenuLevel::BRIGHTNESS) {
            g_menuState.index = g_menuBrightnessLevel;
        } else if (target == SWMenuLevel::LIGHT_EFFECT &&
                   g_menuLightEffect < kMenuLightEffectsCount) {
            g_menuState.index = g_menuLightEffect;
        } else if (target == SWMenuLevel::CONTROLLER_TYPE) {
            const uint8_t currentMode = static_cast<uint8_t>(
                Storage::getInstance().getGamepadOptions().inputMode);
            g_menuState.index = 0;
            for (uint8_t i = 0; i < kMenuControllerTypesCount; i++) {
                if (kMenuControllerTypes[i].targetIndex == currentMode) {
                    g_menuState.index = i;
                    break;
                }
            }
        } else {
            g_menuState.index = 0;
        }
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
    case SWMenuLevel::CONTROLLER_TYPE:
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::MAIN);
        g_menuState.index = mainIndex;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR:
    case SWMenuLevel::LIGHT_EFFECT:
    case SWMenuLevel::BRIGHTNESS:
        // Back to RGB_SUB without applying changes
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::RGB_SUB);
        g_menuState.index = rgbSubIndex;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR_EFFECT:
        // Back to LIGHT_EFFECT (Static Color = index 0)
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::LIGHT_EFFECT);
        g_menuState.index = 0;
        g_menuState.scrollOffset = 0;
        markMenuDirty();
        break;
    case SWMenuLevel::COLOR_EFFECT_BREATH:
        // Back to LIGHT_EFFECT (Breathing = index 2)
        g_menuState.level = static_cast<uint8_t>(SWMenuLevel::LIGHT_EFFECT);
        g_menuState.index = 2;
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

    // Keep OLED activity ownership on Core0. Use the already-debounced
    // gameplay GPIO state so every physical GP2..GP20 key, including controls
    // such as Turbo that may not appear in the final HID button report, wakes
    // the display without consuming or rewriting the input.
    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    if (gamepad != nullptr &&
        (static_cast<uint32_t>(gamepad->debouncedGpio) &
         SCROLLWHEEL_GAMEPLAY_GPIO_MASK) != 0) {
        g_scrollWheelLastActivityMs.store(now, std::memory_order_release);
    }

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
