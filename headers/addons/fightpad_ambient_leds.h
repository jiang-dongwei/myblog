#ifndef _FIGHTPAD_AMBIENT_LEDS_H_
#define _FIGHTPAD_AMBIENT_LEDS_H_

#include "BoardConfig.h"
#include "NeoPico.h"
#include "animation.h"
#include "gpaddon.h"
#include "helper.h"

#include "hardware/pio.h"
#include "pico/stdlib.h"

#ifndef FIGHTPAD12SLIM_AMBIENT_ENABLED
#define FIGHTPAD12SLIM_AMBIENT_ENABLED 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_LEDS_PIN
#define FIGHTPAD12SLIM_AMBIENT_LEDS_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN
#define FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN FIGHTPAD12SLIM_AMBIENT_LEDS_PIN
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT
#define FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT 1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN
#define FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_PREV_PIN
#define FIGHTPAD12SLIM_AMBIENT_PREV_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_NEXT_PIN
#define FIGHTPAD12SLIM_AMBIENT_NEXT_PIN -1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_DIP_SELECTOR_MODE
#define FIGHTPAD12SLIM_AMBIENT_DIP_SELECTOR_MODE 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_CONTROLS_ACTIVE_LOW
#define FIGHTPAD12SLIM_AMBIENT_CONTROLS_ACTIVE_LOW 1
#endif

#ifndef FIGHTPAD12SLIM_BOOST_EN_PIN
#define FIGHTPAD12SLIM_BOOST_EN_PIN -1
#endif

#ifndef LED_FORMAT
#define LED_FORMAT LED_FORMAT_GRB
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_LED_FORMAT
#define FIGHTPAD12SLIM_AMBIENT_LED_FORMAT LED_FORMAT
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_PIO
#define FIGHTPAD12SLIM_AMBIENT_PIO pio0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_PIO_SM
#define FIGHTPAD12SLIM_AMBIENT_PIO_SM 2
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_GP22_PIO_SM
#define FIGHTPAD12SLIM_AMBIENT_GP22_PIO_SM 1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE
#define FIGHTPAD12SLIM_AMBIENT_PIO_GPIO_BASE 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS
#define FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS 0.25f
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_BOOT_ENABLE
#define FIGHTPAD12SLIM_AMBIENT_BOOT_ENABLE 1
#endif

#ifndef FIGHTPAD12SLIM_ESP32_BT_STATUS_DUAL_CHASE
#define FIGHTPAD12SLIM_ESP32_BT_STATUS_DUAL_CHASE 0
#endif

// Normal menu Chase color cadence and per-chain full-cycle timing. Bluetooth
// Pairing/Connecting has its own fixed 50 ms timing and remains independent.
#ifndef FIGHTPAD12SLIM_LIGHT_CHASE_STEP_MS
#define FIGHTPAD12SLIM_LIGHT_CHASE_STEP_MS 100
#endif

#ifndef FIGHTPAD12SLIM_GP22_LIGHT_CHASE_CYCLE_MS
#define FIGHTPAD12SLIM_GP22_LIGHT_CHASE_CYCLE_MS 2000
#endif

#ifndef FIGHTPAD12SLIM_GP40_LIGHT_CHASE_CYCLE_MS
#define FIGHTPAD12SLIM_GP40_LIGHT_CHASE_CYCLE_MS 2000
#endif

#ifndef FIGHTPAD12SLIM_BUTTON_LEDS_COUNT
#define FIGHTPAD12SLIM_BUTTON_LEDS_COUNT 12
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT
#define FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT FIGHTPAD12SLIM_BUTTON_LEDS_COUNT
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_DRIVE_BOOST_EN
#define FIGHTPAD12SLIM_AMBIENT_DRIVE_BOOST_EN 1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_BOOST_EXTERNAL_PULLUP
#define FIGHTPAD12SLIM_AMBIENT_BOOST_EXTERNAL_PULLUP 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL
#define FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL 1
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF
#define FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_BOOST_STARTUP_DELAY_MS
#define FIGHTPAD12SLIM_AMBIENT_BOOST_STARTUP_DELAY_MS 5
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_BOOST_SHUTDOWN_DELAY_US
#define FIGHTPAD12SLIM_AMBIENT_BOOST_SHUTDOWN_DELAY_US 1000
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_POWER_ONLY_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_POWER_ONLY_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_CONTROL_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_CONTROL_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_GPIO40_INIT_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_GPIO40_INIT_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_PIO_SETUP_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_PIO_SETUP_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_ZERO_SHOW_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_ZERO_SHOW_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_FORCE_SOLID_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_FORCE_SOLID_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_PIXEL_SCAN_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_PIXEL_SCAN_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_MS
#define FIGHTPAD12SLIM_AMBIENT_GPIO_SQUARE_MS 500
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN
#define FIGHTPAD12SLIM_AMBIENT_DIAGNOSTIC_LEDS_PIN FIGHTPAD12SLIM_AMBIENT_LEDS_PIN
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_STARTUP_BLINK_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_STARTUP_BLINK_DIAGNOSTIC 0
#endif

