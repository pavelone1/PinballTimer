#pragma once

#include <esp_http_server.h>

// Serves two director-facing HTML pages over DirectorControl's
// existing esp_http_server instance (same pattern as WifiPortal --
// see that class's header comment for why esp_http_server, and why
// reusing one server instance rather than starting a second):
//
//   GET /game-setup  -- game configuration form (mode, player count,
//                        seconds/turn, ball count, machine name,
//                        player names + button/color assignment).
//                        Client-side JS fetch()es GET /status to
//                        prefill and POSTs to /command (DirectorControl)
//                        to apply changes -- this class has no
//                        POST handlers of its own, every mutation it
//                        needs already fits DirectorCommand's vocabulary.
//   GET /game-live   -- live status view during a game. Visual design
//                        is intentionally minimal/placeholder here,
//                        pending a reference photo of the physical
//                        device to model it after -- this page exists
//                        to prove the /status data contract works
//                        (player names/colors/rounds/timers, machine
//                        name, gameOver), not as the final look.
//
// No interactive state machine like WifiPortal needs (no open()/
// close(), no captive-portal DNS) -- both routes are just static pages
// bound at begin() and always reachable once WiFi is up.
class DirectorDashboard {
public:
    void begin(httpd_handle_t server);

    // No-op, same reason as DirectorControl::update()/WifiPortal's
    // routes don't need per-tick work here -- esp_http_server runs its
    // own FreeRTOS task. Kept for symmetry with the other network
    // classes App calls update() on uniformly.
    void update();

private:
    esp_err_t handleSetupPage(httpd_req_t* req);
    esp_err_t handleLivePage(httpd_req_t* req);

    // esp_http_server handlers must be plain function pointers (no
    // captures) -- these trampolines recover `this` from the per-URI
    // user_ctx set at registration, same pattern as DirectorControl.
    static esp_err_t handleSetupPageTrampoline(httpd_req_t* req);
    static esp_err_t handleLivePageTrampoline(httpd_req_t* req);
};
