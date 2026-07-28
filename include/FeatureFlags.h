#pragma once

// Temporary, single-flag kill switch for the whole WiFi/HTTP subsystem
// (network stack, director HTTP API, OTA, WiFi portal, dashboard) --
// see CLAUDE.md's "WiFi heap corruption (open, disabled for now)"
// section for the full investigation. A confirmed real bug (heap
// corruption inside the WiFi driver's own task, verified via
// esp-coredump) is tripping shortly after WiFi.mode() runs; game-mode
// work is proceeding without it rather than blocking on a fix.
//
// Every WiFi-adjacent begin()/update() call and menu item checks this
// one constant (App::begin()/update(), BootMenu, DirectorMenu) -- flip
// it back to true here, in one place, once the underlying bug is
// fixed, rather than hunting down each call site again.
constexpr bool kWifiFeatureEnabled = false;
