#pragma once
// ui/icon_manager.hpp
// IconManager — every vector glyph the chrome uses, hand-drawn as a single
// stroked QPainterPath so the whole set shares one weight, one cap/join
// style, one viewport. This is the same drawing recipe the app already used
// (previously `namespace icons` in main.cpp) — relocated here per the
// project's extraction rule for UI subsystems, plus new glyphs for the
// formatting (Bold/Italic/Underline) and element-type (Scene/Action/
// Character/Dialogue) toolbar buttons.
//
// Owned responsibility: icon geometry + rasterisation only. Colour comes from
// the caller (usually a ThemePalette field); size/viewport constants live in
// DesignTokens.

#include "design_tokens.hpp"

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QColor>
#include <QPointF>
#include <QRectF>

#include <algorithm>

namespace screenplay::ui {

class IconManager {
public:
    enum class Id {
        New, Open, Save, Pdf, Print, Undo, Redo, Search,
        Scenes, Characters, Stats, Focus,
        ChevronUp, ChevronDown, Close, ZoomIn, ZoomOut,
        Sun, Moon, Candle,
        // Formatting toolbar (state reflects the selection/current block)
        Bold, Italic, Underline, Strikethrough,
        // Element-type toolbar (state reflects the current block's type)
        BlockScene, BlockAction, BlockCharacter, BlockDialogue,
        BlockParenthetical, BlockTransition,
        // Paragraph alignment
        AlignLeft, AlignCenter, AlignRight,
        // Chrome: header, sidebar and mode switch
        Menu, Document, Export, Plus, Notes, Comment,
        ChevronLeft, ChevronRight, Pencil, Review,
        // Window controls
        WinMinimize, WinMaximize, WinRestore,
    };

