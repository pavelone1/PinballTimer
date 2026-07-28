#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include "output/TftDisplayManager.h"
#include "storage/SettingsStorage.h"
#include "network/WifiPortal.h"

// Standalone diagnostic: exercises ONLY the TFT and the WiFi
// captive-portal setup flow (WifiPortal -- the browser-based "join a
// network" module) together, with none of PinballTimer's game/input
// classes (no GameModeManager, ButtonInput, TimerManager, etc.) in
// the build at all. This is the fuller-featured sibling of
// examples/tft_httpd_test.cpp -- see that file's header comment and
// CLAUDE.md's "Firmware status" postmortem for why the WiFi.mode()
// call below (before httpd_start(), before WifiPortal.begin()/open())
// is required: httpd_start() needs lwIP's TCP/IP task already running
// to open its listening socket.
// Build/upload with: pio run -e wifi-menu-test -t upload

TftDisplayManager tft;
SettingsStorage settings;
WifiPortal wifiPortal;
httpd_handle_t server = nullptr;

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("[test] tft.begin starting"); Serial.flush();
    tft.begin();
    Serial.println("[test] tft.begin survived"); Serial.flush();

    settings.begin();
    Serial.println("[test] settings.begin survived"); Serial.flush();

    // Brings up lwIP's TCP/IP task -- required before httpd_start()
    // below can open a socket (see file header comment). WifiPortal's
    // own WiFi.mode()/softAP() calls happen later, inside open(), too
    // late to cover this.
    WiFi.mode(WIFI_STA);

    Serial.println("[test] httpd_start starting"); Serial.flush();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    httpd_start(&server, &config);
    Serial.println("[test] httpd_start survived"); Serial.flush();

    wifiPortal.begin(server, settings, tft, "test1234");
    Serial.println("[test] wifiPortal.begin survived"); Serial.flush();

    wifiPortal.open(); // brings up the AP + shows the setup screen on the TFT
    Serial.println("[test] wifiPortal.open survived"); Serial.flush();
}

void loop()
{
    wifiPortal.update();
}
