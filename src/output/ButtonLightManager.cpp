#include "output/ButtonLightManager.h"

#include <Arduino.h>
#include "HardwarePins.h"

void ButtonLightManager::begin()
{
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        pinMode(HardwarePins::BUTTON_LIGHTS[i], OUTPUT);
        digitalWrite(HardwarePins::BUTTON_LIGHTS[i], LOW);
        state_[i].dirty = true;
    }
}

void ButtonLightManager::update()
{
    const unsigned long now = millis();

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        ButtonLightState& s = state_[i];

        if (s.hasOverride && s.overrideExpires && now >= s.overrideExpiresMs) {
            s.hasOverride = false;
            s.dirty = true;
        }

        const LightState& active = s.hasOverride ? s.override_ : s.base;

        if (active.pattern == LightPattern::Blink &&
            now - s.lastBlinkToggleMs >= active.blinkIntervalMs) {
            s.lastBlinkToggleMs = now;
            s.blinkOnPhase = !s.blinkOnPhase;
            s.dirty = true;
        }

        if (s.dirty) {
            render(i);
        }
    }
}

void ButtonLightManager::setBaseState(ButtonId button, LightPattern pattern, unsigned long blinkIntervalMs)
{
    ButtonLightState& s = state_[static_cast<uint8_t>(button)];
    s.base.pattern = pattern;
    s.base.blinkIntervalMs = blinkIntervalMs;
    s.blinkOnPhase = true;
    s.lastBlinkToggleMs = millis();
    s.dirty = true;
}

void ButtonLightManager::setTemporaryOverride(
    ButtonId button,
    LightPattern pattern,
    unsigned long blinkIntervalMs,
    unsigned long durationMs,
    uint8_t priority
)
{
    ButtonLightState& s = state_[static_cast<uint8_t>(button)];

    if (s.hasOverride && s.overridePriority > priority) {
        return;
    }

    s.override_.pattern = pattern;
    s.override_.blinkIntervalMs = blinkIntervalMs;
    s.overridePriority = priority;
    s.overrideExpires = durationMs != 0;
    s.overrideExpiresMs = millis() + durationMs;
    s.hasOverride = true;
    s.blinkOnPhase = true;
    s.lastBlinkToggleMs = millis();
    s.dirty = true;
}

void ButtonLightManager::clearOverride(ButtonId button)
{
    ButtonLightState& s = state_[static_cast<uint8_t>(button)];
    if (s.hasOverride) {
        s.hasOverride = false;
        s.blinkOnPhase = true;
        s.lastBlinkToggleMs = millis();
        s.dirty = true;
    }
}

bool ButtonLightManager::isOn(ButtonId button) const
{
    return state_[static_cast<uint8_t>(button)].currentOn;
}

void ButtonLightManager::render(uint8_t index)
{
    ButtonLightState& s = state_[index];
    const LightState& active = s.hasOverride ? s.override_ : s.base;

    bool desiredOn = false;
    switch (active.pattern) {
        case LightPattern::Off:
            desiredOn = false;
            break;
        case LightPattern::Solid:
            desiredOn = true;
            break;
        case LightPattern::Blink:
            desiredOn = s.blinkOnPhase;
            break;
    }

    if (desiredOn != s.currentOn) {
        s.currentOn = desiredOn;
        digitalWrite(HardwarePins::BUTTON_LIGHTS[index], desiredOn ? HIGH : LOW);
    }

    s.dirty = false;
}
