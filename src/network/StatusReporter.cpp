#include "network/StatusReporter.h"

#include <cstdio>
#include <cstdarg>
#include "network/DirectorControl.h"

void StatusReporter::begin(
    GameModeManager& modeManager,
    PlayerManager& players,
    DisplayAssignmentManager& displayAssignments,
    TimerManager& timers,
    NetworkManager& network,
    SettingsStorage& settings,
    DirectorControl& directorControl,
    PowerManager& power,
    GameStorage& gameStorage
)
{
    modeManager_ = &modeManager;
    players_ = &players;
    displayAssignments_ = &displayAssignments;
    timers_ = &timers;
    network_ = &network;
    settings_ = &settings;
    directorControl_ = &directorControl;
    power_ = &power;
    gameStorage_ = &gameStorage;
}

namespace {

// Appends a snprintf-formatted chunk at *offset within buffer,
// clamped to bufferSize. Silently stops writing (but keeps *offset
// pinned at bufferSize) if the buffer is full.
void appendf(char* buffer, size_t bufferSize, size_t* offset, const char* fmt, ...)
{
    if (*offset >= bufferSize) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    const int written = vsnprintf(buffer + *offset, bufferSize - *offset, fmt, args);
    va_end(args);

    if (written > 0) {
        const size_t writtenSize = static_cast<size_t>(written);
        *offset += writtenSize < (bufferSize - *offset) ? writtenSize : (bufferSize - *offset);
    }
}

} // namespace

size_t StatusReporter::buildStatusJson(char* buffer, size_t bufferSize) const
{
    if (buffer == nullptr || bufferSize == 0) {
        return 0;
    }

    size_t offset = 0;

    GameMode* mode = modeManager_->activeMode();

    appendf(buffer, bufferSize, &offset, "{\"device\":\"%s\"", settings_->deviceName());
    appendf(buffer, bufferSize, &offset, ",\"modeId\":%d", mode != nullptr ? static_cast<int>(mode->id()) : -1);
    appendf(buffer, bufferSize, &offset, ",\"modeName\":%s%s%s",
        mode != nullptr ? "\"" : "",
        mode != nullptr ? mode->name() : "null",
        mode != nullptr ? "\"" : "");
    appendf(buffer, bufferSize, &offset, ",\"playerCount\":%u", modeManager_->playerCount());
    appendf(buffer, bufferSize, &offset, ",\"gameStarted\":%s", modeManager_->isGameStarted() ? "true" : "false");
    appendf(buffer, bufferSize, &offset, ",\"firstTimerStarted\":%s", modeManager_->isFirstTimerStarted() ? "true" : "false");
    appendf(buffer, bufferSize, &offset, ",\"paused\":%s", modeManager_->isPaused() ? "true" : "false");
    appendf(buffer, bufferSize, &offset, ",\"localControlsLocked\":%s", directorControl_->localControlsLocked() ? "true" : "false");
    appendf(buffer, bufferSize, &offset, ",\"networkState\":%d", static_cast<int>(network_->connectionState()));
    appendf(buffer, bufferSize, &offset, ",\"powerState\":%d", static_cast<int>(power_->state()));
    appendf(buffer, bufferSize, &offset, ",\"machineName\":\"%s\"", gameStorage_->machineName());
    appendf(buffer, bufferSize, &offset, ",\"secondsPerTurn\":%ld", mode != nullptr ? mode->modeOption("secondsPerTurn") : 0);
    appendf(buffer, bufferSize, &offset, ",\"ballCount\":%ld", mode != nullptr ? mode->modeOption("ballCount") : 0);
    appendf(buffer, bufferSize, &offset, ",\"gameOver\":%s", (mode != nullptr && mode->isRoundOver()) ? "true" : "false");

    appendf(buffer, bufferSize, &offset, ",\"availableModes\":[");
    for (uint8_t i = 0; i < modeManager_->modeCount(); ++i) {
        GameMode* m = modeManager_->modeAt(i);
        appendf(buffer, bufferSize, &offset, "%s{\"id\":%d,\"name\":\"%s\"}",
            i == 0 ? "" : ",", static_cast<int>(m->id()), m->name());
    }
    appendf(buffer, bufferSize, &offset, "]");

    if (power_->batteryAvailable()) {
        appendf(buffer, bufferSize, &offset,
            ",\"batteryAvailable\":true,\"batteryVoltage\":%.2f,\"batteryPercent\":%u",
            static_cast<double>(power_->batteryVoltageV()), power_->batteryPercent());
    } else {
        appendf(buffer, bufferSize, &offset, ",\"batteryAvailable\":false");
    }

    appendf(buffer, bufferSize, &offset, ",\"displays\":[");

    for (uint8_t i = 0; i < 4; ++i) {
        const DisplayId display = static_cast<DisplayId>(i);
        const DisplayAssignment& assignment = displayAssignments_->assignment(display);

        appendf(buffer, bufferSize, &offset, "%s{\"index\":%u,\"assignmentType\":%d",
            i == 0 ? "" : ",", i, static_cast<int>(assignment.type));

        if (assignment.type == DisplayAssignmentType::SharedTimer) {
            appendf(buffer, bufferSize, &offset, ",\"timerSeconds\":%ld,\"timerRunning\":%s",
                timers_->currentValueSeconds(assignment.timerId),
                timers_->isRunning(assignment.timerId) ? "true" : "false");

            // Mode1RoundRobin assigns displays as SharedTimer (bound to
            // a TimerId), not SinglePlayer/PlayerNumber -- resolve which
            // player owns this timer via TimerManager's own
            // association (set by setAssociatedPlayer() at setup) so
            // the dashboard still gets per-player info for these.
            PlayerId owner;
            if (timers_->associatedPlayer(assignment.timerId, owner)) {
                appendf(buffer, bufferSize, &offset,
                    ",\"playerId\":%u,\"playerNumber\":%u,\"playerStatus\":%d,\"color\":%d,\"roundsRemaining\":%u,\"name\":\"%s\"",
                    static_cast<uint8_t>(owner),
                    players_->displayedNumber(owner),
                    static_cast<int>(players_->status(owner)),
                    static_cast<int>(players_->assignedColor(owner)),
                    players_->roundsRemaining(owner),
                    players_->name(owner));
            }
        }

        if (assignment.type == DisplayAssignmentType::SinglePlayer ||
            assignment.type == DisplayAssignmentType::PlayerNumber) {
            appendf(buffer, bufferSize, &offset,
                ",\"playerId\":%u,\"playerNumber\":%u,\"playerStatus\":%d,\"color\":%d,\"roundsRemaining\":%u,\"name\":\"%s\"",
                static_cast<uint8_t>(assignment.player),
                players_->displayedNumber(assignment.player),
                static_cast<int>(players_->status(assignment.player)),
                static_cast<int>(players_->assignedColor(assignment.player)),
                players_->roundsRemaining(assignment.player),
                players_->name(assignment.player));
        }

        appendf(buffer, bufferSize, &offset, "}");
    }

    appendf(buffer, bufferSize, &offset, "]}");

    return offset;
}
