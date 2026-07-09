#ifndef _SCROLLWHEEL_MENU_H_
#define _SCROLLWHEEL_MENU_H_

#include "BoardConfig.h"
#include "gpaddon.h"

#include "pico/stdlib.h"

#ifndef SCROLLWHEEL_MENU_ENABLED
#define SCROLLWHEEL_MENU_ENABLED 1
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

#ifndef SCROLLWHEEL_LONG_PRESS_MS
#define SCROLLWHEEL_LONG_PRESS_MS 3000
#endif

// GP30 5-state FSM timing parameters (hardcoded in updateButton)
#ifndef SCROLLWHEEL_LONG_PRESS_MS
#define SCROLLWHEEL_LONG_PRESS_MS 3000
#endif

#ifndef SCROLLWHEEL_ROTARY_DEBOUNCE_MS
#define SCROLLWHEEL_ROTARY_DEBOUNCE_MS 80
#endif

#define ScrollWheelMenuName "ScrollWheelMenu"

// ── Menu tree definition (shared between Core0 nav and Core1 render) ─────

enum class SWMenuLevel : uint8_t {
    MAIN          = 0,  // Level 0: RP2350, ESP32C6, RGB Customize
    RGB_SUB       = 1,  // Level 1: Button, ButtonEffect, AmbientEffect, OFF
    COLOR         = 2,  // Level 2: color names (Button RGB flash)
    INFO          = 3,  // Info pages (RP2350/ESP32C6)
    BUTTON_EFFECT = 4,  // Level 2: AnimationEffects picker
    AMBIENT_EFFECT= 5,  // Level 2: AmbientEffectType picker
    COLOR_BTN     = 6,  // Level 3: color picker under Button LED Effect → Static Color
    COLOR_AMB     = 7,  // Level 3: color picker under Ambient LED Effect → Static Color
};

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

extern const SWMenuItem kMenuButtonEffects[];
extern const uint8_t      kMenuButtonEffectsCount;

extern const SWMenuItem kMenuAmbientEffects[];
extern const uint8_t      kMenuAmbientEffectsCount;

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

// Set true as soon as GP30 is pressed (FSM leaves IDLE); cleared on release.
// FightpadAmbientLEDAddon checks this to avoid processing GP30 as a DIP toggle
// during the long-press window before g_scrollWheelMenuActive becomes true.
extern volatile bool g_scrollWheelButtonBusy;

// Set true when a GP30 long press (≥3s) is detected; cleared on release.
// FightpadAmbientLEDAddon checks this to suppress the ON/OFF release edge
// when the button release follows a long press (menu enter or exit).
extern volatile bool g_scrollWheelButtonLongPressed;

// ── RGB color overrides set from the menu ───────────────────────────────
// Each stores an AnimationStation `colors` vector index (0..15), or 0xFF
// for "not set" (use default DIP cycling / white flash).
//  0 = ColorBlack (OFF), 1 = ColorWhite, 2 = ColorRed, 3 = ColorOrange,
//  4 = ColorYellow, 5 = ColorLimeGreen, 6 = ColorGreen, 7 = ColorSeafoam,
//  8 = ColorAqua, 9 = ColorSkyBlue, 10 = ColorBlue, 11 = ColorPurple,
//  12 = ColorPink, 13 = ColorMagenta, 14 = ColorIndigo, 15 = ColorViolet.
extern volatile uint8_t g_menuRgbTop;     // GP22 12-LED chain
extern volatile uint8_t g_menuRgbBottom;  // GP40 19-LED chain
extern volatile uint8_t g_menuRgbButton;  // button-press flash color

// Target being configured while the COLOR level is shown.
// 0 = Top Board, 1 = Bottom Board, 2 = Button.  Set on entry from RGB_SUB.
extern volatile uint8_t g_menuRgbTarget;

// ── RGB effect overrides set from the menu ──────────────────────────────
// 0xFF = not set (use default static-color breathing).
// 0-4 = AnimationEffects / AmbientEffectType enum value.
extern volatile uint8_t g_menuButtonEffect;   // GP22 button LED effect
extern volatile uint8_t g_menuAmbientEffect;  // GP40 ambient LED effect

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

    // Menu navigation
    void navUp();       // GP31 edge → move cursor up
    void navDown();     // GP32 edge → move cursor down
    void navSelect();   // GP30 short press → enter/back
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
