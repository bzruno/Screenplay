#pragma once
// Production revision passes.
//
// Once a screenplay goes into production it stops being reissued as a whole:
// each new pass is printed on a different colour of paper, always in the same
// order, and only the changed pages are distributed. "We're shooting off the
// Pink pages" is a statement everyone on a set understands, so the colour
// sequence below is not decoration — it is the document's version number.

#include <cstdint>

namespace screenplay {

/// The standard colour ladder. `None` is the original white draft; the order
/// of the rest is fixed by industry convention and must not be rearranged.
enum class Revision : uint8_t {
    None = 0,
    Blue,
    Pink,
    Yellow,
    Green,
    Goldenrod,
    Salmon,
    Cherry,
    Buff,
    Tan,
};

inline constexpr int kRevisionCount = 10;   // None + 9 colours

/// English name, as printed on the title page ("Blue Revision — 12/03/26").
/// Translated for display by config::tr_ui(); this is the canonical spelling
/// used in files, so it must never be localised at the source.
inline const char* revision_name(Revision r) {
    switch (r) {
    case Revision::None:      return "White";
    case Revision::Blue:      return "Blue";
    case Revision::Pink:      return "Pink";
    case Revision::Yellow:    return "Yellow";
    case Revision::Green:     return "Green";
    case Revision::Goldenrod: return "Goldenrod";
    case Revision::Salmon:    return "Salmon";
    case Revision::Cherry:    return "Cherry";
    case Revision::Buff:      return "Buff";
    case Revision::Tan:       return "Tan";
    }
    return "White";
}

/// The paper's own colour, 0xRRGGBB — the real stock, not a UI accent, so a
/// reader recognises the pass at a glance. Kept here beside the names because
/// the colour IS the convention; the UI only decides how to show it.
inline constexpr uint32_t revision_rgb(Revision r) {
    switch (r) {
    case Revision::None:      return 0xFFFFFF;
    case Revision::Blue:      return 0x9CC3E8;
    case Revision::Pink:      return 0xF4B8CB;
    case Revision::Yellow:    return 0xF4E07A;
    case Revision::Green:     return 0xA8D5A2;
    case Revision::Goldenrod: return 0xE0B84C;
    case Revision::Salmon:    return 0xF2A48A;
    case Revision::Cherry:    return 0xD9536B;
    case Revision::Buff:      return 0xE8D9B0;
    case Revision::Tan:       return 0xC9A97E;
    }
    return 0xFFFFFF;
}

/// The next pass in the ladder; Tan is the last, so it stays put rather than
/// wrapping back to an earlier colour and losing the ordering.
inline Revision next_revision(Revision r) {
    const int n = (int)r + 1;
    return n < kRevisionCount ? (Revision)n : Revision::Tan;
}

} // namespace screenplay
