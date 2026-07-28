#pragma once
// ui/theme_palette.hpp
// ThemePalette — every colour the application chrome and page rendering use,
// as one value type. A theme IS a ThemePalette; ThemeManager just owns which
// one is active. This replaces the old scheme of loose `inline QColor`
// globals in namespace MD3 with an actual data structure, so a third theme
// (Chamber) is "add a factory function", not "add another branch to every
// `if (dark) … else …` in the app".
//
// Owned responsibility: colour values only. Numeric spacing/radius tokens
// live in design_tokens.hpp; font family/size in typography.hpp.

#include <QColor>
#include <QString>

namespace screenplay::ui {

struct ThemePalette {
    // ── Chrome (window frame, menus, toolbars, docks, status bar) ──────────
    // The app background. Deliberately IDENTICAL to Canvas in every theme:
    // the window must read as one uninterrupted backdrop with the page
    // resting on it, never as a chrome band above a darker workspace strip.
    // Kept as its own field (rather than deleted in favour of Canvas) because
    // chrome code asks for a chrome colour and page code asks for a workspace
    // colour — they simply agree on the value.
    QColor Bg0;
    QColor Bg1;         // panel / popup / toolbar surface
    QColor Card;        // elevated floating surface (toolbar card, menus,
                        // dialogs) — sits ABOVE Bg1, never equal to it
    QColor Border;      // hairline borders (primary separator)
    QColor Divider;     // near-invisible separator, softer than Border
    QColor Shadow;      // colour drop shadows are tinted with
    QColor Text;        // primary UI text
    QColor TextDim;     // secondary UI text
    QColor HoverBg;      // active / checked fill (toolbar/menu hover-on state)
    QColor PressedBg;    // pressed fill
    QColor SelectionBg;  // text selection in chrome widgets (lists, line edits)
    QColor ScrollbarBg;      // scrollbar handle
    QColor ScrollbarHoverBg; // scrollbar handle, hovered
    QColor GoodAccent;   // "saved", match count OK
    QColor WarnAccent;   // "unsaved", no matches (cautionary, non-fatal)
    QColor DangerAccent; // reserved for hard errors (distinct from WarnAccent)
    QColor Primary;      // brand accent — primary buttons, focus ring, caret

    // ── Workspace + page (the screenplay canvas) ────────────────────────────
    QColor Canvas;       // workspace backdrop behind the page
    QColor PageBg;
    QColor PageBorder;
    QColor PageShadow;
    QColor PageText;
    QColor PageTextDim;

    // ── On-page overlay tint alphas (0-255) ─────────────────────────────────
    // Previously hard-branched as `MD3::dark ? A : B` at two call sites; a
    // three-theme system needs a real per-theme value, not a boolean split.
    int CaretTintAlpha = 0;   // caret/active-line wash on the page
    int FillTintAlpha  = 0;   // selection/highlight fill wash on the page

    // Derived Qt::Palette-compatible aliases some call sites already expect.
    QColor Surface()          const { return Bg0; }
    QColor SurfaceVar()       const { return HoverBg; }
    QColor OnSurface()        const { return Text; }
    QColor OnPrimary()        const { return contrastOn(Primary); }
    QColor PrimaryContainer() const { return HoverBg; }
    QColor Secondary()        const { return TextDim; }
    QColor Outline()          const { return Border; }
    QColor Error()            const { return WarnAccent; }

    static QColor contrastOn(const QColor& bg) {
        const double L = (0.299 * bg.red() + 0.587 * bg.green()
                          + 0.114 * bg.blue()) / 255.0;
        return L > 0.6 ? QColor(0x14, 0x14, 0x14) : QColor(0xFF, 0xFF, 0xFF);
    }