    static QIcon make(Id id, const QColor& color) {
        constexpr int S   = IconSize::Canvas;
        constexpr int dpr = 2;   // 2x raster for HiDPI
        QPixmap pm(S * dpr, S * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);

        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        // ONE weight for the whole family. The window controls used to be
        // drawn lighter than everything else; two weights in one interface
        // read as two icon sets, so they now share the family's stroke.
        QPen pen(color, kStroke);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        QPainterPath path;
        auto file_outline = [&] {                    // shared by New / Pdf
            path.moveTo(13, 3); path.lineTo(7, 3);
            path.quadTo(6, 3, 6, 4);   path.lineTo(6, 20);
            path.quadTo(6, 21, 7, 21); path.lineTo(17, 21);
            path.quadTo(18, 21, 18, 20); path.lineTo(18, 8);
            path.lineTo(13, 3);
            path.moveTo(13, 3); path.lineTo(13, 8); path.lineTo(18, 8);
        };

        switch (id) {
        case Id::New:
            file_outline();
            path.moveTo(9, 15);  path.lineTo(15, 15);
            path.moveTo(12, 12); path.lineTo(12, 18);
            break;
        case Id::Open:
            path.moveTo(3, 18); path.lineTo(3, 5);
            path.quadTo(3, 4, 4, 4);   path.lineTo(9, 4);
            path.lineTo(11, 6);        path.lineTo(20, 6);
            path.quadTo(21, 6, 21, 7); path.lineTo(21, 18);
            path.quadTo(21, 19, 20, 19); path.lineTo(4, 19);
            path.quadTo(3, 19, 3, 18);
            break;
        case Id::Save:
            path.addRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
            path.moveTo(8, 4);  path.lineTo(8, 9);
            path.lineTo(15, 9); path.lineTo(15, 4);
            path.moveTo(7, 20); path.lineTo(7, 13);
            path.lineTo(17, 13); path.lineTo(17, 20);
            break;
        case Id::Pdf:
            file_outline();
            path.moveTo(12, 11); path.lineTo(12, 18);
            path.moveTo(9, 15);  path.lineTo(12, 18); path.lineTo(15, 15);
            break;
        case Id::Print:
            path.moveTo(7, 8);  path.lineTo(7, 3);
            path.lineTo(17, 3); path.lineTo(17, 8);
            path.addRoundedRect(QRectF(4, 8, 16, 8), 1.5, 1.5);
            path.addRect(QRectF(7, 13, 10, 8));
            break;
        case Id::Undo:
            path.moveTo(9, 14); path.lineTo(4, 9); path.lineTo(9, 4);
            path.moveTo(4, 9);  path.lineTo(14.5, 9);
            path.arcTo(QRectF(9, 9, 11, 11), 90, -180);
            path.lineTo(11, 20);
            break;
        case Id::Redo:
            path.moveTo(15, 14); path.lineTo(20, 9); path.lineTo(15, 4);
            path.moveTo(20, 9);  path.lineTo(9.5, 9);
            path.arcTo(QRectF(4, 9, 11, 11), 90, 180);
            path.lineTo(13, 20);
            break;
        case Id::Search:
            path.addEllipse(QPointF(10.5, 10.5), 6, 6);
            path.moveTo(15, 15); path.lineTo(20, 20);
            break;
        case Id::Scenes:
            for (int y : {6, 12, 18}) {
                path.addEllipse(QPointF(5, y), 0.8, 0.8);
                path.moveTo(9, y); path.lineTo(20, y);
            }
            break;
        case Id::Characters:
            path.addEllipse(QPointF(9, 7), 3.2, 3.2);
            path.moveTo(3, 20);  path.quadTo(3, 14, 9, 14);
            path.quadTo(15, 14, 15, 20);
            path.addEllipse(QPointF(17.5, 8), 2.4, 2.4);
            path.moveTo(21, 20); path.quadTo(21, 15, 16.5, 14.5);
            break;
        case Id::Stats:
            path.moveTo(3, 3);  path.lineTo(3, 19);
            path.quadTo(3, 21, 5, 21); path.lineTo(21, 21);
            path.moveTo(8, 17);  path.lineTo(8, 13);
            path.moveTo(13, 17); path.lineTo(13, 5);
            path.moveTo(18, 17); path.lineTo(18, 9);
            break;
        case Id::Focus:
            path.addEllipse(QPointF(12, 12), 3, 3);
            path.moveTo(3, 7);  path.lineTo(3, 5);  path.quadTo(3, 3, 5, 3);   path.lineTo(7, 3);
            path.moveTo(17, 3); path.lineTo(19, 3); path.quadTo(21, 3, 21, 5); path.lineTo(21, 7);
            path.moveTo(21, 17); path.lineTo(21, 19); path.quadTo(21, 21, 19, 21); path.lineTo(17, 21);
            path.moveTo(7, 21); path.lineTo(5, 21); path.quadTo(3, 21, 3, 19); path.lineTo(3, 17);
            break;
        case Id::ChevronUp:
            path.moveTo(6, 15); path.lineTo(12, 9); path.lineTo(18, 15);
            break;
        case Id::ChevronDown:
            path.moveTo(6, 9); path.lineTo(12, 15); path.lineTo(18, 9);
            break;
        case Id::Close:
            path.moveTo(6, 6);  path.lineTo(18, 18);
            path.moveTo(18, 6); path.lineTo(6, 18);
            break;
        case Id::ZoomIn:
            path.addEllipse(QPointF(10.5, 10.5), 6, 6);
            path.moveTo(15, 15); path.lineTo(20, 20);
            path.moveTo(8, 10.5);  path.lineTo(13, 10.5);
            path.moveTo(10.5, 8);  path.lineTo(10.5, 13);
            break;
        case Id::ZoomOut:
            path.addEllipse(QPointF(10.5, 10.5), 6, 6);
            path.moveTo(15, 15); path.lineTo(20, 20);
            path.moveTo(8, 10.5); path.lineTo(13, 10.5);
            break;
        case Id::Sun: {
            path.addEllipse(QPointF(12, 12), 3.8, 3.8);
            static const double dirs[8][2] = {
                {1,0},{0.707,0.707},{0,1},{-0.707,0.707},
                {-1,0},{-0.707,-0.707},{0,-1},{0.707,-0.707}};
            for (const auto& d : dirs) {
                path.moveTo(12 + 6.2 * d[0], 12 + 6.2 * d[1]);
                path.lineTo(12 + 9.0 * d[0], 12 + 9.0 * d[1]);
            }
            break;
        }
        case Id::Moon:
            path.arcMoveTo(QRectF(4, 4, 16, 16), 100);
            path.arcTo(QRectF(4, 4, 16, 16), 100, 290);
            path.arcTo(QRectF(10.6, -0.78, 10.04, 10.04), -48.6, -133);
            break;
        case Id::Candle:
            // Body + wick + a small teardrop flame — the Chamber theme icon.
            path.addRoundedRect(QRectF(9, 11, 6, 10), 1.5, 1.5);
            path.moveTo(12, 11); path.lineTo(12, 8);
            path.moveTo(12, 8);
            path.quadTo(9.3, 5.2, 12, 2.5);
            path.quadTo(14.7, 5.2, 12, 8);
            break;

        // ── Formatting: Bold / Italic / Underline ──────────────────────────
        case Id::Bold:
            // Two stacked bumps against a vertical stem — a stroked "B".
            path.moveTo(7, 4); path.lineTo(7, 20);
            path.moveTo(7, 4); path.lineTo(13, 4);
            path.quadTo(17, 4, 17, 8);  path.quadTo(17, 12, 13, 12);
            path.lineTo(7, 12);
            path.moveTo(7, 12); path.lineTo(14, 12);
            path.quadTo(18, 12, 18, 16); path.quadTo(18, 20, 14, 20);
            path.lineTo(7, 20);
            break;
        case Id::Italic:
            // Slanted "I": top bar, slanted stem, bottom bar.
            path.moveTo(10, 4);  path.lineTo(17, 4);
            path.moveTo(13.5, 4); path.lineTo(10.5, 20);
            path.moveTo(7, 20);  path.lineTo(14, 20);
            break;
        case Id::Underline:
            // "U" bowl + baseline rule beneath it.
            path.moveTo(6, 4); path.lineTo(6, 12);
            path.quadTo(6, 18, 12, 18); path.quadTo(18, 18, 18, 12);
            path.lineTo(18, 4);
            path.moveTo(5, 21); path.lineTo(19, 21);
            break;

        // ── Element types: Scene / Action / Character / Dialogue ──────────
        case Id::BlockScene:
            // A location pennant — pole + flag, evoking "where" (scene heading).
            path.moveTo(7, 21); path.lineTo(7, 3);
            path.moveTo(7, 4);  path.lineTo(18, 4);
            path.lineTo(14.5, 8.5); path.lineTo(18, 13); path.lineTo(7, 13);
            break;
        case Id::BlockAction:
            // Descriptive prose — three ragged lines of body text.
            path.moveTo(4, 7);  path.lineTo(20, 7);
            path.moveTo(4, 12); path.lineTo(17, 12);
            path.moveTo(4, 17); path.lineTo(14, 17);
            break;
        case Id::BlockCharacter:
            // A single speaker — head + shoulders.
            path.addEllipse(QPointF(12, 8), 3.6, 3.6);
            path.moveTo(4, 21); path.quadTo(4, 13, 12, 13);
            path.quadTo(20, 13, 20, 21);
            break;
        case Id::BlockDialogue:
            // A speech bubble with a small tail.
            path.addRoundedRect(QRectF(4, 4, 16, 12), 4, 4);
            path.moveTo(9, 16); path.lineTo(7, 20); path.lineTo(12.5, 16);
            break;
        case Id::BlockParenthetical:
            // The wrylie's own punctuation: a facing pair of parentheses.
            path.moveTo(9.5, 4);  path.quadTo(5.5, 12, 9.5, 20);
            path.moveTo(14.5, 4); path.quadTo(18.5, 12, 14.5, 20);
            break;
        case Id::BlockTransition:
            // Two arrows crossing the cut — one scene handing off to the next.
            path.moveTo(3, 8);  path.lineTo(15, 8);
            path.moveTo(11, 4); path.lineTo(15, 8); path.lineTo(11, 12);
            path.moveTo(21, 16); path.lineTo(9, 16);
            path.moveTo(13, 12); path.lineTo(9, 16); path.lineTo(13, 20);
            break;

        // ── Formatting: Strikethrough ─────────────────────────────────────
        case Id::Strikethrough:
            // An "S" bowl with the rule struck through its middle.
            path.moveTo(16.5, 7);
            path.quadTo(15, 4.5, 11.5, 4.5);
            path.quadTo(7.5, 4.5, 7.5, 8);
            path.quadTo(7.5, 10.5, 11, 11.5);
            path.moveTo(13, 12.5);
            path.quadTo(16.5, 13.8, 16.5, 16.5);
            path.quadTo(16.5, 19.5, 12, 19.5);
            path.quadTo(8.5, 19.5, 7, 17);
            path.moveTo(4, 12); path.lineTo(20, 12);
            break;

        // ── Paragraph alignment ───────────────────────────────────────────
        case Id::AlignLeft:
            path.moveTo(4, 6);  path.lineTo(20, 6);
            path.moveTo(4, 11); path.lineTo(14, 11);
            path.moveTo(4, 16); path.lineTo(18, 16);
            path.moveTo(4, 21); path.lineTo(12, 21);
            break;
        case Id::AlignCenter:
            path.moveTo(4, 6);  path.lineTo(20, 6);
            path.moveTo(7, 11); path.lineTo(17, 11);
            path.moveTo(5, 16); path.lineTo(19, 16);
            path.moveTo(8, 21); path.lineTo(16, 21);
            break;
        case Id::AlignRight:
            path.moveTo(4, 6);  path.lineTo(20, 6);
            path.moveTo(10, 11); path.lineTo(20, 11);
            path.moveTo(6, 16);  path.lineTo(20, 16);
            path.moveTo(12, 21); path.lineTo(20, 21);
            break;

        // ── Chrome ────────────────────────────────────────────────────────
        case Id::Menu:
            path.moveTo(4, 7);  path.lineTo(20, 7);
            path.moveTo(4, 12); path.lineTo(20, 12);
            path.moveTo(4, 17); path.lineTo(20, 17);
            break;
        case Id::Document:
            file_outline();
            break;
        case Id::Export:
            // A tray with a page lifting out of it — "send this somewhere".
            path.moveTo(4, 15); path.lineTo(4, 19);
            path.quadTo(4, 21, 6, 21); path.lineTo(18, 21);
            path.quadTo(20, 21, 20, 19); path.lineTo(20, 15);
            path.moveTo(12, 3);  path.lineTo(12, 15);
            path.moveTo(7.5, 7.5); path.lineTo(12, 3); path.lineTo(16.5, 7.5);
            break;
        case Id::Plus:
            path.moveTo(12, 5); path.lineTo(12, 19);
            path.moveTo(5, 12); path.lineTo(19, 12);
            break;
        case Id::Notes:
            // A page with a folded corner and two ruled lines.
            path.moveTo(15, 3); path.lineTo(6, 3);
            path.quadTo(5, 3, 5, 4); path.lineTo(5, 20);
            path.quadTo(5, 21, 6, 21); path.lineTo(18, 21);
            path.quadTo(19, 21, 19, 20); path.lineTo(19, 7);
            path.lineTo(15, 3);
            path.moveTo(15, 3); path.lineTo(15, 7); path.lineTo(19, 7);
            path.moveTo(8.5, 12); path.lineTo(15, 12);
            path.moveTo(8.5, 16); path.lineTo(13, 16);
            break;
        case Id::Comment:
            // Rounded bubble + tail, distinct from Dialogue by its three dots.
            path.addRoundedRect(QRectF(3, 4, 18, 13), 4, 4);
            path.moveTo(8, 17); path.lineTo(8, 21); path.lineTo(13, 17);
            path.moveTo(8.5, 10.5);  path.lineTo(8.6, 10.5);
            path.moveTo(11.9, 10.5); path.lineTo(12.0, 10.5);
            path.moveTo(15.3, 10.5); path.lineTo(15.4, 10.5);
            break;
        case Id::ChevronLeft:
            path.moveTo(15, 5); path.lineTo(9, 12); path.lineTo(15, 19);
            break;
        case Id::ChevronRight:
            path.moveTo(9, 5); path.lineTo(15, 12); path.lineTo(9, 19);
            break;
        case Id::Pencil:
            path.moveTo(4, 20);  path.lineTo(4.8, 16);
            path.lineTo(16, 4.8);
            path.quadTo(17.2, 3.6, 18.4, 4.8);
            path.lineTo(19.2, 5.6);
            path.quadTo(20.4, 6.8, 19.2, 8);
            path.lineTo(8, 19.2); path.lineTo(4, 20);
            break;
        // ── Window controls ───────────────────────────────────────────────
        // Deliberately drawn at a lighter weight (see the pen override below)
        // and on a tighter 10px box, matching how Windows 11 / macOS treat
        // these: present, precise, and quiet.
        case Id::WinMinimize:
            path.moveTo(7, 12); path.lineTo(17, 12);
            break;
        case Id::WinMaximize:
            path.addRoundedRect(QRectF(7, 7, 10, 10), 1.5, 1.5);
            break;
        case Id::WinRestore:
            // Front pane plus the top-right corner of the one behind it.
            path.addRoundedRect(QRectF(6, 9, 9, 9), 1.5, 1.5);
            path.moveTo(9, 6.5); path.lineTo(16.5, 6.5);
            path.quadTo(18, 6.5, 18, 8);
            path.lineTo(18, 15);
            break;
        case Id::Review:
            // A page under a magnifier — reading a draft back.
            path.moveTo(13, 3); path.lineTo(6, 3);
            path.quadTo(5, 3, 5, 4); path.lineTo(5, 20);
            path.quadTo(5, 21, 6, 21); path.lineTo(11, 21);
            path.moveTo(8.5, 8); path.lineTo(13, 8);
            path.addEllipse(QPointF(15.5, 14.5), 4.2, 4.2);
            path.moveTo(18.6, 17.6); path.lineTo(21.5, 20.5);
            break;
        }
        p.drawPath(fit_to_grid(path, optical_box(id)));
        return QIcon(pm);
    }

private:
    /// Stroke weight of the whole family, on the 24-unit design grid.
    static constexpr qreal kStroke = 1.7;

