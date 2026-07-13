#include "output/NumericDisplayManager.h"

#include <Arduino.h>
#include "HardwarePins.h"

NumericDisplayManager::NumericDisplayManager()
    : hardware_{
          TM1637Display(HardwarePins::DISPLAY_1_CLK, HardwarePins::DISPLAY_1_DIO),
          TM1637Display(HardwarePins::DISPLAY_2_CLK, HardwarePins::DISPLAY_2_DIO),
          TM1637Display(HardwarePins::DISPLAY_3_CLK, HardwarePins::DISPLAY_3_DIO),
          TM1637Display(HardwarePins::DISPLAY_4_CLK, HardwarePins::DISPLAY_4_DIO)
      }
{
}

void NumericDisplayManager::begin()
{
    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        hardware_[i].setBrightness(state_[i].brightness, true);
        hardware_[i].clear();
        state_[i].dirty = true;
    }
}

void NumericDisplayManager::update()
{
    const unsigned long now = millis();

    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        DisplayState& s = state_[i];

        const bool isNegativeTime = s.format == DisplayFormat::TimeMinutesSeconds && s.value < 0;
        const bool effectiveFlashing = s.flashing || isNegativeTime;
        const unsigned long effectiveInterval = isNegativeTime
            ? NEGATIVE_TIME_FLASH_INTERVAL_MS
            : s.flashIntervalMs;

        if (effectiveFlashing && now - s.lastFlashToggleMs >= effectiveInterval) {
            s.lastFlashToggleMs = now;
            s.flashOnPhase = !s.flashOnPhase;
            s.dirty = true;
        }

        if (s.dirty) {
            render(i);
        }
    }
}

void NumericDisplayManager::setValue(DisplayId display, int value)
{
    DisplayState& s = state_[static_cast<uint8_t>(display)];
    if (s.value != value) {
        s.value = value;
        s.dirty = true;
    }
}

void NumericDisplayManager::setFormat(DisplayId display, DisplayFormat format)
{
    DisplayState& s = state_[static_cast<uint8_t>(display)];
    if (s.format != format) {
        s.format = format;
        s.dirty = true;
    }
}

void NumericDisplayManager::setColon(DisplayId display, bool on)
{
    DisplayState& s = state_[static_cast<uint8_t>(display)];
    if (s.colonOn != on) {
        s.colonOn = on;
        s.dirty = true;
    }
}

void NumericDisplayManager::setBrightness(DisplayId display, uint8_t brightness)
{
    DisplayState& s = state_[static_cast<uint8_t>(display)];
    const uint8_t clamped = brightness > 7 ? 7 : brightness;
    if (s.brightness != clamped) {
        s.brightness = clamped;
        s.dirty = true;
    }
}

void NumericDisplayManager::setVisible(DisplayId display, bool visible)
{
    DisplayState& s = state_[static_cast<uint8_t>(display)];
    if (s.visible != visible) {
        s.visible = visible;
        s.dirty = true;
    }
}

void NumericDisplayManager::setFlashing(DisplayId display, bool flashing, unsigned long intervalMs)
{
    DisplayState& s = state_[static_cast<uint8_t>(display)];
    s.flashing = flashing;
    s.flashIntervalMs = intervalMs;
    s.flashOnPhase = true;
    s.lastFlashToggleMs = millis();
    s.dirty = true;
}

void NumericDisplayManager::setAssignedSource(DisplayId display, uint8_t sourceId)
{
    state_[static_cast<uint8_t>(display)].assignedSource = sourceId;
}

int NumericDisplayManager::value(DisplayId display) const
{
    return state_[static_cast<uint8_t>(display)].value;
}

uint8_t NumericDisplayManager::assignedSource(DisplayId display) const
{
    return state_[static_cast<uint8_t>(display)].assignedSource;
}

void NumericDisplayManager::render(uint8_t index)
{
    DisplayState& s = state_[index];

    hardware_[index].setBrightness(s.brightness, true);

    const bool isNegativeTime = s.format == DisplayFormat::TimeMinutesSeconds && s.value < 0;
    const bool effectiveFlashing = s.flashing || isNegativeTime;

    const bool shouldShowNumber = s.visible && (!effectiveFlashing || s.flashOnPhase);

    if (!shouldShowNumber) {
        hardware_[index].clear();
        s.dirty = false;
        return;
    }

    int displayValue = s.value;

    if (s.format == DisplayFormat::TimeMinutesSeconds) {
        // Negative time counts back up from zero: -5s displays as 00:05.
        const int magnitude = isNegativeTime ? -s.value : s.value;
        const int minutes = magnitude / 60;
        const int seconds = magnitude % 60;
        displayValue = minutes * 100 + seconds;
    }

    const uint8_t dots = s.colonOn ? COLON_DOTS : 0;

    hardware_[index].showNumberDecEx(displayValue, dots, true, 4, 0);
    s.dirty = false;
}