    static QString hex(const QColor& c) { return c.name(QColor::HexRgb); }
    static QString rgba(const QColor& c, int alpha) {
        return QString("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
    }

    // Neutral hover/pressed washes: bright-on-dark or dark-on-light, low
    // alpha, so plain hover reads as "lit up" without borrowing the vivid
    // Primary tint reserved for genuinely active/checked/selected states.
    QString hoverSoft()   const { return rgba(overlayBase(), 22); }
    QString pressedSoft() const { return rgba(overlayBase(), 36); }

    // True when this is a dark-chrome theme. Shadows need more alpha on dark
    // surfaces to register at all; elevation helpers ask via this.
    bool isDarkChrome() const { return luminance(Bg1) <= 0.5; }

    // ── Factories — the only three themes the app ships today ──────────────
    static ThemePalette light();
    static ThemePalette dark();
    static ThemePalette chamber();

private:
    static double luminance(const QColor& c) {
        return (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) / 255.0;
    }
    // The colour a translucent "lit up" wash is built from: white for dark
    // chrome, black for light/warm chrome.
    QColor overlayBase() const {
        return luminance(Bg1) > 0.5 ? QColor(0, 0, 0) : QColor(255, 255, 255);
    }
};

inline ThemePalette ThemePalette::dark() {
    ThemePalette p;
    // Near-neutral dark greys with a whisper of blue so the surfaces read as
    // deliberate rather than muddy — the Linear/Arc register. Never pure
    // black; the card surface always sits a step above the app background so
    // floating elements separate without needing a heavy border.
    p.Canvas      = { 0x14, 0x14, 0x17 };
    p.PageBg      = { 0x2A, 0x2A, 0x2A };
    p.PageBorder  = { 0x3C, 0x3C, 0x3C };
    p.PageShadow  = { 0x00, 0x00, 0x00 };
    p.PageText    = { 0xD6, 0xD6, 0xD6 };
    p.PageTextDim = { 0x8A, 0x8A, 0x8A };
    p.Bg0         = p.Canvas;   // uniform backdrop — see note in light()
    p.Bg1         = { 0x17, 0x17, 0x1B };
    p.Card        = { 0x1E, 0x1E, 0x23 };
    p.Border      = { 0x2C, 0x2C, 0x33 };
    p.Divider     = { 0x23, 0x23, 0x29 };
    p.Shadow      = { 0x00, 0x00, 0x00 };
    p.Text        = { 0xEC, 0xEC, 0xF0 };
    p.TextDim     = { 0x8E, 0x8E, 0x9C };
    p.HoverBg     = { 0x2A, 0x2A, 0x31 };
    p.PressedBg   = { 0x34, 0x34, 0x3D };
    p.SelectionBg = { 0x33, 0x2C, 0x54 };
    p.ScrollbarBg      = { 0x35, 0x35, 0x3E };
    p.ScrollbarHoverBg = { 0x4C, 0x4C, 0x59 };
    p.GoodAccent  = { 0x4A, 0xD1, 0x91 };
    p.WarnAccent  = { 0xE0, 0x8A, 0x5C };
    p.DangerAccent= { 0xE5, 0x5C, 0x5C };
    p.Primary     = { 0x8B, 0x5C, 0xF6 };   // violet — the 2026 brand accent
    p.CaretTintAlpha = 26;
    p.FillTintAlpha  = 46;
    return p;
}

inline ThemePalette ThemePalette::light() {
    ThemePalette p;
    // Soft off-white chrome — pure #FFF is reserved for the page and for the
    // floating cards, so the elevation reads even with near-invisible borders.
    p.Canvas      = { 0xF4, 0xF4, 0xF7 };
    p.PageBg      = { 0xFF, 0xFF, 0xFF };
    p.PageBorder  = { 0xE6, 0xE6, 0xEB };
    p.PageShadow  = { 0x1A, 0x1A, 0x2E };
    p.PageText    = { 0x1A, 0x1A, 0x1A };
    p.PageTextDim = { 0x8A, 0x8A, 0x8A };
    p.Bg0         = p.Canvas;   // uniform backdrop — see note below
    p.Bg1         = { 0xF4, 0xF4, 0xF7 };
    p.Card        = { 0xFF, 0xFF, 0xFF };
    p.Border      = { 0xE4, 0xE4, 0xEA };
    p.Divider     = { 0xEE, 0xEE, 0xF2 };
    p.Shadow      = { 0x1A, 0x1A, 0x2E };
    p.Text        = { 0x1B, 0x1B, 0x22 };
    p.TextDim     = { 0x74, 0x74, 0x82 };
    p.HoverBg     = { 0xEC, 0xEC, 0xF1 };
    p.PressedBg   = { 0xE0, 0xE0, 0xE8 };
    p.SelectionBg = { 0xE4, 0xDC, 0xFD };
    p.ScrollbarBg      = { 0xD2, 0xD2, 0xDC };
    p.ScrollbarHoverBg = { 0xB4, 0xB4, 0xC2 };
    p.GoodAccent  = { 0x14, 0x9E, 0x6A };
    p.WarnAccent  = { 0xC2, 0x6A, 0x2E };
    p.DangerAccent= { 0xC6, 0x28, 0x28 };
    p.Primary     = { 0x6D, 0x3F, 0xE8 };   // violet — the 2026 brand accent
    p.CaretTintAlpha = 20;
    p.FillTintAlpha  = 34;
    return p;
}

inline ThemePalette ThemePalette::chamber() {
    // "Chamber" — an old library by candlelight, the flame a bit more alive:
    // warmer, more saturated chrome (workspace/toolbar/hover/selection), a
    // real ember-orange accent, and a visibly sandy paper (not near-white —
    // the earlier cream read as plain white on screen). Never pure white,
    // never pure black. Per the redesign brief this theme's identity is
    // preserved verbatim; only the new structural tokens (Card, Divider,
    // Shadow) are added, tuned to the existing warm range.
    ThemePalette p;
    p.Canvas      = { 0xE1, 0xD1, 0xB3 };   // workspace — warmer, richer than before
    p.PageBg      = { 0xF0, 0xE2, 0xC4 };   // paper — light sand, clearly warm
    p.PageBorder  = { 0xCB, 0xB6, 0x8C };   // deepened to still read against the paper
    p.PageShadow  = { 0x2D, 0x2A, 0x26 };   // warm dark, not black
    p.PageText    = { 0x2D, 0x2A, 0x26 };   // primary text
    p.PageTextDim = { 0x6B, 0x64, 0x5B };   // secondary text
    p.Bg0         = p.Canvas;   // uniform backdrop — see note in light()
    p.Bg1         = { 0xEE, 0xDF, 0xC2 };   // toolbar band
    p.Card        = { 0xF7, 0xEC, 0xD6 };   // floating card — lifted, still sand
    p.Border      = { 0xC8, 0xAF, 0x8A };   // deeper, more "wood"
    p.Divider     = { 0xDC, 0xC7, 0xA2 };   // softer than Border, same wood family
    p.Shadow      = { 0x4A, 0x3A, 0x24 };   // warm brown shadow, never black
    p.Text        = { 0x2D, 0x2A, 0x26 };
    p.TextDim     = { 0x6B, 0x64, 0x5B };
    p.HoverBg     = { 0xEA, 0xD3, 0xA7 };   // richer amber wash
    p.PressedBg   = { 0xDF, 0xC4, 0x93 };
    p.SelectionBg = { 0xE0, 0xB8, 0x74 };   // more saturated gold, still legible
    p.ScrollbarBg      = { 0xB5, 0xAA, 0x98 };   // per spec
    p.ScrollbarHoverBg = { 0x98, 0x8C, 0x79 };   // per spec
    p.GoodAccent  = { 0x6B, 0x64, 0x5B };   // stays neutral-warm, not green
    p.WarnAccent  = { 0xB4, 0x43, 0x3A };   // caution stays legible red
    p.DangerAccent= { 0xA8, 0x32, 0x26 };   // stronger red, still warm-legible
    p.Primary     = { 0xAD, 0x62, 0x28 };   // ember-orange accent — the "flame"
    p.CaretTintAlpha = 34;
    p.FillTintAlpha  = 54;
    return p;
}

} // namespace screenplay::ui
