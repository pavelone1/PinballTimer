#include "output/BuzzerManager.h"

#include <Arduino.h>
#include "HardwarePins.h"

void BuzzerManager::begin()
{
    ledcSetup(LEDC_CHANNEL, 2000, LEDC_RESOLUTION_BITS); // initial freq is a placeholder -- every note overwrites it
    ledcAttachPin(HardwarePins::BUZZER_PIN, LEDC_CHANNEL);
    ledcWrite(LEDC_CHANNEL, 0); // silent at idle
}

void BuzzerManager::update()
{
    if (!playing_ || millis() < noteOffAtMs_) {
        return;
    }

    const uint8_t nextIndex = sequenceIndex_ + 1;
    if (nextIndex >= sequenceLength_) {
        silence();
        return;
    }

    sequenceIndex_ = nextIndex;
    const Note& note = sequence_[sequenceIndex_];
    ledcWriteTone(LEDC_CHANNEL, note.frequencyHz);
    noteOffAtMs_ = millis() + note.durationMs;
}

void BuzzerManager::silence()
{
    ledcWriteTone(LEDC_CHANNEL, 0);
    playing_ = false;
    sequence_ = nullptr;
    sequenceLength_ = 0;
    sequenceIndex_ = 0;
}

void BuzzerManager::startSequence(const Note* notes, uint8_t count)
{
    sequence_ = notes;
    sequenceLength_ = count;
    sequenceIndex_ = 0;
    playing_ = true;
    ledcWriteTone(LEDC_CHANNEL, notes[0].frequencyHz);
    noteOffAtMs_ = millis() + notes[0].durationMs;
}

void BuzzerManager::playTone(unsigned int frequencyHz, unsigned long durationMs)
{
    customNote_ = {frequencyHz, durationMs};
    startSequence(&customNote_, 1);
}

void BuzzerManager::click()
{
    static constexpr Note notes[] = {{4000, 10}};
    startSequence(notes, 1);
}

void BuzzerManager::beep()
{
    static constexpr Note notes[] = {{1800, 60}};
    startSequence(notes, 1);
}

void BuzzerManager::boop()
{
    static constexpr Note notes[] = {{450, 90}};
    startSequence(notes, 1);
}

void BuzzerManager::buzz()
{
    // A passive piezo driven at one steady frequency reads as a clean
    // tone, not the harsh rattle of the old active module -- alternate
    // between two close low frequencies to fake a rough "buzz" texture
    // instead. 10 notes x 300ms = 3000ms, same total length the old
    // TIMEOUT_BUZZ_MS active-buzz elimination alert used.
    static constexpr Note notes[] = {
        {160, 300}, {220, 300}, {160, 300}, {220, 300}, {160, 300},
        {220, 300}, {160, 300}, {220, 300}, {160, 300}, {220, 300},
    };
    startSequence(notes, 10);
}

void BuzzerManager::tone()
{
    static constexpr Note notes[] = {{1200, 80}};
    startSequence(notes, 1);
}

void BuzzerManager::littleTune()
{
    // Short ascending jingle (C5-E5-G5-C6) with brief silent gaps
    // between notes so they read as distinct notes, not one sliding
    // tone -- reserved for the whole game ending (see CLAUDE.md
    // "Buzzer"), not fired on every turn/round.
    static constexpr Note notes[] = {
        {523, 90}, {0, 15}, {659, 90}, {0, 15}, {784, 90}, {0, 15}, {1047, 140},
    };
    startSequence(notes, 7);
}