    /// The box every glyph is fitted into. Leaves room for the stroke, which
    /// straddles the path and so adds half its width on each side.
    static constexpr qreal kOpticalBox = 19.0;

    /// Minimise/maximise/close are chrome-of-the-chrome: they belong to the
    /// window, not to the document, and every desktop draws them smaller than
    /// the application's own tools. They share the family's stroke and grid —
    /// only the box is tighter, so they still read as the same set.
    static constexpr qreal kWindowBox = 13.0;

    static constexpr qreal optical_box(Id id) {
        return (id == Id::WinMinimize || id == Id::WinMaximize
             || id == Id::WinRestore || id == Id::Close) ? kWindowBox
                                                         : kOpticalBox;
    }

    /// Scales a glyph so its longest side fills the optical box, then centres
    /// it on the grid.
    ///
    /// The 46 paths here are drawn by hand, and by hand they drifted: measured,
    /// they ranged from 8 to 20 units across and four sat off centre, which is
    /// exactly what makes a row of icons look like it was assembled rather than
    /// designed. Normalising once, here, is what keeps them honest — a glyph
    /// added later cannot be the wrong size or off centre, because nothing
    /// downstream trusts the coordinates it was drawn with.
    static QPainterPath fit_to_grid(const QPainterPath& path, qreal box) {
        const QRectF ink = path.boundingRect();
        if (ink.width() <= 0.0 && ink.height() <= 0.0) return path;

        const qreal longest = std::max(ink.width(), ink.height());
        const qreal scale   = longest > 0.0 ? box / longest : 1.0;
        const qreal centre  = IconSize::Canvas / 2.0;

        QTransform fit;
        fit.translate(centre, centre);
        fit.scale(scale, scale);
        fit.translate(-ink.center().x(), -ink.center().y());
        return fit.map(path);
    }
};

} // namespace screenplay::ui
