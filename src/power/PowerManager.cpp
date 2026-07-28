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
    updateBattery();

    if (state_ == PowerState::Critical) {
        return; // outputs stay blanked until the battery itself recovers; idle timeout doesn't apply
    }

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

void PowerManager::setBatteryVoltageReader(BatteryVoltageReader reader)
{
    batteryVoltageReader_ = reader;
}

void PowerManager::setStubBatteryVoltage(float volts)
{
    manualVoltageOverrideActive_ = true;
    manualVoltageOverrideV_ = volts;
}

void PowerManager::clearBatteryVoltageOverride()
{
    manualVoltageOverrideActive_ = false;
}

bool PowerManager::batteryAvailable() const
{
    return batteryAvailable_;
}

float PowerManager::batteryVoltageV() const
{
    return batteryVoltageV_;
}

uint8_t PowerManager::batteryPercent() const
{
    return batteryPercent_;
}

bool PowerManager::pollBatteryEvent(BatteryEventType& outEvent)
{
    if (beQueueCount_ == 0) {
        return false;
    }

    outEvent = batteryEventQueue_[beQueueHead_];
    beQueueHead_ = (beQueueHead_ + 1) % BATTERY_EVENT_QUEUE_SIZE;
    beQueueCount_--;

    return true;
}

uint8_t PowerManager::voltageToPercent(float voltageV)
{
    if (voltageV <= BATTERY_EMPTY_V) {
        return 0;
    }

    if (voltageV >= BATTERY_FULL_V) {
        return 100;
    }

    const float pct = (voltageV - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;
    return static_cast<uint8_t>(pct + 0.5f);
}

void PowerManager::updateBattery()
{
    float voltage;

    if (manualVoltageOverrideActive_) {
        voltage = manualVoltageOverrideV_;
    } else if (batteryVoltageReader_ != nullptr) {
        voltage = batteryVoltageReader_();
    } else {
        batteryAvailable_ = false;
        return;
    }

    batteryAvailable_ = true;
    batteryVoltageV_ = voltage;

    const uint8_t percent = voltageToPercent(voltage);
    batteryPercent_ = percent;

    if (percent <= BATTERY_CRITICAL_PCT) {
        if (!critical5Fired_) {
            critical5Fired_ = true;
            pushBatteryEvent(BatteryEventType::Critical5);
            enterCritical();
        }
    } else if (percent <= BATTERY_WARNING_10_PCT) {
        if (!warning10Fired_) {
            warning10Fired_ = true;
            pushBatteryEvent(BatteryEventType::Warning10);
        }
    } else if (percent <= BATTERY_WARNING_20_PCT) {
        if (!warning20Fired_) {
            warning20Fired_ = true;
            pushBatteryEvent(BatteryEventType::Warning20);
        }
    }

    // Re-arm each threshold once the level recovers past it with
    // margin (e.g. charger connected), so warnings fire again on the
    // next discharge cycle instead of staying latched forever.
    if (percent >= BATTERY_WARNING_10_PCT + BATTERY_REARM_HYSTERESIS_PCT) {
        warning10Fired_ = false;
    }
    if (percent >= BATTERY_WARNING_20_PCT + BATTERY_REARM_HYSTERESIS_PCT) {
        warning20Fired_ = false;
    }

    if (state_ == PowerState::Critical && percent >= BATTERY_CRITICAL_PCT + BATTERY_REARM_HYSTERESIS_PCT) {
        critical5Fired_ = false;
        exitCritical();
    }
}

void PowerManager::pushBatteryEvent(BatteryEventType event)
{
    if (beQueueCount_ >= BATTERY_EVENT_QUEUE_SIZE) {
        beQueueHead_ = (beQueueHead_ + 1) % BATTERY_EVENT_QUEUE_SIZE;
        beQueueCount_--;
    }

    batteryEventQueue_[beQueueTail_] = event;
    beQueueTail_ = (beQueueTail_ + 1) % BATTERY_EVENT_QUEUE_SIZE;
    beQueueCount_++;
}

void PowerManager::enterStandby()
{
    state_ = PowerState::Standby;
    blankOutputs();
}

void PowerManager::exitStandby()
{
    state_ = PowerState::Active;
    restoreOutputs();
}

void PowerManager::enterCritical()
{
    state_ = PowerState::Critical;
    blankOutputs();
}

void PowerManager::exitCritical()
{
    state_ = PowerState::Active;
    restoreOutputs();
    lastActivityMs_ = millis(); // don't immediately fall straight back into standby
}

void PowerManager::blankOutputs()
{
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

void PowerManager::restoreOutputs()
{
    context_->tft.wake();

    for (uint8_t i = 0; i < static_cast<uint8_t>(DisplayId::Count); ++i) {
        context_->numericDisplays.setVisible(static_cast<DisplayId>(i), true);
    }

    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::Count); ++i) {
        context_->buttonLights.clearOverride(static_cast<ButtonId>(i));
    }
}
