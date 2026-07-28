#pragma once

#include <cstdint>
#include <ESP32Encoder.h>
#include "SystemTypes.h"

// Polls the KY-040 rotary encoder via the hardware pulse counter
// (through the ESP32Encoder library) and debounces its pushbutton.
// Produces rotation and SW press/release/short/long events.
class EncoderInput {
public:
    void begin();
    void update();

    // In physical detents/clicks, not raw PCNT counts -- see
    // COUNTS_PER_DETENT.
    long position() const;
    bool isSwPressed() const;

    // Drains one pending event per call. Call in a loop until it
    // returns false to process everything update() generated.
    bool pollEvent(EncoderEvent& outEvent);

private:
    static constexpr unsigned long DEBOUNCE_MS = 30;
    static constexpr unsigned long LONG_PRESS_MS = 600;
    static constexpr uint8_t EVENT_QUEUE_SIZE = 16;

    // attachHalfQuad() (see begin()) counts both edges of one PCNT
    // channel, which is 2 raw counts per physical detent/click on a
    // standard KY-040 -- without dividing this back out, every menu
    // rotation event fires twice per click, making on-device menus
    // feel like they skip an item per click. Confirmed against the
    // ESP32Encoder library's own PCNT config (single channel, both
    // edges counted) rather than assumed.
    static constexpr int8_t COUNTS_PER_DETENT = 2;

    ESP32Encoder encoder_;
    long lastPosition_ = 0; // detent-scaled; what position()/events expose

    bool rawSwState_ = false;
    bool stableSwState_ = false;
    unsigned long lastSwChangeMs_ = 0;
    unsigned long swPressStartMs_ = 0;
    bool longPressFired_ = false;

    EncoderEvent eventQueue_[EVENT_QUEUE_SIZE];
    uint8_t eventQueueHead_ = 0;
    uint8_t eventQueueTail_ = 0;
    uint8_t eventQueueCount_ = 0;

    void pushEvent(EncoderEventType type, long position, unsigned long timestampMs);
};
