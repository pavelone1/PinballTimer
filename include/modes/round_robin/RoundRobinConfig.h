#pragma once

#include <cstdint>
#include "game/MachineCatalog.h"

class RoundRobinConfig {
public:
    static constexpr uint8_t MIN_PLAYERS = 1;
    static constexpr uint8_t MAX_PLAYERS = 4;
    static constexpr long MIN_GAME_TIME_SECONDS = 1;
    static constexpr long MAX_GAME_TIME_SECONDS = 5999;
    static constexpr long DEFAULT_GAME_TIME_SECONDS = 180;
    static constexpr uint8_t MIN_BALL_COUNT = 1;
    static constexpr uint8_t MAX_BALL_COUNT = 5;
    static constexpr uint8_t DEFAULT_BALL_COUNT = 3;

    enum class ValidationError : uint8_t {
        None,
        InvalidPlayerCount,
        InvalidGameTime,
        InvalidBallCount,
        MissingCatalogRecord
    };

    void reset();

    uint8_t playerCount() const;
    bool setPlayerCount(uint8_t count);

    MachineId machineId() const;
    bool hasMachineSelection() const;
    bool selectMachine(MachineId id, const MachineCatalog& catalog);
    void clearMachineSelection();

    long gameTimeSeconds() const;
    bool setGameTimeSeconds(long seconds);

    uint8_t ballCount() const;
    bool setBallCount(uint8_t balls);

    ValidationError validate(const MachineCatalog& catalog) const;

private:
    uint8_t playerCount_ = MAX_PLAYERS;
    MachineId machineId_ = 0;
    long gameTimeSeconds_ = DEFAULT_GAME_TIME_SECONDS;
    uint8_t ballCount_ = DEFAULT_BALL_COUNT;
};

