#pragma once
// ui/floating_toolbar.hpp
// FloatingToolbar — the content-width card of grouped tools that floats,
// centred, beneath the header. Replaces the old full-width Qt toolbar strip.
//
// Two classes:
//   ToolbarCard  — the card surface itself (radius, hairline, soft shadow).
//   FloatingToolbar — a band widget that keeps exactly one card horizontally
//                     centred no matter how the window is resized.
//
// Groups are separated by a ToolDivider — a single low-alpha hairline with
// generous air on both sides. (An earlier revision used space alone; grouping
// that many controls proved to need the extra reading cue.) add_group() opens
// a new group; add() appends to the current one.
//
// Owned responsibility: assembling and laying out toolbar buttons. What each
// button DOES is the caller's business — every button is driven by a QAction
// the caller already owns.

#include "design_tokens.hpp"
#include "controls.hpp"
#include "elevation.hpp"
#include "theme_manager.hpp"

#include <QWidget>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QAction>
#include <vector>

namespace screenplay::ui {

// ─────────────────────────────────────────────────────────────────────────────
// ToolbarCard — a floating surface holding one row of grouped controls.
// ─────────────────────────────────────────────────────────────────────────────
class ToolbarCard : public QWidget {
public:
    explicit ToolbarCard(QWidget* parent = nullptr) : QWidget(parent) {
        row_ = new QHBoxLayout(this);
        // A capsule, not a window: tight vertical padding so the card hugs its
        // buttons, slightly more horizontally so the end buttons aren't jammed
        // against the rounded edge.
        row_->setContentsMargins(Spacing::S, Spacing::XS, Spacing::S, Spacing::XS);
        // A tight gap WITHIN a group and a wide one BETWEEN groups (see
        // add_group) produces the "group → space → group" rhythm rather than
        // one undifferentiated run of icons. Both come from ButtonRhythm, the
        // same source the header uses, so the two rows are spaced alike.
        row_->setSpacing(ButtonRhythm::WithinGroup);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Opens a new visual group, separated from the previous one by equal air
    // either side of one hairline. The first call adds nothing, so callers can
    // start every group with add_group() and not special-case the first.
    void add_group() {
        if (row_->count() == 0) return;
        const int air = (ButtonRhythm::BetweenGroups
                       - ButtonRhythm::WithinGroup) / 2;
        row_->addSpacing(air);
        row_->addWidget(new ToolDivider(this), 0, Qt::AlignVCenter);
        row_->addSpacing(air);
    }

    // A button bound to an existing QAction: icon state, checked state,
    // enabled state and tooltip all keep flowing from the action.
    //
    // Every button in this card is identical — same box, same glyph size, same
    // radius, icon-only. The label belongs in the tooltip: this is a
    // keyboard-first editor, and a row of captions turns a capsule back into a
    // toolbar. Callers pass no label at all.
    SoftButton* add(QAction* act, IconManager::Id glyph) {
        auto* b = new SoftButton(this);
        b->glyph(glyph)
         ->layout_mode(SoftButton::Layout::IconOnly)
         ->height_token(kButtonPx)
         ->icon_px(kGlyphPx)
         ->radius(Radius::Button);
        b->setDefaultAction(act);        // click routing + checked/enabled sync
        b->setToolTip(act->toolTip());
        row_->addWidget(b);
        buttons_.push_back(b);
        return b;
    }

    static constexpr int kButtonPx = ControlHeight::Button;   // 40
    static constexpr int kGlyphPx  = IconSize::Chrome;

    void refresh_elevation() {
        ElevationFx::apply(this, Elevation::Card,
                           ThemeManager::instance().palette());
        update();
    }

    const std::vector<SoftButton*>& buttons() const { return buttons_; }

protected:
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        // Radius is half the height: a true capsule, whatever the card's
        // content ends up being.
        const qreal rad = r.height() * 0.5;
        QPainterPath path;
        path.addRoundedRect(r, rad, rad);
        g.fillPath(path, p.Card);
        // The border is deliberately fainter than a full-strength hairline:
        // at this radius the shadow already separates the card, and a crisp
        // outline would read as a framed widget rather than a floating panel.
        QColor edge = p.Border;
        edge.setAlpha(p.isDarkChrome() ? 150 : 190);
        g.setPen(QPen(edge, BorderWidth::Hairline));
        g.drawPath(path);
    }

private:
    QHBoxLayout*             row_ = nullptr;
    std::vector<SoftButton*> buttons_;
};

// ─────────────────────────────────────────────────────────────────────────────
// FloatingToolbar — the band the card floats in.
// ─────────────────────────────────────────────────────────────────────────────
class FloatingToolbar : public QWidget {
public:
    explicit FloatingToolbar(QWidget* parent = nullptr) : QWidget(parent) {
        card_ = new ToolbarCard(this);

        auto* band = new QHBoxLayout(this);
        // Real air above and below the capsule so the reading order is
        // header → mode → toolbar → page with gaps, not a stack of touching
        // bars. The shadow also needs room or the parent clips it.
        // Enough room for the shadow, no more — every extra pixel here is a
        // pixel the page loses.
        const int m = ElevationFx::margin_for(Elevation::Card);
        band->setContentsMargins(Spacing::XL, Spacing::M, Spacing::XL, m);
        band->addStretch(1);
        band->addWidget(card_);          // centred: stretch on both sides
        band->addStretch(1);

        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    ToolbarCard* card() const { return card_; }

    void refresh_theme() { card_->refresh_elevation(); update(); }

private:
    ToolbarCard* card_ = nullptr;
};

} // namespace screenplay::ui
