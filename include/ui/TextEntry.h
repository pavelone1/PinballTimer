#pragma once

#include <cstdint>
#include "SystemTypes.h"

// One-encoder character picker, shared by every on-device text-entry
// need (WiFi SSID/password in WifiSetupMenu, player names in
// BootMenu's Player Info submenu): rotate cycles a fixed charset,
// click appends the highlighted character, continuing to rotate past
// the alphabet reaches DONE and DEL, long-press cancels.
//
// Owns only the buffer/cursor state and the encoder-event state
// machine -- it does not render anything itself (callers build their
// own TftDisplayManager::showStatusScreen() lines from text()/
// currentPickerLabel(), since exact wording/layout differs per
// caller) and does not decide what DONE/Cancel mean (surfaced back
// via the Result enum, same "caller owns cancel" convention as
// DirectorMenu/WifiSetupMenu/WifiPortal elsewhere in this UI).
//
// Has no Arduino/hardware dependency (SystemTypes.h is pure
// cstdint), unlike most of the UI layer -- native-testable if that's
// ever worth adding.
class TextEntry {
public:
    enum class Result : uint8_t {
        None,   // rotation, append, or delete -- caller should re-render
        Done,   // DONE picked -- caller reads text()
        Cancel  // long-press -- caller decides what cancelling means
    };

    static constexpr uint8_t MAX_LENGTH = 32;

    void reset(const char* initialText = "");

    Result handleEncoderEvent(const EncoderEvent& event);

    const char* text() const;
    uint8_t length() const;

    // Current highlighted picker entry, formatted for display:
    // a single character, "DONE", or "DEL".
    void currentPickerLabel(char* outBuf, uint8_t bufSize) const;

private:
    char buffer_[MAX_LENGTH + 1] = "";
    uint8_t length_ = 0;
    uint8_t pickerIndex_ = 0;
};
