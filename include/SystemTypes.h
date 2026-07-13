#pragma once

#include <cstdint>

// Shared types used across the firmware. Anything one subsystem needs
// to hand to another (input events, IDs for physical resources) lives
// here so it isn't redefined per-file.
//
// Deliberately does NOT yet include game-state or network-result
// types: the game subsystem (modes, zero-crossing behavior) and the
// network/director-control subsystem haven't been designed yet and
// are still open questions in CLAUDE.md. Add those enums when those
// subsystems are actually built, not before.

enum class PlayerId : uint8_t {
    Player1 = 0,
    Player2,
    Player3,
    Player4,
    Count
};

enum class ButtonId : uint8_t {
    Player1 = 0,
    Player2,
    Player3,
    Player4,
    Action,
    Count
};

enum class DisplayId : uint8_t {
    Display1 = 0,
    Display2,
    Display3,
    Display4,
    Count
};

using TimerId = uint8_t;

enum class ColorId : uint8_t {
    Black,
    White,
    Red,
    Green,
    Blue,
    Yellow,
    Cyan,
    Magenta,
    Orange,
    Purple,
    DarkBlue
};

enum class ButtonEventType : uint8_t {
    Pressed,
    Released,
    ShortPress,
    LongPress
};

struct ButtonEvent {
    ButtonId button;
    ButtonEventType type;
    unsigned long timestampMs;
};

enum class EncoderEventType : uint8_t {
    RotatedClockwise,
    RotatedCounterClockwise,
    SwPressed,
    SwReleased,
    SwShortPress,
    SwLongPress
};

struct EncoderEvent {
    EncoderEventType type;
    long position;
    unsigned long timestampMs;
};

enum class SystemState : uint8_t {
    Startup,
    Setup,
    Standby,
    GameRunning,
    Paused,
    Error
};
