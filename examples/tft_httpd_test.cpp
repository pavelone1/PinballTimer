#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include "output/TftDisplayManager.h"

// Standalone diagnostic: TFT + a bare esp_http_server instance only --
// no WifiPortal, no DNSServer, no SettingsStorage, no PinballTimer
// game/input classes at all. This is the minimal reproduction from
// CLAUDE.md's "Firmware status" postmortem: httpd_start() needs
// lwIP's TCP/IP task already running (via WiFi.mode()) before it can
// open a listening socket -- skip the WiFi.mode() call below and this
// crashes on every boot with "assert failed: tcpip_send_msg_wait_sem
// ... (Invalid mbox)", which from the outside looks exactly like an
// unrelated TFT/SPI crash, since execution has already moved on from
// tft.begin() by the time httpd_start() actually aborts.
// Build/upload with: pio run -e tft-httpd-test -t upload

TftDisplayManager tft;
httpd_handle_t server = nullptr;

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("[test] tft.begin starting"); Serial.flush();
    tft.begin();
    Serial.println("[test] tft.begin survived"); Serial.flush();

    // Brings up lwIP's TCP/IP task -- required before httpd_start()
    // below can open a socket. See file header comment.
    WiFi.mode(WIFI_STA);

    Serial.println("[test] httpd_start starting"); Serial.flush();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);
    Serial.println("[test] httpd_start survived"); Serial.flush();
}

void loop()
{
    delay(1000);
}
