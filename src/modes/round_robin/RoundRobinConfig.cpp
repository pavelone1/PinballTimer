#include "modes/round_robin/RoundRobinConfig.h"

void RoundRobinConfig::reset()
{
    playerCount_ = MAX_PLAYERS;
    machineId_ = 0;
    gameTimeSeconds_ = DEFAULT_GAME_TIME_SECONDS;
    ballCount_ = DEFAULT_BALL_COUNT;
}

uint8_t RoundRobinConfig::playerCount() const { return playerCount_; }

bool RoundRobinConfig::setPlayerCount(uint8_t count)
{
    if (count < MIN_PLAYERS || count > MAX_PLAYERS) {
        return false;
    }
    playerCount_ = count;
    return true;
}

MachineId RoundRobinConfig::machineId() const { return machineId_; }
bool RoundRobinConfig::hasMachineSelection() const { return machineId_ != 0; }

bool RoundRobinConfig::selectMachine(
    MachineId id, const MachineCatalog& catalog)
{
    if (!catalog.find(id)) {
        return false;
    }
    machineId_ = id;
    return true;
}

void RoundRobinConfig::clearMachineSelection() { machineId_ = 0; }

long RoundRobinConfig::gameTimeSeconds() const { return gameTimeSeconds_; }

bool RoundRobinConfig::setGameTimeSeconds(long seconds)
{
    if (seconds < MIN_GAME_TIME_SECONDS || seconds > MAX_GAME_TIME_SECONDS) {
        return false;
    }
    gameTimeSeconds_ = seconds;
    return true;
}

uint8_t RoundRobinConfig::ballCount() const { return ballCount_; }

bool RoundRobinConfig::setBallCount(uint8_t balls)
{
    if (balls < MIN_BALL_COUNT || balls > MAX_BALL_COUNT) {
        return false;
    }
    ballCount_ = balls;
    return true;
}

RoundRobinConfig::ValidationError
RoundRobinConfig::validate(const MachineCatalog& catalog) const
{
    if (playerCount_ < MIN_PLAYERS || playerCount_ > MAX_PLAYERS) {
        return ValidationError::InvalidPlayerCount;
    }
    if (gameTimeSeconds_ < MIN_GAME_TIME_SECONDS ||
        gameTimeSeconds_ > MAX_GAME_TIME_SECONDS) {
        return ValidationError::InvalidGameTime;
    }
    if (ballCount_ < MIN_BALL_COUNT || ballCount_ > MAX_BALL_COUNT) {
        return ValidationError::InvalidBallCount;
    }
    if (machineId_ != 0 && !catalog.find(machineId_)) {
        return ValidationError::MissingCatalogRecord;
    }
    return ValidationError::None;
}

