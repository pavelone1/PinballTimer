#pragma once

#include <cstdint>
#include <DNSServer.h>
#include <esp_http_server.h>
#include "storage/SettingsStorage.h"
#include "output/TftDisplayManager.h"

// Browser-based WiFi (re)configuration: the ESP32 broadcasts its own
// WPA2-protected access point ("<deviceName> Setup") with a captive
// portal, so venue-network credentials can be typed on a phone's own
// keyboard instead of PinballTimer's rotary encoder (see
// WifiSetupMenu for that alternative -- both exist; this one is
// nicer when a phone is handy, WifiSetupMenu when it isn't).
//
// Reuses DirectorControl's existing httpd instance
// (DirectorControl::server()) rather than opening a second server on
// another port -- that server stays reachable at the AP's gateway IP
// even with no STA connection, since esp_http_server binds all
// interfaces regardless of WiFi mode. Only the DNSServer (port 53,
// for captive-portal auto-popup on phones) and the AP itself belong
// to this class. Uses esp_http_server rather than Arduino's WebServer
// for the same reason as DirectorControl -- see that class's header
// comment.
//
// Reachable on demand via DirectorMenu/BootMenu's "Setup WiFi (Web)"
// item. Testing a candidate network deliberately bypasses
// NetworkManager (raw WiFi.begin() in WIFI_AP_STA mode, so the portal
// AP survives the test) rather than committing to it via
// NetworkManager::begin() immediately -- a bad password would
// otherwise tear down the portal the phone is using to enter it. App
// is responsible for calling NetworkManager::begin() with the saved
// credentials once isOpen() goes false (see App::update()'s
// edge-detection, same pattern as WifiSetupMenu) -- and for not
// calling NetworkManager::update() while this is open, since that
// would race the same WiFi radio state from the other direction.
//
// Persistent hotspot / "adhoc" mode (SettingsStorage's
// WifiOperatingMode, see storage/SettingsStorage.h): independent of
// the interactive flow above, the AP itself can be told to stay up in
// the background indefinitely -- e.g. so a director can connect
// straight to the device's own hotspot and reach DirectorControl's
// /status and /command without ever joining the venue network.
// applyStartupMode() brings this up at boot from the saved setting;
// setPersistentHotspot()/revertToAdhoc() change it live. When
// persistent, close() (called after the interactive flow finishes,
// however it finishes) intentionally leaves the AP/DNS server running
// rather than tearing them down -- only isOpen() (interactive
// UI/input focus) goes back to false, which is what App actually
// needs to know to resume routing input to the game again.
class WifiPortal {
public:
    void begin(httpd_handle_t server, SettingsStorage& settings, TftDisplayManager& tft, const char* apPassword);

    // Applies whatever WifiOperatingMode is currently saved -- call
    // once at boot (after NetworkManager::begin()) to bring the AP up
    // in the background if the saved mode calls for it, without
    // taking over input/TFT focus the way open() does.
    void applyStartupMode();

    void open();
    void close();
    bool isOpen() const;

    // Forces AccessPointOnly: drops the STA connection, brings the AP
    // up (persistently) if it wasn't already, persists the choice,
    // and opens the interactive flow so the operator sees the
    // hotspot's join info rather than it silently appearing.
    void revertToAdhoc();

    // Toggles WifiOperatingMode between Both and StationOnly (i.e.
    // whether the hotspot stays up alongside a normal WiFi
    // connection), applied immediately, not just on next boot.
    void setPersistentHotspot(bool persistent);
    bool persistentHotspot() const;

    // Call every tick -- drives the DNS server (needed whenever the
    // AP is up at all, interactively or persistently) and the
    // connect-attempt timeout/grace periods (only while the
    // interactive flow is active).
    void update();

private:
    enum class State : uint8_t {
        Idle,
        Portal,      // AP up, waiting for the setup form to be submitted
        TestConnect, // AP+STA, form submitted, trying the candidate network
        Success,     // brief grace period so the phone's last status poll sees "connected" before the AP drops
        Failed       // brief display period before returning to Portal for a retry
    };

    static constexpr unsigned long TEST_CONNECT_TIMEOUT_MS = 15000;
    static constexpr unsigned long SUCCESS_GRACE_MS = 4000;
    static constexpr unsigned long FAILURE_DISPLAY_MS = 4000;
    static constexpr uint8_t SSID_MAX_LENGTH = 32;
    static constexpr uint8_t PASSWORD_MAX_LENGTH = 63;

    httpd_handle_t server_ = nullptr;
    SettingsStorage* settings_ = nullptr;
    TftDisplayManager* tft_ = nullptr;
    DNSServer dnsServer_;

    char apPassword_[PASSWORD_MAX_LENGTH + 1] = "";
    State state_ = State::Idle;
    unsigned long stateEnteredMs_ = 0;
    bool persistentApActive_ = false;

    char pendingSsid_[SSID_MAX_LENGTH + 1] = "";
    char pendingPassword_[PASSWORD_MAX_LENGTH + 1] = "";

    void registerRoutes();
    esp_err_t handleRoot(httpd_req_t* req);
    esp_err_t handleScan(httpd_req_t* req);
    esp_err_t handleConnect(httpd_req_t* req);
    esp_err_t handleStatus(httpd_req_t* req);
    esp_err_t handleNotFound(httpd_req_t* req);

    // esp_http_server handlers must be plain function pointers (no
    // captures); the 404 error handler in particular has no per-URI
    // user_ctx slot to recover `this` from, so these trampolines rely
    // on there only ever being one WifiPortal instance (App owns
    // exactly one) via instance_.
    static esp_err_t handleRootTrampoline(httpd_req_t* req);
    static esp_err_t handleScanTrampoline(httpd_req_t* req);
    static esp_err_t handleConnectTrampoline(httpd_req_t* req);
    static esp_err_t handleStatusTrampoline(httpd_req_t* req);
    static esp_err_t handleNotFoundTrampoline(httpd_req_t* req, httpd_err_code_t error);
    static WifiPortal* instance_;

    void startAp();
    void renderTftStatus();
};
