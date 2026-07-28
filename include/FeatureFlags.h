#pragma once

// WiFi is enabled through the exclusive-mode implementation:
// the radio is either a station or an access point, never WIFI_AP_STA.
// The latter was the mode implicated by the REV2 coredump.
constexpr bool kWifiFeatureEnabled = true;
