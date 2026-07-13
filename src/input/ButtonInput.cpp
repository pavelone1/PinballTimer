#include "input/ButtonInput.h"

#include <Arduino.h>
#include "HardwarePins.h"

void ButtonInput::begin()
{
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        pinMode(HardwarePins::BUTTON_SWITCHES[i], INPUT_PULLUP);
    }
}

void ButtonInput::update()
{
    const unsigned long now = millis();

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool raw = digitalRead(HardwarePins::BUTTON_SWITCHES[i]) == LOW;
        const ButtonId button = static_cast<ButtonId>(i);

        if (raw != rawState_[i]) {
            rawState_[i] = raw;
            lastChangeMs_[i] = now;
        }

        if (now - lastChangeMs_[i] >= DEBOUNCE_MS && stableState_[i] != rawState_[i]) {
            stableState_[i] = rawState_[i];

            if (stableState_[i]) {
                pressStartMs_[i] = now;
                longPressFired_[i] = false;
                pushEvent(button, ButtonEventType::Pressed, now);
            } else {
                pushEvent(button, ButtonEventType::Released, now);

                if (!longPressFired_[i]) {
                    pushEvent(button, ButtonEventType::ShortPress, now);
                }
            }
        }

        if (stableState_[i] && !longPressFired_[i] &&
            now - pressStartMs_[i] >= LONG_PRESS_MS) {
            longPressFired_[i] = true;
            pushEvent(button, ButtonEventType::LongPress, now);
        }
    }
}

bool ButtonInput::isPressed(ButtonId button) const
{
    return stableState_[static_cast<uint8_t>(button)];
}

unsigned long ButtonInput::heldDurationMs(ButtonId button) const
{
    const uint8_t i = static_cast<uint8_t>(button);

    if (!stableState_[i]) {
        return 0;
    }

    return millis() - pressStartMs_[i];
}

bool ButtonInput::pollEvent(ButtonEvent& outEvent)
{
    if (eventQueueCount_ == 0) {
        return false;
    }

    outEvent = eventQueue_[eventQueueHead_];
    eventQueueHead_ = (eventQueueHead_ + 1) % EVENT_QUEUE_SIZE;
    eventQueueCount_--;

    return true;
}

void ButtonInput::pushEvent(ButtonId button, ButtonEventType type, unsigned long timestampMs)
{
    if (eventQueueCount_ >= EVENT_QUEUE_SIZE) {
        // Queue full: drop the oldest event to make room for the newest.
        eventQueueHead_ = (eventQueueHead_ + 1) % EVENT_QUEUE_SIZE;
        eventQueueCount_--;
    }

    eventQueue_[eventQueueTail_] = ButtonEvent{button, type, timestampMs};
    eventQueueTail_ = (eventQueueTail_ + 1) % EVENT_QUEUE_SIZE;
    eventQueueCount_++;
}
