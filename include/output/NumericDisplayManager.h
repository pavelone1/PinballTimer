#pragma once

#include <cstdint>
#include <TM1637Display.h>
#include "SystemTypes.h"

// Format a display's numeric value is interpreted as.
//
// TimeMinutesSeconds accepts negative totals: a negative value means
// the timer has gone past zero. It is shown as the elapsed magnitude
// counting back up from 00:00 (e.g. -5s shows as 00:05), with rapid
// flashing forced automatically -- the caller does not need to manage
// this, it is trapped internally purely by the value going negative.
enum class DisplayFormat : uint8_t {
    RawNumber,
    TimeMinutesSeconds
};

// Controls the four TM1637 displays as general-purpose numeric
// resources. This is a display module only -- it renders whatever
// value it's given, it does not run countdown/timer logic itself
// (that's TimerManager's job later). Stores per-display value,
// format, colon state, brightness, visibility, and flash state, and
// renders to hardware only when something changed. Does not decide
// what a display shows -- that's DisplayAssignmentManager's job
// later. assignedSource is stored here purely as an opaque tag for
// that manager to read/write.
class NumericDisplayManager {
public:
    NumericDisplayManager();

    void begin();
    void update();

    void setValue(DisplayId display, int value);
    void setFormat(DisplayId display, DisplayFormat format);
    void setColon(DisplayId display, bool on);
    void setBrightness(DisplayId display, uint8_t brightness);
    void setVisible(DisplayId display, bool visible);
    void setFlashing(DisplayId display, bool flashing, unsigned long intervalMs = 500);
    void setAssignedSource(DisplayId display, uint8_t sourceId);

    int value(DisplayId display) const;
    uint8_t assignedSource(DisplayId display) const;

private:
    static constexpr uint8_t DISPLAY_COUNT = static_cast<uint8_t>(DisplayId::Count);
    static constexpr uint8_t COLON_DOTS = 0b01000000;
    static constexpr unsigned long NEGATIVE_TIME_FLASH_INTERVAL_MS = 150;

    struct DisplayState {
        int value = 0;
        DisplayFormat format = DisplayFormat::RawNumber;
        bool colonOn = false;
        uint8_t brightness = 4;
        bool visible = true;
        bool flashing = false;
        unsigned long flashIntervalMs = 500;
        bool flashOnPhase = true;
        unsigned long lastFlashToggleMs = 0;
        uint8_t assignedSource = 0;
        bool dirty = true;
    };

    TM1637Display hardware_[DISPLAY_COUNT];
    DisplayState state_[DISPLAY_COUNT];

    void render(uint8_t index);
};
