#pragma once
// Colour access for code that predates ThemePalette. `MD3` is a read-only
// facade over the active theme, refreshed by sync() on every theme change.

#include "theme_manager.hpp"
#include "../model/model.hpp"

#include <QColor>
#include <QString>

namespace MD3 {

using screenplay::ui::ThemeManager;

inline QColor Surface, SurfaceVar, OnSurface, Primary, OnPrimary,
              PrimaryContainer, Secondary, Outline, Error,
              PageBg, PageBorder, PageShadow, PageText, PageTextDim,
              Canvas, Bg0, Bg1, Card, Divider, Border, Text, TextDim,
              HoverBg, PressedBg, SelectionBg,
              ScrollbarBg, ScrollbarHoverBg,
              GoodAccent, WarnAccent, DangerAccent;

/// True only for the Dark theme. Chamber's page is light warm paper, so it
/// deliberately takes the same branch as Light wherever this is tested.
inline bool dark = true;

inline void sync() {
    const auto& p = ThemeManager::instance().palette();
    dark = (ThemeManager::instance().theme() == ThemeManager::Theme::Dark);

    Canvas = p.Canvas; PageBg = p.PageBg; PageBorder = p.PageBorder;
    PageShadow = p.PageShadow; PageText = p.PageText; PageTextDim = p.PageTextDim;
    Bg0 = p.Bg0; Bg1 = p.Bg1; Card = p.Card; Divider = p.Divider;
    Border = p.Border; Text = p.Text; TextDim = p.TextDim;
    HoverBg = p.HoverBg; PressedBg = p.PressedBg; SelectionBg = p.SelectionBg;
    ScrollbarBg = p.ScrollbarBg; ScrollbarHoverBg = p.ScrollbarHoverBg;
    GoodAccent = p.GoodAccent; WarnAccent = p.WarnAccent; DangerAccent = p.DangerAccent;

    Surface = p.Surface(); SurfaceVar = p.SurfaceVar(); OnSurface = p.OnSurface();
    Primary = p.Primary; OnPrimary = p.OnPrimary(); PrimaryContainer = p.PrimaryContainer();
    Secondary = p.Secondary(); Outline = p.Outline(); Error = p.Error();
}

inline int caretTintAlpha() { return ThemeManager::instance().palette().CaretTintAlpha; }
inline int fillTintAlpha()  { return ThemeManager::instance().palette().FillTintAlpha; }

inline QString hx(const QColor& c) { return c.name(QColor::HexRgb); }

inline QString rgba(const QColor& c, int alpha) {
    return QString("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

inline QString hoverSoft()   { return ThemeManager::instance().palette().hoverSoft(); }
inline QString pressedSoft() { return ThemeManager::instance().palette().pressedSoft(); }

/// Translucent wash for on-page rendering (selection, active-block tint).
inline QColor pageOverlay(int lightAlpha, int darkAlpha) {
    return dark ? QColor(255, 255, 255, darkAlpha)
                : QColor(0, 0, 0, lightAlpha);
}

} // namespace MD3

namespace screenplay::ui {

/// Accent identifying a screenplay element. Saturations are chosen to hold
/// contrast on both a white and a dark page, so one value serves every theme.
inline QColor block_color(screenplay::BlockType t) {
    switch (t) {
    case screenplay::BlockType::SceneHeading:  return { 0x7C, 0x4D, 0xFF };
    case screenplay::BlockType::Action:        return { 0x2E, 0xA0, 0x43 };
    case screenplay::BlockType::Character:     return { 0xF5, 0x7C, 0x00 };
    case screenplay::BlockType::Parenthetical: return { 0xC8, 0x94, 0x00 };
    case screenplay::BlockType::Dialogue:      return { 0x1E, 0x88, 0xE5 };
    case screenplay::BlockType::Transition:    return { 0xD8, 0x1B, 0x8C };
    case screenplay::BlockType::DualDialogue:  return { 0x1E, 0x88, 0xE5 };
    case screenplay::BlockType::Shot:          return { 0x00, 0x89, 0x7B };
    case screenplay::BlockType::General:       return { 0x60, 0x6C, 0x76 };
    case screenplay::BlockType::ActBreak:      return { 0x8E, 0x24, 0xAA };
    }
    return MD3::TextDim;
}

/// Black or white, whichever reads better on `background` (WCAG luminance).
inline QColor contrast_text(const QColor& background) {
    const double luminance = (0.299 * background.red()
                            + 0.587 * background.green()
                            + 0.114 * background.blue()) / 255.0;
    return luminance > 0.6 ? QColor(0x14, 0x14, 0x14) : QColor(0xFF, 0xFF, 0xFF);
}

} // namespace screenplay::ui
