#pragma once

#include <WebServer.h>
#include "game/GameModeManager.h"
#include "game/GameMode.h"
#include "network/RemoteCommand.h"

class StatusReporter; // forward-declared to avoid a circular include with StatusReporter.h

// Handles director-issued commands over a small HTTP REST API
// (ESP32's built-in WebServer, no extra library):
//   GET  /status   -> JSON status document (via StatusReporter)
//   POST /command  -> form-encoded fields: type, intValue, stringKey,
//                      longValue (see RemoteCommand.h for type names
//                      and DirectorControl.cpp's parseCommandType())
//
// Commands are validated by the active game mode (GameMode::
// allowsRemoteCommand()) before execution, except SelectMode (there
// may be no active mode yet), LockLocalControls/UnlockLocalControls,
// and RequestFullStatus, which are always allowed.
//
// This concrete HTTP API shape (paths, field names, port 80) is a
// judgment call made when building this class -- there was no
// existing spec or director-side client to match, so this IS the
// spec going forward unless changed.
class DirectorControl {
public:
    void begin(GameModeManager& modeManager, GameModeContext& context, StatusReporter& statusReporter);
    void update();

    DirectorCommandResult execute(const DirectorCommand& command);
    bool localControlsLocked() const;

private:
    GameModeManager* modeManager_ = nullptr;
    GameModeContext* context_ = nullptr;
    StatusReporter* statusReporter_ = nullptr;
    WebServer server_;
    bool localControlsLocked_ = false;

    void handleStatusRoute();
    void handleCommandRoute();

    static DirectorCommandType parseCommandType(const String& name);
    static const char* resultToString(DirectorCommandResult result);
};
