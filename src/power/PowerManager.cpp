#include "power/PowerManager.h"

#include <Arduino.h>

void PowerManager::begin(GameModeContext& context, unsigned long standbyTimeoutMs)
{
    context_ = &context;
    standbyTimeoutMs_ = standbyTimeoutMs;
    lastActivityMs_ = millis();
    state_ = PowerState::Active;
}

void PowerManager::update()
{
    if (state_ == PowerState::Active &&
        millis() - lastActivityMs_ >= standbyTimeoutMs_) {
        enterStandby();
    }
}

void PowerManager::notifyActivity()
{
    lastActivityMs_ = millis();

    if (state_ == PowerState::Standby) {
        exitStandby();
    }
}

PowerState PowerManager::state() const
{
    return state_;
}

void PowerManager::setStandbyTimeoutMs(unsigned long timeoutMs)
{
    standbyTimeoutMs_ = timeoutMs;
}

void PowerManager::enterStandby()
{
    state_ = PowerState::Standby;

    context_->tft.sleep();

    for (uint8_t i = 0; i < static_cast<uint8_t>(DisplayId::Count); ++i) {
        context_->numericDisplays.setVisible(static_cast<DisplayId>(i), false);
    }

    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::Count); ++i) {
        context_->buttonLights.setTemporaryOverride(
            static_cast<ButtonId>(i),
            LightPattern::Off,
            0,
            0,
            255
        );
    }
}

void PowerManager::exitStandby()
{
    state_ = PowerState::Active;

    context_->tft.wake();

    for (uint8_t i = 0; i < static_cast<uint8_t>(DisplayId::Count); ++i) {
        context_->numericDisplays.setVisible(static_cast<DisplayId>(i), true);
    }

    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::Count); ++i) {
        context_->buttonLights.clearOverride(static_cast<ButtonId>(i));
    }
}
