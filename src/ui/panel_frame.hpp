#pragma once
// A side panel's frame: one outlined card with its name centred at the top.
//
// QDockWidget draws its title OUTSIDE the widget it hosts, so an outline on the
// panel body always excluded the title — the name floated above an unbordered
// area and the panel had no edge to read against the page. Hosting the title
// inside the frame is the only way to get one card containing both, and it also
// puts the title's alignment and weight under our control rather than the
// platform style's.

#include "app_palette.hpp"
#include "design_tokens.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"

#include <QDockWidget>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWidget>

namespace screenplay::ui {

class PanelFrame : public QWidget {
public:
    PanelFrame(const QString& title, QWidget* content, QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* column = new QVBoxLayout(this);
        column->setContentsMargins(kEdge, kEdge, kEdge, kEdge);
        column->setSpacing(0);

        title_ = new QLabel(title, this);
        title_->setAlignment(Qt::AlignCenter);
        title_->setFixedHeight(kTitleH);
        column->addWidget(title_);

        content->setParent(this);
        column->addWidget(content, 1);

        restyle();
    }

    void set_title(const QString& t) { title_->setText(t); }

    void restyle() {
        const ThemePalette& p = ThemeManager::instance().palette();
        title_->setFont(Typography::ui_font(Typography::Size::Label,
                                            Typography::Weight::Semibold));
        title_->setStyleSheet("color:" + ThemePalette::hex(p.TextDim) + ";"
                              "letter-spacing:1px;");
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing);

        QPainterPath card;
        card.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            Radius::Card, Radius::Card);
        g.fillPath(card, p.Bg1);
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawPath(card);

        // A hairline under the title separates the name from the content
        // without a second box around either.
        const float y = kEdge + kTitleH + 0.5f;
        g.setPen(QPen(p.Divider, BorderWidth::Hairline));
        g.drawLine(QPointF(kEdge + Spacing::S, y),
                   QPointF(width() - kEdge - Spacing::S, y));
    }

private:
    static constexpr int kEdge   = 1;    // the outline itself
    static constexpr int kTitleH = 34;
    QLabel* title_ = nullptr;
};

/// Wraps `content` in a titled frame and hands back a dock with no native
/// title bar, so the frame's own centred title is the only one shown.
inline QDockWidget* make_panel_dock(QMainWindow* window, const QString& title,
                                    QWidget* content, PanelFrame** out_frame) {
    auto* frame = new PanelFrame(title, content);
    if (out_frame) *out_frame = frame;

    auto* dock = new QDockWidget(title, window);
    dock->setWidget(frame);
    // An empty title bar removes Qt's own caption. The panel is shown and
    // hidden from the View menu and the header, so nothing is lost — and the
    // splitter that resizes it is a separate handle, still there.
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    return dock;
}

} // namespace screenplay::ui
