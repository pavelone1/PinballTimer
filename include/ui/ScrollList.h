#pragma once

#include <cstdint>

// Shared by every on-device scrollable list menu (DirectorMenu,
// WifiSetupMenu's network list, BootMenu's submenus) so the fiddly
// windowing/rotation math isn't reimplemented per menu. Deliberately
// just two pure free functions, not a rendering component -- each
// menu still formats its own lines (cursor glyph, suffixes like " *"
// for locked networks, etc. all differ per caller), only the index
// math is shared.
namespace ScrollList {

// First visible index of a window `maxVisible` items wide over
// `itemCount` items, keeping `selected` visible (centered where
// possible, clamped at the ends).
inline uint8_t scrollWindowStart(uint8_t selected, uint8_t itemCount, uint8_t maxVisible)
{
    if (itemCount <= maxVisible) {
        return 0;
    }

    uint8_t windowStart = 0;
    if (selected >= maxVisible / 2) {
        windowStart = selected - maxVisible / 2;
    }

    const uint8_t maxStart = itemCount - maxVisible;
    if (windowStart > maxStart) {
        windowStart = maxStart;
    }

    return windowStart;
}

// Wraps a selection index by one step; matches the rotate-to-move,
// wrap-at-the-ends convention used by every menu in this UI.
inline uint8_t rotate(uint8_t current, uint8_t count, bool clockwise)
{
    if (count == 0) {
        return 0;
    }

    return clockwise ? (current + 1) % count : (current + count - 1) % count;
}

} // namespace ScrollList
