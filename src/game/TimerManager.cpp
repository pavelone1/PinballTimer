#include "game/TimerManager.h"

#include <Arduino.h>

void TimerManager::begin()
{
    for (uint8_t i = 0; i < MAX_TIMERS; ++i) {
        timers_[i] = Timer{};
    }
}

void TimerManager::update()
{
    const unsigned long now = millis();

    for (uint8_t i = 0; i < MAX_TIMERS; ++i) {
        Timer& t = timers_[i];

        if (!t.inUse) {
            continue;
        }

        if (!t.running) {
            t.lastTickMillis = now;
            continue;
        }

        const unsigned long deltaMs = now - t.lastTickMillis;
        t.lastTickMillis = now;

        const long signedDelta = t.direction == CountDirection::CountDown
            ? -static_cast<long>(deltaMs)
            : static_cast<long>(deltaMs);

        const long previousValueMs = t.valueMs;
        long newValueMs = t.valueMs + signedDelta;

        const bool crossedZero =
            (previousValueMs >= 0 && newValueMs < 0) ||
            (previousValueMs < 0 && newValueMs >= 0);

        if (!t.allowBelowZero && newValueMs < 0) {
            if (t.loopEnabled) {
                newValueMs = t.initialValueMs;
            } else {
                newValueMs = 0;
                if (t.stopAtZero) {
                    t.running = false;
                }
            }
        }

        t.valueMs = newValueMs;

        if (crossedZero) {
            t.warningFired = false;
            pushZeroCrossing(static_cast<TimerId>(i));
        }

        if (t.hasWarningThreshold && !t.warningFired) {
            const bool pastThreshold = t.direction == CountDirection::CountDown
                ? (t.valueMs <= t.warningThresholdMs && previousValueMs > t.warningThresholdMs)
                : (t.valueMs >= t.warningThresholdMs && previousValueMs < t.warningThresholdMs);

            if (pastThreshold) {
                t.warningFired = true;
                pushWarning(static_cast<TimerId>(i));
            }
        }
    }
}

TimerId TimerManager::createTimer(
    CountDirection direction,
    long initialValueSeconds,
    bool allowBelowZero,
    bool stopAtZero,
    bool loopEnabled
)
{
    for (uint8_t i = 0; i < MAX_TIMERS; ++i) {
        if (!timers_[i].inUse) {
            Timer& t = timers_[i];
            t = Timer{};
            t.inUse = true;
            t.direction = direction;
            t.initialValueMs = initialValueSeconds * 1000L;
            t.valueMs = t.initialValueMs;
            t.allowBelowZero = allowBelowZero;
            t.stopAtZero = stopAtZero;
            t.loopEnabled = loopEnabled;
            t.lastTickMillis = millis();
            return static_cast<TimerId>(i);
        }
    }

    return INVALID_TIMER;
}

void TimerManager::destroyTimer(TimerId id)
{
    if (id >= MAX_TIMERS) {
        return;
    }

    timers_[id] = Timer{};
}

void TimerManager::start(TimerId id)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    timers_[id].running = true;
    timers_[id].lastTickMillis = millis();
}

void TimerManager::stop(TimerId id)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    timers_[id].running = false;
}

void TimerManager::reset(TimerId id, long initialValueSeconds)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    Timer& t = timers_[id];
    t.initialValueMs = initialValueSeconds * 1000L;
    t.valueMs = t.initialValueMs;
    t.warningFired = false;
    t.lastTickMillis = millis();
}

bool TimerManager::isRunning(TimerId id) const
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return false;
    }

    return timers_[id].running;
}

long TimerManager::currentValueSeconds(TimerId id) const
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return 0;
    }

    return timers_[id].valueMs / 1000L;
}

void TimerManager::setWarningThreshold(TimerId id, long thresholdSeconds)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    Timer& t = timers_[id];
    t.hasWarningThreshold = true;
    t.warningThresholdMs = thresholdSeconds * 1000L;
    t.warningFired = false;
}

void TimerManager::clearWarningThreshold(TimerId id)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    timers_[id].hasWarningThreshold = false;
}

void TimerManager::setAssociatedPlayer(TimerId id, PlayerId player)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    timers_[id].hasAssociatedPlayer = true;
    timers_[id].associatedPlayer = player;
}

bool TimerManager::associatedPlayer(TimerId id, PlayerId& outPlayer) const
{
    if (id >= MAX_TIMERS || !timers_[id].inUse || !timers_[id].hasAssociatedPlayer) {
        return false;
    }

    outPlayer = timers_[id].associatedPlayer;
    return true;
}

void TimerManager::setDisplayAssignment(TimerId id, DisplayId display)
{
    if (id >= MAX_TIMERS || !timers_[id].inUse) {
        return;
    }

    timers_[id].hasDisplayAssignment = true;
    timers_[id].displayAssignment = display;
}

bool TimerManager::displayAssignment(TimerId id, DisplayId& outDisplay) const
{
    if (id >= MAX_TIMERS || !timers_[id].inUse || !timers_[id].hasDisplayAssignment) {
        return false;
    }

    outDisplay = timers_[id].displayAssignment;
    return true;
}

bool TimerManager::pollZeroCrossingEvent(TimerId& outId)
{
    if (zcQueueCount_ == 0) {
        return false;
    }

    outId = zeroCrossingQueue_[zcQueueHead_];
    zcQueueHead_ = (zcQueueHead_ + 1) % EVENT_QUEUE_SIZE;
    zcQueueCount_--;

    return true;
}

bool TimerManager::pollWarningEvent(TimerId& outId)
{
    if (warnQueueCount_ == 0) {
        return false;
    }

    outId = warningQueue_[warnQueueHead_];
    warnQueueHead_ = (warnQueueHead_ + 1) % EVENT_QUEUE_SIZE;
    warnQueueCount_--;

    return true;
}

void TimerManager::pushZeroCrossing(TimerId id)
{
    if (zcQueueCount_ >= EVENT_QUEUE_SIZE) {
        zcQueueHead_ = (zcQueueHead_ + 1) % EVENT_QUEUE_SIZE;
        zcQueueCount_--;
    }

    zeroCrossingQueue_[zcQueueTail_] = id;
    zcQueueTail_ = (zcQueueTail_ + 1) % EVENT_QUEUE_SIZE;
    zcQueueCount_++;
}

void TimerManager::pushWarning(TimerId id)
{
    if (warnQueueCount_ >= EVENT_QUEUE_SIZE) {
        warnQueueHead_ = (warnQueueHead_ + 1) % EVENT_QUEUE_SIZE;
        warnQueueCount_--;
    }

    warningQueue_[warnQueueTail_] = id;
    warnQueueTail_ = (warnQueueTail_ + 1) % EVENT_QUEUE_SIZE;
    warnQueueCount_++;
}
