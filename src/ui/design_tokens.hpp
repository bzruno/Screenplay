#pragma once
// ui/design_tokens.hpp
// Central design-system constants — spacing, radius, sizes, elevation, motion.
// Pure data, no Qt dependency: any widget that needs a padding/radius/size
// reads it from here instead of writing a literal. This is the single place
// that defines "what numbers this UI uses" — never sprinkle 3px/5px/7px
// through stylesheets again; pick the nearest token instead.
//
// Owned responsibility: numeric design tokens only. Colour lives in
// ThemePalette/ThemeManager (theme_palette.hpp / theme_manager.hpp); font
// family/size scale lives in Typography (typography.hpp).

namespace screenplay::ui {

// 4pt grid — every spacing value in the app is one of these, or a sum of them.
struct Spacing {
    static constexpr int XS = 4;
    static constexpr int S  = 8;
    static constexpr int M  = 12;
    static constexpr int L  = 16;
    static constexpr int XL = 24;
};

// Corner radius — named by the surface each level belongs to, so a call site
// asks for "the button radius", never for "12". The old three-level scale
// (4/6/10) produced the boxy, Win32-adjacent look the 2026 redesign replaces;
// every level below is deliberately generous.
struct Radius {
    static constexpr int Chip    = 8;    // small inline chips, badges, tags
    static constexpr int Button  = 12;   // every button and tool button
    static constexpr int Menu    = 14;   // menus and dropdown surfaces
    static constexpr int Card    = 16;   // panels, popups
    static constexpr int Pill    = 999;  // fully rounded (pills)

    // Legacy alias — kept so any not-yet-migrated call site still compiles
    // and lands on a value from the new scale instead of the old boxy one.
    static constexpr int Small  = Chip;
};

// Icon glyph sizes — the pixmap viewport IconManager draws into is always
// Canvas; RenderSize is what gets requested from QIcon at each call site.
struct IconSize {
    static constexpr int Canvas = 24;  // IconManager glyph viewport (drawing space)

    // THE chrome glyph size. Every button the user sees — writing capsule,
    // header, status strip, window controls — renders its icon at exactly
    // this. One value, one place: if icons ever look inconsistent again, it is
    // because a call site hardcoded a number instead of asking for this.
    static constexpr int Chrome = 19;

    // Legacy aliases, all pointing at Chrome so nothing can drift.
    static constexpr int Small     = Chrome;
    static constexpr int Toolbar   = Chrome;
};

// Control heights — the redesign brief calls for 40-44px touch-comfortable
// controls; everything the user clicks is one of these.
struct ControlHeight {
    static constexpr int Compact = 32;  // status strip affordances
    static constexpr int Button  = 40;  // toolbar / header buttons
};

// The rhythm of a row of icon buttons. Both the header and the writing capsule
// build their rows from these, so a group of tools is spaced identically
// wherever it sits — they used to disagree (the header aired its buttons, the
// capsule butted them together), which is what made one row read as tidier
// than the other.
struct ButtonRhythm {
    static constexpr int WithinGroup   = Spacing::XS;   // 4
    static constexpr int BetweenGroups = Spacing::L;    // 16
};

// Chrome element heights.
struct ChromeHeight {
    static constexpr int StatusBar = 34;   // a quiet footer, not a bar
};

// Side panel widths — the redesign widens the sidebar (Arc-like) from 260.
struct SidebarWidth {
    static constexpr int Default = 300;
    static constexpr int Minimum = 240;
};

// Hairline border thickness — the app never uses anything thicker.
struct BorderWidth {
    static constexpr int Hairline = 1;
};

// ── Elevation ────────────────────────────────────────────────────────────────
// Shadows are large-blur / very-low-alpha so surfaces read as "floating"
// without ever darkening the page. Never a hard, dark, offset drop shadow.
// Alpha values are 0-255 and get scaled per theme (see ThemePalette::shadow()).
struct Elevation {
    struct Level {
        int blur;
        int y_offset;
        int alpha;
    };
    // Resting card (toolbar, panels): a wide, very faint halo — the shadow
    // should be felt rather than seen. Large blur with low alpha reads as
    // "suspended"; a tighter, darker shadow reads as "stuck on top of".
    static constexpr Level Card    { 44, 6,  14 };
};

// Scrollbar geometry — the ONE definition; both the global QSS and the
// canvas-specific scrollbar must build their rule from these, so the two can
// never again drift apart. Thin and fully rounded per the redesign brief.
struct ScrollbarMetrics {
    static constexpr int Width           = 8;
    static constexpr int HandleRadius    = 4;   // == Width/2 → fully rounded
    static constexpr int MinHandleLength = 32;
};

// Animation durations (ms). Honoured by the custom-painted controls in
// ui/controls.hpp, which interpolate their hover/press wash instead of letting
// QSS snap between states.
struct AnimationDuration {
    static constexpr int Hover   = 120;
    static constexpr int Pressed = 100;
    static constexpr int Focus   = 120;
    static constexpr int Fade    = 120;   // dropdown / tooltip fade-in
};

} // namespace screenplay::ui
