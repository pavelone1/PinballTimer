#include "network/DirectorControl.h"

#include <cstdio>
#include <cstring>
#include "network/StatusReporter.h"

void DirectorControl::begin(GameModeManager& modeManager, GameModeContext& context, StatusReporter& statusReporter)
{
    modeManager_ = &modeManager;
    context_ = &context;
    statusReporter_ = &statusReporter;
    localControlsLocked_ = false;

    server_.on("/status", HTTP_GET, [this]() { handleStatusRoute(); });
    server_.on("/command", HTTP_POST, [this]() { handleCommandRoute(); });
    server_.begin();
}

void DirectorControl::update()
{
    server_.handleClient();
}

DirectorCommandResult DirectorControl::execute(const DirectorCommand& command)
{
    if (command.type == DirectorCommandType::RequestFullStatus) {
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::SelectMode) {
        return modeManager_->selectMode(command.intValue)
            ? DirectorCommandResult::Ok
            : DirectorCommandResult::InvalidRequest;
    }

    if (command.type == DirectorCommandType::LockLocalControls) {
        localControlsLocked_ = true;
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::UnlockLocalControls) {
        localControlsLocked_ = false;
        return DirectorCommandResult::Ok;
    }

    if (!modeManager_->hasActiveMode()) {
        return DirectorCommandResult::NoActiveMode;
    }

    if (!modeManager_->activeMode()->allowsRemoteCommand(static_cast<uint8_t>(command.type))) {
        return DirectorCommandResult::Rejected;
    }

    switch (command.type) {
        case DirectorCommandType::SetPlayerCount:
            return modeManager_->setPlayerCount(command.intValue)
                ? DirectorCommandResult::Ok
                : DirectorCommandResult::InvalidRequest;

        case DirectorCommandType::SetModeOption:
            return modeManager_->activeMode()->setModeOption(command.stringKey, command.longValue)
                ? DirectorCommandResult::Ok
                : DirectorCommandResult::InvalidRequest;

        case DirectorCommandType::StartGame:
            modeManager_->initializeActiveMode();
            modeManager_->notifyRemoteStart();
            modeManager_->notifyGameStart();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::StartFirstTimer:
            modeManager_->notifyFirstTimerStart();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::Pause:
            modeManager_->notifyPause();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::Resume:
            modeManager_->notifyResume();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::Reset:
            modeManager_->notifyReset();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::IdentifyTimer:
            context_->buttonLights.setTemporaryOverride(
                static_cast<ButtonId>(command.intValue),
                LightPattern::Blink,
                150,
                3000,
                255
            );
            return DirectorCommandResult::Ok;

        default:
            return DirectorCommandResult::InvalidRequest;
    }
}

bool DirectorControl::localControlsLocked() const
{
    return localControlsLocked_;
}

void DirectorControl::handleStatusRoute()
{
    char buffer[512];
    statusReporter_->buildStatusJson(buffer, sizeof(buffer));
    server_.send(200, "application/json", buffer);
}

void DirectorControl::handleCommandRoute()
{
    DirectorCommand command;
    command.type = parseCommandType(server_.arg("type"));
    command.intValue = static_cast<uint8_t>(server_.arg("intValue").toInt());
    server_.arg("stringKey").toCharArray(command.stringKey, sizeof(command.stringKey));
    command.longValue = server_.arg("longValue").toInt();

    const DirectorCommandResult result = execute(command);

    char responseBuffer[64];
    snprintf(responseBuffer, sizeof(responseBuffer), "{\"result\":\"%s\"}", resultToString(result));
    server_.send(result == DirectorCommandResult::Ok ? 200 : 400, "application/json", responseBuffer);
}

DirectorCommandType DirectorControl::parseCommandType(const String& name)
{
    static const struct { const char* text; DirectorCommandType type; } kMap[] = {
        {"SelectMode", DirectorCommandType::SelectMode},
        {"SetPlayerCount", DirectorCommandType::SetPlayerCount},
        {"SetModeOption", DirectorCommandType::SetModeOption},
        {"StartGame", DirectorCommandType::StartGame},
        {"StartFirstTimer", DirectorCommandType::StartFirstTimer},
        {"Pause", DirectorCommandType::Pause},
        {"Resume", DirectorCommandType::Resume},
        {"Reset", DirectorCommandType::Reset},
        {"LockLocalControls", DirectorCommandType::LockLocalControls},
        {"UnlockLocalControls", DirectorCommandType::UnlockLocalControls},
        {"IdentifyTimer", DirectorCommandType::IdentifyTimer},
        {"RequestFullStatus", DirectorCommandType::RequestFullStatus},
    };

    for (const auto& entry : kMap) {
        if (name == entry.text) {
            return entry.type;
        }
    }

    return DirectorCommandType::Unknown;
}

const char* DirectorControl::resultToString(DirectorCommandResult result)
{
    switch (result) {
        case DirectorCommandResult::Ok: return "ok";
        case DirectorCommandResult::Rejected: return "rejected";
        case DirectorCommandResult::InvalidRequest: return "invalid_request";
        case DirectorCommandResult::NoActiveMode: return "no_active_mode";
    }

    return "unknown";
}
