#include "network/OtaManager.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

void OtaManager::begin(const char* hostname, const char* password, TftDisplayManager& tft)
{
    tft_ = &tft;

    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword(password);

    ArduinoOTA.onStart([this]() {
        Serial.println("[OTA] Update starting");
        const char* lines[] = {"Do not power off"};
        tft_->showStatusScreen("UPDATING FIRMWARE", lines, 1, ColorId::Black, ColorId::Orange, ColorId::White);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] Update complete, rebooting");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned long lastLogMs = 0;
        const unsigned long now = millis();
        if (now - lastLogMs >= 500 && total > 0) {
            lastLogMs = now;
            Serial.printf("[OTA] %u%%\n", (progress * 100) / total);
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]\n", static_cast<unsigned int>(error));
    });

    ArduinoOTA.begin();
}

void OtaManager::update()
{
    ArduinoOTA.handle();
}
