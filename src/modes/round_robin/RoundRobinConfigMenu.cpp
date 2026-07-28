#include "modes/round_robin/RoundRobinConfigMenu.h"


void RoundRobinConfigMenu::begin(
    RoundRobinConfig& config, const MachineCatalog& catalog)
{
    config_ = &config;
    catalog_ = &catalog;
    screen_ = Screen::Root;
    selectedItem_ = Item::StartRoundRobin;
    catalogIndex_ = 0;
    pendingPlayerCount_ = config.playerCount();
    pendingGameTimeSeconds_ = config.gameTimeSeconds();
    pendingBallCount_ = config.ballCount();
    lastValidation_ = RoundRobinConfig::ValidationError::None;
}

RoundRobinConfigMenu::Outcome
RoundRobinConfigMenu::handleEncoderEvent(const EncoderEvent& event)
{
    if (!config_ || !catalog_) {
        return Outcome::None;
    }
    if (event.type == EncoderEventType::RotatedClockwise) {
        rotate(true);
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        rotate(false);
    } else if (event.type == EncoderEventType::SwShortPress) {
        return select();
    } else if (event.type == EncoderEventType::SwLongPress) {
        if (screen_ == Screen::Root) {
            return Outcome::Back;
        }
        screen_ = Screen::Root;
    }
    return Outcome::None;
}

RoundRobinConfigMenu::Screen RoundRobinConfigMenu::screen() const { return screen_; }
RoundRobinConfigMenu::Item RoundRobinConfigMenu::selectedItem() const { return selectedItem_; }
uint16_t RoundRobinConfigMenu::selectedCatalogIndex() const { return catalogIndex_; }
uint8_t RoundRobinConfigMenu::pendingPlayerCount() const { return pendingPlayerCount_; }
long RoundRobinConfigMenu::pendingGameTimeSeconds() const { return pendingGameTimeSeconds_; }
uint8_t RoundRobinConfigMenu::pendingBallCount() const { return pendingBallCount_; }
RoundRobinConfig::ValidationError RoundRobinConfigMenu::lastValidation() const { return lastValidation_; }

const char* RoundRobinConfigMenu::title() const
{
    switch (screen_) {
        case Screen::Root: return "Round Robin Config";
        case Screen::EditPlayerCount: return "Number of Players";
        case Screen::SelectCatalogMachine: return "Select Machine";
        case Screen::EditGameTime: return "Game Time";
        case Screen::EditBallCount: return "Ball Count";
        case Screen::ValidationWarning: return "Round Robin Not Ready";
    }
    return "Round Robin Config";
}

const char* RoundRobinConfigMenu::selectedItemLabel() const
{
    static const char* labels[] = {
        "Start Round Robin",
        "Number of Players",
        "Select Machine",
        "Game Time",
        "Ball Count",
        "Player Setup",
        "Back"
    };
    return labels[static_cast<uint8_t>(selectedItem_)];
}

void RoundRobinConfigMenu::rotate(bool clockwise)
{
    if (screen_ == Screen::Root) {
        const uint8_t count = static_cast<uint8_t>(Item::Count);
        const uint8_t current = static_cast<uint8_t>(selectedItem_);
        selectedItem_ = static_cast<Item>(
            clockwise ? (current + 1) % count : (current + count - 1) % count);
    } else if (screen_ == Screen::EditPlayerCount) {
        if (clockwise && pendingPlayerCount_ < RoundRobinConfig::MAX_PLAYERS) {
            ++pendingPlayerCount_;
        } else if (!clockwise && pendingPlayerCount_ > RoundRobinConfig::MIN_PLAYERS) {
            --pendingPlayerCount_;
        }
    } else if (screen_ == Screen::SelectCatalogMachine) {
        const uint16_t countWithNone = catalog_->count() + 1;
        catalogIndex_ = clockwise
            ? (catalogIndex_ + 1) % countWithNone
            : (catalogIndex_ + countWithNone - 1) % countWithNone;
    } else if (screen_ == Screen::EditGameTime) {
        const long delta = clockwise ? 1 : -1;
        const long candidate = pendingGameTimeSeconds_ + delta;
        if (candidate >= RoundRobinConfig::MIN_GAME_TIME_SECONDS &&
            candidate <= RoundRobinConfig::MAX_GAME_TIME_SECONDS) {
            pendingGameTimeSeconds_ = candidate;
        }
    } else if (screen_ == Screen::EditBallCount) {
        if (clockwise && pendingBallCount_ < RoundRobinConfig::MAX_BALL_COUNT) {
            ++pendingBallCount_;
        } else if (!clockwise && pendingBallCount_ > RoundRobinConfig::MIN_BALL_COUNT) {
            --pendingBallCount_;
        }
    }
}

RoundRobinConfigMenu::Outcome RoundRobinConfigMenu::select()
{
    if (screen_ == Screen::ValidationWarning) {
        screen_ = Screen::Root;
        return Outcome::None;
    }
    if (screen_ == Screen::EditPlayerCount) {
        config_->setPlayerCount(pendingPlayerCount_);
        screen_ = Screen::Root;
        return Outcome::None;
    }
    if (screen_ == Screen::SelectCatalogMachine) {
        if (catalogIndex_ == 0) {
            config_->clearMachineSelection();
        } else {
            config_->selectMachine(catalog_->at(catalogIndex_ - 1)->id, *catalog_);
        }
        screen_ = Screen::Root;
        return Outcome::None;
    }
    if (screen_ == Screen::EditGameTime) {
        config_->setGameTimeSeconds(pendingGameTimeSeconds_);
        screen_ = Screen::Root;
        return Outcome::None;
    }
    if (screen_ == Screen::EditBallCount) {
        config_->setBallCount(pendingBallCount_);
        screen_ = Screen::Root;
        return Outcome::None;
    }

    switch (selectedItem_) {
        case Item::StartRoundRobin:
            lastValidation_ = config_->validate(*catalog_);
            if (lastValidation_ != RoundRobinConfig::ValidationError::None) {
                screen_ = Screen::ValidationWarning;
                return Outcome::None;
            }
            return Outcome::StartRoundRobin;
        case Item::NumberOfPlayers:
            pendingPlayerCount_ = config_->playerCount();
            screen_ = Screen::EditPlayerCount;
            return Outcome::None;
        case Item::SelectMachine:
            catalogIndex_ = 0;
            if (config_->hasMachineSelection()) {
                for (uint16_t i = 0; i < catalog_->count(); ++i) {
                    if (catalog_->at(i)->id == config_->machineId()) {
                        catalogIndex_ = i + 1;
                        break;
                    }
                }
            }
            screen_ = Screen::SelectCatalogMachine;
            return Outcome::None;
        case Item::GameTime:
            pendingGameTimeSeconds_ = config_->gameTimeSeconds();
            screen_ = Screen::EditGameTime;
            return Outcome::None;
        case Item::BallCount:
            pendingBallCount_ = config_->ballCount();
            screen_ = Screen::EditBallCount;
            return Outcome::None;
        case Item::PlayerSetup:
            return Outcome::OpenPlayerSetup;
        case Item::Back:
            return Outcome::Back;
        case Item::Count:
            return Outcome::None;
    }
    return Outcome::None;
}
