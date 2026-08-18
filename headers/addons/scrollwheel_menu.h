#ifndef _SCROLLWHEEL_MENU_H_
#define _SCROLLWHEEL_MENU_H_

#include "BoardConfig.h"
#include "gpaddon.h"

#include "pico/stdlib.h"

#include <atomic>

#ifndef SCROLLWHEEL_MENU_ENABLED
#define SCROLLWHEEL_MENU_ENABLED 1
#endif

// Keep the Battery Info implementation compiled while allowing a board to
// hide its level-0 entry during normal use. Set to 1 for gauge debugging.
#ifndef SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED
#define SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED 1
#endif

#ifndef SCROLLWHEEL_PIN_SW
#define SCROLLWHEEL_PIN_SW 30
#endif

#ifndef SCROLLWHEEL_PIN_A
#define SCROLLWHEEL_PIN_A 31
#endif

#ifndef SCROLLWHEEL_PIN_B
#define SCROLLWHEEL_PIN_B 32
#endif

#ifndef SCROLLWHEEL_PIN_BACK
#define SCROLLWHEEL_PIN_BACK 19
#endif

#ifndef SCROLLWHEEL_ACTIVE_LOW
#define SCROLLWHEEL_ACTIVE_LOW 1
#endif

// Physical Fightpad controls that must not reach gameplay while the menu owns
// input. GP19 remains available to the menu through its direct GPIO read.
static constexpr uint32_t SCROLLWHEEL_GAMEPLAY_GPIO_MASK = 0x001FFFFCu;

#ifndef SCROLLWHEEL_LONG_PRESS_MS
#define SCROLLWHEEL_LONG_PRESS_MS 2000
#endif

#ifndef SCROLLWHEEL_ROTARY_DEBOUNCE_MS
#define SCROLLWHEEL_ROTARY_DEBOUNCE_MS 80
#endif

#define ScrollWheelMenuName "ScrollWheelMenu"

// ── Menu tree definition (shared between Core0 nav and Core1 render) ─────

enum class SWMenuLevel : uint8_t {
    MAIN          = 0,  // Level 0: RP2350, ESP32C6, RGB Customize (+ optional Battery Info)
    RGB_SUB       = 1,  // Level 1: Key Flash, Light Effect, Brightness, All OFF
    COLOR         = 2,  // Level 2: color names (Button RGB flash)
    INFO          = 3,  // Info pages (RP2350/ESP32C6)
    LIGHT_EFFECT  = 4,  // Level 2: shared GP22/GP40 effect picker
    COLOR_EFFECT  = 6,  // Level 3: shared color picker under Static Color
    COLOR_EFFECT_BREATH = 8, // Level 3: shared color picker under Breathing
    COLOR_EFFECT_CHASE = 9, // Level 3: shared color picker under Chase
    BATTERY_INFO  = 10, // Battery runtime/config/calibration/charge pages
    BRIGHTNESS    = 11, // Level 2: shared Key/Base effect brightness
    CONTROLLER_TYPE = 12, // Level 1: upstream wired USB input mode picker
    BLUETOOTH_TYPE  = 13, // Level 1: ESP32-C6 BLE HID profile picker
};

// Unified runtime effect IDs used by both the GP22 Key chain and GP40 Base
// chain. The legacy protobuf fields use different IDs and are translated at
// the scrollwheel persistence boundary.
enum SWLightEffect : uint8_t {
    LIGHT_EFFECT_STATIC_COLOR = 0,
    LIGHT_EFFECT_GRADIENT     = 1,
    LIGHT_EFFECT_BREATHING    = 2,
    LIGHT_EFFECT_RAINBOW      = 3,
    LIGHT_EFFECT_CHASE        = 4,
    LIGHT_EFFECT_COUNT        = 5,
    LIGHT_EFFECT_UNSET        = 0xFF,
};

static constexpr uint8_t SW_BATTERY_PAGE_COUNT = 4;

struct SWMenuItem {
    const char* label;
    SWMenuLevel targetLevel;   // Level to enter on selection, or INFO for leaf
    // For COLOR items (targetLevel==INFO): AnimationStation `colors` vector index (0..15).
    // For non-COLOR items: pre-set index when entering that level (unused for INFO).
    uint8_t     targetIndex;
};

// Forward-declare the menu table arrays
extern const SWMenuItem kMenuMain[];
extern const uint8_t      kMenuMainCount;

extern const SWMenuItem kMenuRgbSub[];
extern const uint8_t      kMenuRgbSubCount;

extern const SWMenuItem kMenuColors[];
extern const uint8_t      kMenuColorsCount;

extern const SWMenuItem kMenuChaseColors[];
extern const uint8_t      kMenuChaseColorsCount;

extern const SWMenuItem kMenuLightEffects[];
extern const uint8_t      kMenuLightEffectsCount;

extern const SWMenuItem kMenuBrightness[];
extern const uint8_t      kMenuBrightnessCount;

extern const SWMenuItem kMenuControllerTypes[];
extern const uint8_t      kMenuControllerTypesCount;

extern const SWMenuItem kMenuBluetoothTypes[];
extern const uint8_t      kMenuBluetoothTypesCount;

// ── Cross-core state ─────────────────────────────────────────────────────

struct ScrollWheelMenuState {
    bool    active;        // menu is being shown on OLED
    uint8_t level;         // current SWMenuLevel value
    uint8_t index;         // selected item index
    uint8_t scrollOffset;  // first visible item index
    uint8_t infoSource;    // level that sent us to INFO (0=MAIN, 1=COLOR)
};

// Written by Core0, read by Core1.
extern volatile ScrollWheelMenuState g_menuState;

// Written by Core0 whenever g_menuState changes meaningfully.
extern volatile bool g_menuStateDirty;

