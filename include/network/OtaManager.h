#pragma once

#include "output/TftDisplayManager.h"

// Wraps ArduinoOTA (bundled with the ESP32 Arduino core, no extra
// lib_deps entry needed) so firmware can be pushed over WiFi without
// opening the enclosure. Once begin() has run, `pio run -e app -t
// upload --upload-port <device-ip>` works exactly like a USB upload --
// PlatformIO's OTA uploader speaks ArduinoOTA's protocol natively.
//
// Password-gated (see Secrets.h.example): an unauthenticated OTA
// endpoint would let anyone on the network overwrite the device's
// firmware outright, which is a materially bigger risk than
// DirectorControl's unauthenticated /command endpoint (that can only
// pause/reset a game, not replace the code running it).
class OtaManager {
public:
    void begin(const char* hostname, const char* password, TftDisplayManager& tft);
    void update();

private:
    TftDisplayManager* tft_ = nullptr;
};