#ifndef FIGHTPAD12SLIM_AMBIENT_RENDER_TOGGLE_DIAGNOSTIC
#define FIGHTPAD12SLIM_AMBIENT_RENDER_TOGGLE_DIAGNOSTIC 0
#endif

#define FightpadAmbientLEDName "FightpadAmbientLED"

extern volatile uint8_t fightpadAmbientDiagControls;
extern volatile uint8_t fightpadAmbientDiagDataOut;
extern volatile uint8_t fightpadAmbientDiagEffect;
extern volatile uint8_t fightpadAmbientDiagEnabled;
extern volatile uint32_t fightpadAmbientDiagTicks;
extern volatile uint32_t fightpadAmbientDiagShows;

class FightpadAmbientLEDAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess() {}
    virtual void process();
    virtual void postprocess(bool sent) {}
    virtual void reinit() {}
    virtual std::string name() { return FightpadAmbientLEDName; }

private:
    static constexpr uint8_t CONTROL_ONOFF = 0x01;
    static constexpr uint8_t CONTROL_PREV = 0x02;
    static constexpr uint8_t CONTROL_NEXT = 0x04;
    static constexpr uint8_t EFFECT_COUNT = 8;
    static constexpr uint32_t UPDATE_INTERVAL_MS = 20;
    static constexpr uint32_t CONTROL_DEBOUNCE_MS = 180;
    static constexpr uint32_t BUTTON_FLASH_MS = 80;

    uint8_t readControls();
    bool isPressed(int pin);
    void updateButtonFlash(uint32_t now);
    void handleControlEdges(uint8_t controls, uint32_t now);
    void previousEffect();
    void nextEffect();
    void render(uint32_t now);
    void updateLightEffectAnimation(uint32_t now);
    void renderAmbient(uint32_t now);   // GP40 19-LED chain effects
    void renderBluetoothStatusAmbient(uint8_t status, uint32_t elapsedMs);
    void renderButtons(uint32_t now);   // GP22 12-LED chain effects
    void show();
    void setBoostPower(bool powerEnabled);
    void fill(RGB color, float brightness = FIGHTPAD12SLIM_AMBIENT_BRIGHTNESS);
    void clearFrame();
    float getBreathBrightness(uint32_t now);

    NeoPico neopico;
    NeoPico neopico_gp22;
    uint32_t frame[100] = {};
    uint32_t frame_gp22[100] = {};
    uint8_t lastControls = 0;
    uint8_t lastSelector = 0xFF;
    uint8_t effectIndex = 0;
    uint32_t lastControlTime = 0;
    uint32_t lastFrameTime = 0;
    uint32_t lastGamepadButtons = 0;
    uint8_t lastGamepadDpad = 0;
    uint32_t gp22FlashUntil[100] = {};
    uint32_t renderCounter = 0;
    bool enabled = FIGHTPAD12SLIM_AMBIENT_BOOT_ENABLE != 0;
    // True is also the safe fallback for boards without a controllable rail.
    bool boostPowerEnabled = true;
    // Connecting/Pairing/Connected can temporarily request GP40 power even
    // when the saved RGB mode is All OFF. Low-battery cutoff still wins.
    bool bluetoothStatusLightRequired = false;

    // ── Effect animation state ──────────────────────────────
    int16_t  wheelFrame = 0;            // shared rainbow/gradient/chase hue (0-255)
    bool     wheelReverse = false;      // shared Gradient/Rainbow bounce direction
    uint32_t wheelLastMs = 0;           // last shared Gradient/Rainbow update
    uint32_t lightChaseStartedMs = 0;   // shared epoch; each chain maps it to its own cycle
    uint32_t lightChaseLastMs = 0;      // last normal Chase hue advance timestamp
    uint8_t  lastLightEffect = 0xFF;    // resets shared phase when mode changes
};

#endif