// Written by Core0, read by Core1 (DisplayAddon) and FightpadAmbientLEDAddon.
extern volatile bool g_scrollWheelMenuActive;

// Core0-only gameplay ownership gate. It stays asserted after the menu closes
// until every GP2..GP20 input has been released and debounced.
bool isScrollWheelGameplayInputLocked();

// Set true as soon as GP30 is pressed (FSM leaves IDLE); cleared on release.
// FightpadAmbientLEDAddon checks this to avoid processing GP30 as a DIP toggle
// during the long-press window before g_scrollWheelMenuActive becomes true.
extern volatile bool g_scrollWheelButtonBusy;

// Set true when a GP30 long press (≥2s) is detected; cleared on release.
// FightpadAmbientLEDAddon checks this to suppress the ON/OFF release edge
// when the button release follows a long press (menu enter or exit).
extern volatile bool g_scrollWheelButtonLongPressed;

// Last activity from GP2..GP20 or raw edge on GP30/GP31/GP32.
// Written by Core0 and read by Core1 to control Fightpad OLED idle sleep.
extern std::atomic<uint32_t> g_scrollWheelLastActivityMs;

// ── RGB color overrides set from the menu ───────────────────────────────
// Each stores an AnimationStation `colors` vector index (0..15), or 0xFF.
// For the shared effect color, 0xFF selects Rainbow while Chase is active;
// for button flash it means "not set" and uses the default white flash.
//  0 = ColorBlack (OFF), 1 = ColorWhite, 2 = ColorRed, 3 = ColorOrange,
//  4 = ColorYellow, 5 = ColorLimeGreen, 6 = ColorGreen, 7 = ColorSeafoam,
//  8 = ColorAqua, 9 = ColorSkyBlue, 10 = ColorBlue, 11 = ColorPurple,
//  12 = ColorPink, 13 = ColorMagenta, 14 = ColorIndigo, 15 = ColorViolet.
extern volatile uint8_t g_menuRgbEffectColor; // shared GP22/GP40 effect color
extern volatile uint8_t g_menuRgbButton;  // button-press flash color

// ── Shared RGB effect override set from the menu ────────────────────────
// Uses SWLightEffect. 0xFF is reserved for the persisted All OFF state.
extern volatile uint8_t g_menuLightEffect;

// Runtime request for the shared GP22/GP40 RGB power rail.  The menu only
// changes this request; FightpadAmbientLEDAddon remains the sole GP24 writer.
// "All OFF" clears it and selecting a visible color/effect sets it again.
extern volatile bool g_menuRgbPowerEnabled;

// Persisted GP30 master switch for normal Light Effect and Key Flash output.
// It is intentionally separate from the menu's existing destructive All OFF
// state. Bluetooth GP40 status feedback may still request temporary light
// output while this switch is false.
extern volatile bool g_manualLightEffectsEnabled;

// One-shot RAM-only blackout used to make a pending Controller Type reboot
// visible. It is never persisted, so the saved light effect returns on boot.
extern volatile bool g_scrollWheelRebootBlackout;

// BLE Profile changes are written asynchronously by GP2040 at the end of the
// Core0 loop.  The ESP32 proxy must not transmit the RAM-only value before the
// flash write succeeds.
bool isFightpadBluetoothProfileSavePending();
void handleFightpadBluetoothProfileSaveResult(bool successful);

// Shared brightness for Key/Base Static Color, Gradient and Rainbow.
// 0 = Bright (0.5f), 1 = Normal (0.3f), 2 = Dim (0.1f).
extern volatile uint8_t g_menuBrightnessLevel;

// ── GPAddon (Core0) ──────────────────────────────────────────────────────

class ScrollWheelMenuAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess() {}
    virtual void process();
    virtual void postprocess(bool sent) {}
    virtual void reinit() {}
    virtual std::string name() { return ScrollWheelMenuName; }

private:
    // GPIO helpers
    bool readPin(int pin) const;
    void initPin(int pin);

    // GP30: 5-state button FSM (0x1abin/MultiButton classic pattern)
    void updateButton(uint32_t now);

    // Menu gameplay-input ownership state machine (Core0 only)
    void updateGameplayInputLock(uint32_t now);

    // Menu navigation
    void navUp();       // GP31 edge → move cursor up
    void navDown();     // GP32 edge → move cursor down
    void navSelect();   // GP30 short press -> runtime lights/menu select
    void navToggle();   // GP30 long press → enter/exit menu
    void navBack();     // GP19 short press → back one level / exit

    // Helpers
    uint8_t         currentItemCount() const;
    const SWMenuItem* currentMenuTable() const;

    // ── Button FSM state ────────────────────────────────────
    enum BtnState : uint8_t {
        BTN_IDLE,
        BTN_DEBOUNCE_PRESS,
        BTN_PRESS,
        BTN_LONG,
        BTN_DEBOUNCE_RELEASE,
    };
    BtnState  btnState        = BTN_IDLE;
    uint32_t  btnTimer         = 0;   // stage timer, reset on every transition
    bool      debouncedButton  = false; // 30ms-filtered signal (FSM reads ONLY this)
    bool      btnFromLong      = false; // true when DEBOUNCE_RELEASE originated from LONG
    bool      prevButtonRaw    = false; // raw GP30 state for OLED activity/wake

    // Rotary edge detection
    bool prevA = false;
    bool prevB = false;
    uint32_t lastRotaryTime = 0;

    // GP19 BACK button
    bool prevBack = false;
    uint32_t lastBackTime = 0;

    // Stored parent indices for BACK navigation
    uint8_t mainIndex = 0;       // which item was selected in level 0
    uint8_t rgbSubIndex = 0;     // which item was selected in level 1
};

#endif
