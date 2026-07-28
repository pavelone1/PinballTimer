#include "game/GameModeManager.h"

void GameModeManager::begin(GameModeContext& context)
{
    context_ = &context;
    modeCount_ = 0;
    activeMode_ = nullptr;
    playerCount_ = 0;
    initialized_ = false;
    gameStarted_ = false;
    firstTimerStarted_ = false;
    paused_ = false;

    for (uint8_t i = 0; i < MAX_MODES; ++i) {
        modes_[i] = nullptr;
    }
}

bool GameModeManager::registerMode(GameMode* mode)
{
    if (mode == nullptr || modeCount_ >= MAX_MODES) {
        return false;
    }

    modes_[modeCount_] = mode;
    modeCount_++;
    return true;
}

bool GameModeManager::selectMode(uint8_t modeId)
{
    for (uint8_t i = 0; i < modeCount_; ++i) {
        if (modes_[i]->id() == modeId) {
            activeMode_ = modes_[i];
            playerCount_ = activeMode_->defaultPlayerCount();
            initialized_ = false;
            gameStarted_ = false;
            firstTimerStarted_ = false;
            paused_ = false;
            return true;
        }
    }

    return false;
}

void GameModeManager::initializeActiveMode()
{
    if (activeMode_ == nullptr || initialized_ || context_ == nullptr) {
        return;
    }

    activeMode_->setupAssignments(*context_, playerCount_);
    initialized_ = true;
}

bool GameModeManager::restoreActiveMode()
{
    if (activeMode_ == nullptr || initialized_ || context_ == nullptr) {
        return false;
    }

    if (!activeMode_->restoreState(*context_)) {
        return false;
    }

    initialized_ = true;
    gameStarted_ = true;
    firstTimerStarted_ = true;
    paused_ = false;
    return true;
}

GameMode* GameModeManager::activeMode() const
{
    return activeMode_;
}

bool GameModeManager::hasActiveMode() const
{
    return activeMode_ != nullptr;
}

uint8_t GameModeManager::modeCount() const
{
    return modeCount_;
}

GameMode* GameModeManager::modeAt(uint8_t index) const
{
    if (index >= modeCount_) {
        return nullptr;
    }

    return modes_[index];
}

bool GameModeManager::setPlayerCount(uint8_t count)
{
    if (activeMode_ == nullptr) {
        return false;
    }

    if (count < activeMode_->minPlayers() || count > activeMode_->maxPlayers()) {
        return false;
    }

    playerCount_ = count;
    return true;
}

uint8_t GameModeManager::playerCount() const
{
    return playerCount_;
}

bool GameModeManager::setPlayerOption(PlayerId player, const char* key, long value)
{
    if (activeMode_ == nullptr || context_ == nullptr) {
        return false;
    }
    return activeMode_->setPlayerOption(*context_, player, key, value);
}

long GameModeManager::playerOption(PlayerId player, const char* key) const
{
    if (activeMode_ == nullptr || context_ == nullptr) {
        return 0;
    }
    return activeMode_->playerOption(*context_, player, key);
}

bool GameModeManager::setLiveModeOption(const char* key, long value)
{
    if (activeMode_ == nullptr || context_ == nullptr) {
        return false;
    }
    return activeMode_->setLiveModeOption(*context_, key, value);
}

void GameModeManager::notifyLocalStart()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onLocalStart(*context_);
    }
}

void GameModeManager::notifyRemoteStart()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onRemoteStart(*context_);
    }
}

void GameModeManager::notifyGameStart()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onGameStart(*context_);
        gameStarted_ = true;
    }
}

void GameModeManager::notifyFirstTimerStart()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onFirstTimerStart(*context_);
        firstTimerStarted_ = true;
    }
}

void GameModeManager::notifyPause()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onPause(*context_);
        paused_ = true;
    }
}

void GameModeManager::notifyResume()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onResume(*context_);
        paused_ = false;
    }
}

void GameModeManager::notifyStop()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onStop(*context_);
    }

    gameStarted_ = false;
    firstTimerStarted_ = false;
    paused_ = false;
}

void GameModeManager::notifyReset()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onReset(*context_);
    }

    gameStarted_ = false;
    firstTimerStarted_ = false;
    paused_ = false;
}

void GameModeManager::returnToModeSelection()
{
    if (gameStarted_) {
        notifyStop();
    }

    activeMode_ = nullptr;
    playerCount_ = 0;
    initialized_ = false;
}

void GameModeManager::update()
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->update(*context_);
    }
}

void GameModeManager::handleButtonEvent(const ButtonEvent& event)
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        const bool justStarted = activeMode_->onButtonEvent(*context_, event);
        if (justStarted) {
            notifyLocalStart();
            notifyGameStart();
        }
    }
}

void GameModeManager::handleEncoderEvent(const EncoderEvent& event)
{
    if (activeMode_ != nullptr && context_ != nullptr) {
        activeMode_->onEncoderEvent(*context_, event);
    }
}

bool GameModeManager::isGameStarted() const
{
    return gameStarted_;
}

bool GameModeManager::isFirstTimerStarted() const
{
    return firstTimerStarted_;
}

bool GameModeManager::isPaused() const
{
    return paused_;
}
