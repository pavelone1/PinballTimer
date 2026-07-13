#pragma once

#include <cstdint>
#include "SystemTypes.h"

// Polls and debounces the five physical button switches (4 player +
// Action) and produces press/release/short-press/long-press events.
// Knows nothing about what a button currently represents or what a
// press should do -- that's ButtonAssignmentManager's job later.
class ButtonInput {
public:
    void begin();
    void update();

    bool isPressed(ButtonId button) const;
    unsigned long heldDurationMs(ButtonId button) const;

    // Drains one pending event per call. Call in a loop until it
    // returns false to process everything update() generated.
    bool pollEvent(ButtonEvent& outEvent);

private:
    static constexpr uint8_t BUTTON_COUNT = static_cast<uint8_t>(ButtonId::Count);
    static constexpr unsigned long DEBOUNCE_MS = 30;
    static constexpr unsigned long LONG_PRESS_MS = 600;
    static constexpr uint8_t EVENT_QUEUE_SIZE = 16;

    bool rawState_[BUTTON_COUNT] = {};
    bool stableState_[BUTTON_COUNT] = {};
    unsigned long lastChangeMs_[BUTTON_COUNT] = {};
    unsigned long pressStartMs_[BUTTON_COUNT] = {};
    bool longPressFired_[BUTTON_COUNT] = {};

    ButtonEvent eventQueue_[EVENT_QUEUE_SIZE];
    uint8_t eventQueueHead_ = 0;
    uint8_t eventQueueTail_ = 0;
    uint8_t eventQueueCount_ = 0;

    void pushEvent(ButtonId button, ButtonEventType type, unsigned long timestampMs);
};
