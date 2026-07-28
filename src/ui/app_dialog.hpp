#pragma once
// ui/app_dialog.hpp
// AppDialog — the frame every non-trivial dialog in the app wears.
//
// ConfirmDialog (confirm_dialog.hpp) is the tiny two-or-three-button question
// box; this is its bigger sibling for dialogs that own real content: a title
// row with a close button, a content area the caller fills, and an optional
// button row along the bottom.
//
// Same two structural decisions as everywhere else: frameless (the app draws
// its own title bar, so a native caption would reintroduce the system-coloured
// strip) and square (a top-level OS window's real corners cannot be rounded by
// a border-radius — see the note in theme_manager.hpp).
//
// Owned responsibility: dialog chrome. The content is entirely the caller's.

#include "design_tokens.hpp"
#include "controls.hpp"
#include "window_frame.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>

namespace screenplay::ui {

class AppDialog : public QDialog {
public:
    explicit AppDialog(QWidget* parent, const QString& title)
        : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(Spacing::XL, Spacing::L, Spacing::XL, Spacing::XL);
        col->setSpacing(Spacing::L);

        // ── Title row: doubles as the drag handle ────────────────────────────
        auto* bar = new QHBoxLayout;
        bar->setContentsMargins(0, 0, 0, 0);
        bar->setSpacing(Spacing::S);

        title_ = new QLabel(title, this);
        title_->setFont(Typography::ui_font(Typography::Size::Title,
                                            Typography::Weight::Semibold));
        bar->addWidget(title_);
        bar->addStretch(1);

        close_btn_ = new SoftButton(this);
        close_btn_->glyph(IconManager::Id::Close)
                  ->layout_mode(SoftButton::Layout::IconOnly)
                  ->variant(SoftButton::Variant::Danger)
                  ->radius(Radius::Chip)
                  ->height_token(ControlHeight::Compact)
                  ->icon_px(IconSize::Chrome);
        QObject::connect(close_btn_, &QToolButton::clicked, this, [this]{ reject(); });
        bar->addWidget(close_btn_);
        col->addLayout(bar);

        // ── Content ──────────────────────────────────────────────────────────
        content_ = new QVBoxLayout;
        content_->setContentsMargins(0, 0, 0, 0);
        content_->setSpacing(Spacing::M);
        col->addLayout(content_, 1);

        // ── Optional button row ──────────────────────────────────────────────
        buttons_ = new QHBoxLayout;
        buttons_->setContentsMargins(0, 0, 0, 0);
        buttons_->setSpacing(Spacing::S);
        buttons_->addStretch(1);
        col->addLayout(buttons_);

        restyle();
    }

    // Where callers put their widgets.
    QVBoxLayout* content() const { return content_; }

    // Adds a bottom-row button. `variant` picks its weight; the returned
    // button is the caller's to connect.
    SoftButton* add_button(const QString& text, SoftButton::Variant v) {
        auto* b = new SoftButton(this);
        b->label(text)
         ->layout_mode(SoftButton::Layout::TextOnly)
         ->variant(v)
         ->height_token(ControlHeight::Button)
         ->text_size(Typography::Size::Body);
        buttons_->addWidget(b);
        return b;
    }

    void restyle() {
        const ThemePalette& p = ThemeManager::instance().palette();
        title_->setStyleSheet("color:" + ThemePalette::hex(p.Text) + ";");
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.fillRect(rect(), p.Card);
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

    // Dragging anywhere that isn't a control moves the dialog, since there is
    // no OS caption to grab.
    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton &&
                childAt(ev->position().toPoint()) == nullptr) {
            FrameGeometry::begin_move(this);
            return;
        }
        QDialog::mousePressEvent(ev);
    }

    void keyPressEvent(QKeyEvent* ev) override {
        if (ev->key() == Qt::Key_Escape) { reject(); return; }
        QDialog::keyPressEvent(ev);
    }

private:
    QLabel*      title_     = nullptr;
    SoftButton*  close_btn_ = nullptr;
    QVBoxLayout* content_   = nullptr;
    QHBoxLayout* buttons_   = nullptr;
};

} // namespace screenplay::ui
