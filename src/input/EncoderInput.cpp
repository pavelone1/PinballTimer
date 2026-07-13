#include "input/EncoderInput.h"

#include <Arduino.h>
#include "HardwarePins.h"

void EncoderInput::begin()
{
    pinMode(HardwarePins::ENCODER_SW, INPUT_PULLUP);

    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder_.attachHalfQuad(HardwarePins::ENCODER_CLK, HardwarePins::ENCODER_DT);
    encoder_.setCount(0);
    lastPosition_ = 0;
}

void EncoderInput::update()
{
    const unsigned long now = millis();

    const long currentPosition = encoder_.getCount();
    if (currentPosition != lastPosition_) {
        const EncoderEventType type = currentPosition > lastPosition_
            ? EncoderEventType::RotatedClockwise
            : EncoderEventType::RotatedCounterClockwise;

        while (lastPosition_ != currentPosition) {
            lastPosition_ += (currentPosition > lastPosition_) ? 1 : -1;
            pushEvent(type, lastPosition_, now);
        }
    }

    const bool raw = digitalRead(HardwarePins::ENCODER_SW) == LOW;

    if (raw != rawSwState_) {
        rawSwState_ = raw;
        lastSwChangeMs_ = now;
    }

    if (now - lastSwChangeMs_ >= DEBOUNCE_MS && stableSwState_ != rawSwState_) {
        stableSwState_ = rawSwState_;

        if (stableSwState_) {
            swPressStartMs_ = now;
            longPressFired_ = false;
            pushEvent(EncoderEventType::SwPressed, lastPosition_, now);
        } else {
            pushEvent(EncoderEventType::SwReleased, lastPosition_, now);

            if (!longPressFired_) {
                pushEvent(EncoderEventType::SwShortPress, lastPosition_, now);
            }
        }
    }

    if (stableSwState_ && !longPressFired_ &&
        now - swPressStartMs_ >= LONG_PRESS_MS) {
        longPressFired_ = true;
        pushEvent(EncoderEventType::SwLongPress, lastPosition_, now);
    }
}

long EncoderInput::position() const
{
    return lastPosition_;
}

bool EncoderInput::isSwPressed() const
{
    return stableSwState_;
}

bool EncoderInput::pollEvent(EncoderEvent& outEvent)
{
    if (eventQueueCount_ == 0) {
        return false;
    }

    outEvent = eventQueue_[eventQueueHead_];
    eventQueueHead_ = (eventQueueHead_ + 1) % EVENT_QUEUE_SIZE;
    eventQueueCount_--;

    return true;
}

void EncoderInput::pushEvent(EncoderEventType type, long position, unsigned long timestampMs)
{
    if (eventQueueCount_ >= EVENT_QUEUE_SIZE) {
        // Queue full: drop the oldest event to make room for the newest.
        eventQueueHead_ = (eventQueueHead_ + 1) % EVENT_QUEUE_SIZE;
        eventQueueCount_--;
    }

    eventQueue_[eventQueueTail_] = EncoderEvent{type, position, timestampMs};
    eventQueueTail_ = (eventQueueTail_ + 1) % EVENT_QUEUE_SIZE;
    eventQueueCount_++;
}
