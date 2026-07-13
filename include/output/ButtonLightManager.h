#pragma once

#include <cstdint>
#include "SystemTypes.h"

// Physical colors are fixed per button (not RGB) -- driven digitally
// through the ULN2803 (on/off only, no PWM on this hardware, so no
// brightness field here).
enum class LightPattern : uint8_t {
    Off,
    Solid,
    Blink
};

// Controls the five illuminated buttons (4 player + Action). Stores
// a long-lived base state per button plus an optional temporary
// override (with priority and auto-expiry) that reverts back to the
// base state on its own. Does not decide what a light means -- a
// GameMode/App sets the base state and requests overrides, this
// module just renders them to hardware.
class ButtonLightManager {
public:
    void begin();
    void update();

    void setBaseState(ButtonId button, LightPattern pattern, unsigned long blinkIntervalMs = 500);

    // durationMs == 0 means the override persists until clearOverride()
    // is called explicitly. A request with lower priority than an
    // already-active override is ignored; equal or higher priority
    // replaces it.
    void setTemporaryOverride(
        ButtonId button,
        LightPattern pattern,
        unsigned long blinkIntervalMs,
        unsigned long durationMs,
        uint8_t priority
    );

    void clearOverride(ButtonId button);

    bool isOn(ButtonId button) const;

private:
    struct LightState {
        LightPattern pattern = LightPattern::Off;
        unsigned long blinkIntervalMs = 500;
    };

    struct ButtonLightState {
        LightState base;

        bool hasOverride = false;
        LightState override_;
        unsigned long overrideExpiresMs = 0;
        bool overrideExpires = false;
        uint8_t overridePriority = 0;

        bool blinkOnPhase = true;
        unsigned long lastBlinkToggleMs = 0;
        bool currentOn = false;
        bool dirty = true;
    };

    static constexpr uint8_t BUTTON_COUNT = static_cast<uint8_t>(ButtonId::Count);

    ButtonLightState state_[BUTTON_COUNT];

    void render(uint8_t index);
};
