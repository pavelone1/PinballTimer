#pragma once

#include <cstdint>
#include "SystemTypes.h"

enum class CountDirection : uint8_t {
    CountDown,
    CountUp
};

// Stores and updates all logical timers. Timers are not inherently
// tied to players or displays -- associatedPlayer/displayAssignment
// are stored purely as opaque tags for PlayerManager/
// DisplayAssignmentManager to read later; this module never acts on
// them itself, and never touches hardware directly (a caller reads
// currentValueSeconds() and hands it to NumericDisplayManager).
//
// Rules enforced here:
//  - No unintended rollover: a timer only wraps back to its initial
//    value if loopEnabled was explicitly set at creation.
//  - allowBelowZero lets a countdown go negative (paired with
//    NumericDisplayManager's automatic negative-time flash) instead
//    of clamping at zero.
//  - Every zero crossing produces a polled event; a caller decides
//    what visual/audio response that triggers.
class TimerManager {
public:
    static constexpr TimerId INVALID_TIMER = 0xFF;

    void begin();
    void update();

    TimerId createTimer(
        CountDirection direction,
        long initialValueSeconds,
        bool allowBelowZero,
        bool stopAtZero,
        bool loopEnabled = false
    );
    void destroyTimer(TimerId id);

    void start(TimerId id);
    void stop(TimerId id);
    void reset(TimerId id, long initialValueSeconds);

    bool isRunning(TimerId id) const;
    long currentValueSeconds(TimerId id) const;

    void setWarningThreshold(TimerId id, long thresholdSeconds);
    void clearWarningThreshold(TimerId id);

    void setAssociatedPlayer(TimerId id, PlayerId player);
    bool associatedPlayer(TimerId id, PlayerId& outPlayer) const;

    void setDisplayAssignment(TimerId id, DisplayId display);
    bool displayAssignment(TimerId id, DisplayId& outDisplay) const;

    // Drains one pending event per call. Call in a loop until it
    // returns false to process everything update() generated.
    bool pollZeroCrossingEvent(TimerId& outId);
    bool pollWarningEvent(TimerId& outId);

private:
    static constexpr uint8_t MAX_TIMERS = 8;
    static constexpr uint8_t EVENT_QUEUE_SIZE = 16;

    struct Timer {
        bool inUse = false;
        CountDirection direction = CountDirection::CountDown;
        long valueMs = 0;
        long initialValueMs = 0;
        bool running = false;
        bool allowBelowZero = false;
        bool stopAtZero = true;
        bool loopEnabled = false;
        bool hasWarningThreshold = false;
        long warningThresholdMs = 0;
        bool warningFired = false;
        bool hasAssociatedPlayer = false;
        PlayerId associatedPlayer = PlayerId::Player1;
        bool hasDisplayAssignment = false;
        DisplayId displayAssignment = DisplayId::Display1;
        unsigned long lastTickMillis = 0;
    };

    Timer timers_[MAX_TIMERS];

    TimerId zeroCrossingQueue_[EVENT_QUEUE_SIZE];
    uint8_t zcQueueHead_ = 0;
    uint8_t zcQueueTail_ = 0;
    uint8_t zcQueueCount_ = 0;

    TimerId warningQueue_[EVENT_QUEUE_SIZE];
    uint8_t warnQueueHead_ = 0;
    uint8_t warnQueueTail_ = 0;
    uint8_t warnQueueCount_ = 0;

    void pushZeroCrossing(TimerId id);
    void pushWarning(TimerId id);
};
