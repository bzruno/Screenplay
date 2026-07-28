#pragma once
// Picks the revision pass the writer is working in.
//
// Shown as the real paper stock rather than a list of words: on a set the pass
// IS its colour, and a swatch is recognised faster than "Goldenrod" is read.

#include "app_dialog.hpp"
#include "design_tokens.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"
#include "../config/ui_strings.hpp"
#include "../model/revision.hpp"

#include <QEnterEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <vector>

namespace screenplay::ui {

/// One colour of paper: swatch, name, and a tick when it is the active pass.
class RevisionSwatch : public QWidget {
public:
    RevisionSwatch(QWidget* parent, screenplay::Revision revision,
                   std::function<void()> on_click)
        : QWidget(parent), revision_(revision), on_click_(std::move(on_click)) {
        setFixedHeight(kHeight);
        setCursor(Qt::PointingHandCursor);
        setToolTip(config::tr_ui(screenplay::revision_name(revision)));
    }

    void set_selected(bool on) { selected_ = on; update(); }
    screenplay::Revision revision() const { return revision_; }

protected:
    void enterEvent(QEnterEvent*) override { hovered_ = true;  update(); }
    void leaveEvent(QEvent*)      override { hovered_ = false; update(); }

    void mouseReleaseEvent(QMouseEvent*) override { if (on_click_) on_click_(); }

    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing);

        QPainterPath row;
        row.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                           Radius::Chip, Radius::Chip);
        if (selected_)     g.fillPath(row, p.SelectionBg);
        else if (hovered_) g.fillPath(row, p.HoverBg);

        // The swatch is the paper, so it always carries a hairline: pale
        // stocks like Buff would otherwise dissolve into a light theme.
        const QRectF chip(Spacing::M, (height() - kChip) * .5, kChip, kChip);
        QPainterPath swatch;
        swatch.addRoundedRect(chip, Radius::Chip * .6, Radius::Chip * .6);
        g.fillPath(swatch, QColor::fromRgb(screenplay::revision_rgb(revision_)));
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawPath(swatch);

        g.setPen(selected_ ? p.Text : p.TextDim);
        g.setFont(Typography::ui_font(Typography::Size::Body,
                                      selected_ ? Typography::Weight::Semibold
                                                : Typography::Weight::Regular));
        g.drawText(QRectF(chip.right() + Spacing::M, 0,
                          width() - chip.right() - Spacing::M * 2, height()),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   config::tr_ui(screenplay::revision_name(revision_)));

        if (selected_) {
            g.setPen(QPen(p.Primary, 2));
            const float cx = width() - Spacing::L - 10.f;
            const float cy = height() * .5f;
            g.drawLine(QPointF(cx - 5, cy), QPointF(cx - 1, cy + 4));
            g.drawLine(QPointF(cx - 1, cy + 4), QPointF(cx + 6, cy - 4));
        }
    }

private:
    static constexpr int kHeight = 38;
    static constexpr int kChip   = 20;

    screenplay::Revision  revision_;
    std::function<void()> on_click_;
    bool selected_ = false;
    bool hovered_  = false;
};

class RevisionDialog : public AppDialog {
public:
    RevisionDialog(QWidget* parent, screenplay::Revision current)
        : AppDialog(parent, config::tr_ui("Revision pass")), chosen_(current) {
        setFixedWidth(kWidth);

        auto* hint = new QLabel(
            config::tr_ui("Each pass is issued on its own colour of paper. "
                          "While a pass is open, every edit marks its element "
                          "with an asterisk in the margin."), this);
        hint->setWordWrap(true);
        hint->setFont(Typography::ui_font(Typography::Size::Caption));
        hint->setStyleSheet(
            "color:" + ThemePalette::hex(
                ThemeManager::instance().palette().TextDim) + ";");
        content()->addWidget(hint);

        for (int i = 0; i < kRevisionCount; ++i) {
            const auto revision = (screenplay::Revision)i;
            auto* swatch = new RevisionSwatch(this, revision, [this, revision] {
                select(revision);
            });
            swatch->set_selected(revision == current);
            swatches_.push_back(swatch);
            content()->addWidget(swatch);
        }

        auto* cancel = add_button(config::tr_ui("Cancel"),
                                  SoftButton::Variant::Ghost);
        QObject::connect(cancel, &QToolButton::clicked, this, [this]{ reject(); });
        auto* apply = add_button(config::tr_ui("Apply"),
                                 SoftButton::Variant::Primary);
        QObject::connect(apply, &QToolButton::clicked, this, [this]{ accept(); });
    }

    screenplay::Revision chosen() const { return chosen_; }

private:
    static constexpr int kWidth = 360;

    void select(screenplay::Revision revision) {
        chosen_ = revision;
        for (auto* s : swatches_) s->set_selected(s->revision() == revision);
    }

    screenplay::Revision         chosen_;
    std::vector<RevisionSwatch*> swatches_;
};

} // namespace screenplay::ui
