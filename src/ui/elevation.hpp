#pragma once
// ui/elevation.hpp
// Elevation — turns an Elevation::Level token into a real Qt drop shadow on a
// widget. One helper, so no widget ever constructs its own
// QGraphicsDropShadowEffect with hand-picked blur/offset/alpha numbers.
//
// House style, enforced here rather than per call site: large blur, small
// downward offset, very low alpha, tinted with the theme's Shadow colour
// (never pure black on a warm theme). Dark chrome needs more alpha for the
// same perceived lift, so the level's alpha is scaled per theme.
//
// Owned responsibility: shadow construction only. The numbers come from
// DesignTokens::Elevation; the colour from ThemePalette::Shadow.

#include "design_tokens.hpp"
#include "theme_palette.hpp"

#include <QGraphicsDropShadowEffect>
#include <QWidget>

namespace screenplay::ui {

class ElevationFx {
public:
    // Applies (or replaces) the drop shadow on `w`. Safe to call repeatedly —
    // a theme change just re-applies with the new tint.
    static void apply(QWidget* w, const Elevation::Level& level,
                      const ThemePalette& pal) {
        if (!w) return;
        auto* fx = qobject_cast<QGraphicsDropShadowEffect*>(w->graphicsEffect());
        if (!fx) {
            fx = new QGraphicsDropShadowEffect(w);
            w->setGraphicsEffect(fx);
        }
        QColor c = pal.Shadow;
        c.setAlpha(scaled_alpha(level.alpha, pal));
        fx->setBlurRadius(level.blur);
        fx->setOffset(0, level.y_offset);
        fx->setColor(c);
    }

    // The margin a layout must leave around an elevated widget so its shadow
    // isn't clipped by the parent's bounds.
    static int margin_for(const Elevation::Level& level) {
        return level.blur / 2 + level.y_offset;
    }

private:
    // A shadow that reads correctly on paper is invisible on near-black
    // chrome; dark themes get roughly half again as much alpha.
    static int scaled_alpha(int base, const ThemePalette& pal) {
        const int a = pal.isDarkChrome() ? base * 3 / 2 : base;
        return a > 255 ? 255 : a;
    }
};

} // namespace screenplay::ui
