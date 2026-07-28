#pragma once

#include <esp_http_server.h>
#include "game/GameModeManager.h"
#include "game/GameMode.h"
#include "network/RemoteCommand.h"
#include "power/PowerManager.h"

class StatusReporter; // forward-declared to avoid a circular include with StatusReporter.h

// Handles director-issued commands over a small HTTP REST API (ESP-IDF's
// esp_http_server, not Arduino's WebServer -- see the note on why
// below):
//   GET  /status   -> JSON status document (via StatusReporter)
//   POST /command  -> form-encoded fields: type, intValue, stringKey,
//                      longValue (see RemoteCommand.h for type names
//                      and DirectorControl.cpp's parseCommandType())
//
// Commands are validated by the active game mode (GameMode::
// allowsRemoteCommand()) before execution, except SelectMode (there
// may be no active mode yet), LockLocalControls/UnlockLocalControls,
// RequestFullStatus, and the battery-stub debug commands
// (SetStubBatteryVoltage/ClearBatteryVoltageOverride), which are
// always allowed.
//
// Any command via POST /command other than RequestFullStatus wakes
// PowerManager from standby ("wake from remote command" per the
// architecture doc). GET /status does NOT wake it -- a director
// dashboard polling status every few seconds would otherwise defeat
// standby entirely.
//
// This concrete HTTP API shape (paths, field names, port 80) is a
// judgment call made when building this class -- there was no
// existing spec or director-side client to match, so this IS the
// spec going forward unless changed.
//
// Uses esp_http_server instead of the Arduino WebServer library this
// originally shipped with: extensive on-hardware bisection (see
// CLAUDE.md's "Firmware status") traced a reliable TFT/SPI boot
// crash specifically to WebServer/WiFiServer's linked code footprint
// -- not to anything this class's own logic does, since the crash
// reproduced even before begin() ever ran. Switching HTTP server
// implementations was the most targeted thing left to try.
class DirectorControl {
public:
    void begin(GameModeManager& modeManager, GameModeContext& context, StatusReporter& statusReporter, PowerManager& power);
    void update();

    DirectorCommandResult execute(const DirectorCommand& command);
    bool localControlsLocked() const;

    // Lets other subsystems (currently WifiPortal) register additional
    // URI handlers on the same httpd instance instead of starting a
    // second server -- esp_http_server binds all interfaces regardless
    // of WiFi mode, so this stays reachable at the AP's gateway IP even
    // with no STA connection.
    httpd_handle_t server() const;

private:
    GameModeManager* modeManager_ = nullptr;
    GameModeContext* context_ = nullptr;
    StatusReporter* statusReporter_ = nullptr;
    PowerManager* power_ = nullptr;
    httpd_handle_t server_ = nullptr;
    bool localControlsLocked_ = false;

    esp_err_t handleStatusRoute(httpd_req_t* req);
    esp_err_t handleCommandRoute(httpd_req_t* req);

    // esp_http_server handlers must be plain function pointers (no
    // captures) -- these trampolines recover `this` from the per-URI
    // user_ctx set at registration and forward into the real member
    // function.
    static esp_err_t handleStatusRouteTrampoline(httpd_req_t* req);
    static esp_err_t handleCommandRouteTrampoline(httpd_req_t* req);

    static DirectorCommandType parseCommandType(const char* name);
    static const char* resultToString(DirectorCommandResult result);
};
