#pragma once

// WiFi is enabled through the exclusive-mode implementation:
// the radio is either a station or an access point, never WIFI_AP_STA.
// The latter was the mode implicated by the REV2 coredump.
constexpr bool kWifiFeatureEnabled = true;

// TEMPORARY DEVELOPMENT POLICY: the timer is bench-powered while firmware is
// being built and debugged. Keep the radio powered and modem sleep disabled
// until the project reaches its alpha milestone, then set this to false and
// restore the normal explicit-on/boot-off power policy.
constexpr bool kPreAlphaWifiAlwaysOn = true;
