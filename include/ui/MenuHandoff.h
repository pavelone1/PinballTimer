#pragma once

#include <cstdint>

// What the caller (App) should do after routing an encoder event into
// an open on-device menu (DirectorMenu or BootMenu). Both menus have
// items that hand off to a different class's multi-step flow rather
// than firing a single in-place action -- App owns those instances
// (WifiSetupMenu, WifiPortal), so the menu itself can't open them
// directly.
enum class MenuHandoff : uint8_t {
    None,           // selection moved or nothing happened, menu stays open
    Close,          // item executed (or cancelled) -- close() the menu (and, for DirectorMenu, resume the game)
    OpenWifiSetup,  // hand off to WifiSetupMenu (rotary-encoder WiFi join flow)
    OpenWifiPortal, // hand off to WifiPortal (phone/web WiFi join flow)
    RevertToAdhoc,  // hand off to WifiPortal::revertToAdhoc() (BootMenu only)
    EndGame         // DirectorMenu only: reset the mode and return to BootMenu, NOT resume
};
