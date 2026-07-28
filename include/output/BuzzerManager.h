#pragma once

#include <cstdint>

// Drives the passive piezo buzzer (GPIO38, see HardwarePins.h) via the
// ESP32's LEDC peripheral. A passive buzzer has no oscillator of its
// own (unlike the REV1-era active module this replaces, which only
// needed a plain digitalWrite HIGH/LOW) -- every sound here is an
// actual driven frequency. Non-blocking: playTone()/the named presets
// below load a short note sequence (one note for most, a handful for
// littleTune()/buzz()'s tremolo); update() (call every App::update()
// tick) advances the sequence and silences the buzzer once the last
// note elapses, so nothing here ever stalls the rest of the loop.
class BuzzerManager {
public:
    void begin();
    void update();

    // Generic building block: any one-off sound can go through this
    // directly instead of adding a new named preset.
    void playTone(unsigned int frequencyHz, unsigned long durationMs);

    // Named presets, tuned for a passive piezo. Current call sites
    // (see CLAUDE.md "Buzzer"): click() = encoder rotation, beep() =
    // any button press, tone() = encoder short-press (menu
    // confirm/select), boop() = encoder long-press (menu back/cancel),
    // buzz() = a player's timeout/elimination, littleTune() = the
    // whole game ending.
    void click();
    void beep();
    void boop();
    void buzz();
    void tone();
    void littleTune();

private:
    struct Note {
        unsigned int frequencyHz;
        unsigned long durationMs;
    };

    void startSequence(const Note* notes, uint8_t count);
    void silence();

    static constexpr uint8_t LEDC_CHANNEL = 0;
    static constexpr uint8_t LEDC_RESOLUTION_BITS = 10;

    Note customNote_{0, 0}; // backing storage for playTone()'s single dynamic note

    const Note* sequence_ = nullptr;
    uint8_t sequenceLength_ = 0;
    uint8_t sequenceIndex_ = 0;
    unsigned long noteOffAtMs_ = 0;
    bool playing_ = false;
};
