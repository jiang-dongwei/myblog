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
    MAIN    = 0,  // Level 0: RP2350, ESP32C6, RGB Customize
    RGB_SUB = 1,  // Level 1: Top Board, Bottom Board, Button
    COLOR   = 2,  // Level 2: color names
    INFO    = 3,  // Info pages (RP2350/ESP32C6)
};

struct SWMenuItem {
    const char* label;
    SWMenuLevel targetLevel;   // Level to enter on selection, or INFO for leaf
    uint8_t     targetIndex;   // Pre-set index when entering that level (unused for INFO)
};

// Forward-declare the menu table arrays
extern const SWMenuItem kMenuMain[];
extern const uint8_t      kMenuMainCount;

extern const SWMenuItem kMenuRgbSub[];
extern const uint8_t      kMenuRgbSubCount;

extern const SWMenuItem kMenuColors[];
extern const uint8_t      kMenuColorsCount;

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
// 0xFF = not set (use default DIP cycling / white flash).
// 0–7 = kMenuColors index (Red..White).
extern volatile uint8_t g_menuRgbTop;     // GP22 12-LED chain
extern volatile uint8_t g_menuRgbBottom;  // GP40 19-LED chain
extern volatile uint8_t g_menuRgbButton;  // button-press flash color

// Target being configured while the COLOR level is shown.
// 0 = Top Board, 1 = Bottom Board, 2 = Button.  Set on entry from RGB_SUB.
extern volatile uint8_t g_menuRgbTarget;

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

    // Stored parent indices for BACK navigation
    uint8_t mainIndex = 0;       // which item was selected in level 0
    uint8_t rgbSubIndex = 0;     // which item was selected in level 1
};

#endif
