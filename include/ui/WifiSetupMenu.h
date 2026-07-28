#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "network/NetworkManager.h"
#include "storage/SettingsStorage.h"
#include "output/TftDisplayManager.h"
#include "ui/TextEntry.h"

// On-device WiFi (re)configuration, reached via DirectorMenu's "Setup
// WiFi" item (MenuHandoff::OpenWifiSetup). There's no
// keyboard on this hardware -- one rotary encoder is the only input
// while it's open -- so the flow is built around that:
//   1. Scan for nearby networks (WiFi.scanNetworks), pick one by
//      rotating/clicking instead of typing an SSID character by
//      character. A "[Manual Entry]" row handles hidden networks;
//      a "[Rescan]" row retries the scan.
//   2. If the chosen network isn't open, enter its password via
//      TextEntry (see ui/TextEntry.h), long-press cancels the whole
//      flow from anywhere.
//   3. Saves into SettingsStorage/NVS and re-runs
//      NetworkManager::begin() with the new credentials, then waits
//      (with a timeout) to report success/failure on the TFT.
//
// Like DirectorMenu, this owns the encoder/TFT while open -- App
// routes input here instead of to the active GameMode. Unlike
// DirectorMenu, it can also close itself autonomously (connect
// success, or after a failed attempt returns to network selection) --
// App must poll isOpen() every tick, not just from input events, to
// notice that and resume the game.
class WifiSetupMenu {
public:
    void open(NetworkManager& network, SettingsStorage& settings, TftDisplayManager& tft);
    void close();
    bool isOpen() const;

    // Call every tick while open -- drives the async network scan and
    // the connect-attempt timeout, neither of which is input-driven.
    void update();

    void handleEncoderEvent(const EncoderEvent& event);

private:
    enum class State : uint8_t {
        Scanning,
        SelectNetwork,
        EnterSsidManual,
        EnterPassword,
        Connecting,
        Result
    };

    static constexpr uint8_t MAX_NETWORKS = 16;
    static constexpr uint8_t MAX_VISIBLE_ITEMS = 5; // must not exceed TftDisplayManager::MAX_LINES
    static constexpr uint8_t SSID_MAX_LENGTH = 32;
    static constexpr unsigned long CONNECT_TIMEOUT_MS = 15000;
    static constexpr unsigned long RESULT_DISPLAY_MS = 2500;

    bool open_ = false;
    State state_ = State::Scanning;
    NetworkManager* network_ = nullptr;
    SettingsStorage* settings_ = nullptr;
    TftDisplayManager* tft_ = nullptr;

    char networkSsids_[MAX_NETWORKS][SSID_MAX_LENGTH + 1] = {};
    bool networkIsOpen_[MAX_NETWORKS] = {};
    uint8_t networkCount_ = 0;
    uint8_t selectedNetworkIndex_ = 0;

    // Shared by EnterSsidManual and EnterPassword -- only one is ever
    // in progress at a time.
    TextEntry textEntry_;

    char pendingSsid_[SSID_MAX_LENGTH + 1] = {};
    char previousSsid_[SSID_MAX_LENGTH + 1] = {};
    char previousPassword_[65] = {};

    unsigned long stateEnteredMs_ = 0;
    bool connectSucceeded_ = false;
    int failureStatus_ = 0;

    void startScan();
    void pollScan();
    void beginConnect();
    void pollConnecting();

    void handleSelectNetworkEncoder(const EncoderEvent& event);
    void handleTextEntryEncoder(const EncoderEvent& event);

    void render();
    void renderSelectNetwork();
    void renderTextEntry(const char* title);
    void renderConnecting();
    void renderResult();
};
