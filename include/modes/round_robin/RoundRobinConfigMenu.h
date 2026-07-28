#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/MachineCatalog.h"
#include "modes/round_robin/RoundRobinConfig.h"
class TftDisplayManager;

class RoundRobinConfigMenu {
public:
    enum class Item : uint8_t {
        StartRoundRobin,
        NumberOfPlayers,
        SelectMachine,
        GameTime,
        BallCount,
        PlayerSetup,
        Back,
        Count
    };

    enum class Screen : uint8_t {
        Root,
        EditPlayerCount,
        SelectCatalogMachine,
        EditGameTime,
        EditBallCount,
        ValidationWarning
    };

    enum class Outcome : uint8_t {
        None,
        StartRoundRobin,
        OpenPlayerSetup,
        Back
    };

    void begin(RoundRobinConfig& config, const MachineCatalog& catalog);
    Outcome handleEncoderEvent(const EncoderEvent& event);
    void render(TftDisplayManager& tft) const;

    Screen screen() const;
    Item selectedItem() const;
    uint16_t selectedCatalogIndex() const; // 0=None, records begin at 1
    uint8_t pendingPlayerCount() const;
    long pendingGameTimeSeconds() const;
    uint8_t pendingBallCount() const;
    RoundRobinConfig::ValidationError lastValidation() const;

    const char* title() const;
    const char* selectedItemLabel() const;

private:
    RoundRobinConfig* config_ = nullptr;
    const MachineCatalog* catalog_ = nullptr;
    Screen screen_ = Screen::Root;
    Item selectedItem_ = Item::StartRoundRobin;
    uint16_t catalogIndex_ = 0;
    uint8_t pendingPlayerCount_ = RoundRobinConfig::MAX_PLAYERS;
    long pendingGameTimeSeconds_ = RoundRobinConfig::DEFAULT_GAME_TIME_SECONDS;
    uint8_t pendingBallCount_ = RoundRobinConfig::DEFAULT_BALL_COUNT;
    RoundRobinConfig::ValidationError lastValidation_ =
        RoundRobinConfig::ValidationError::None;

    void rotate(bool clockwise);
    Outcome select();
};
