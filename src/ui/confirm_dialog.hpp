#pragma once
// ui/confirm_dialog.hpp
// ConfirmDialog — the app's own modal question box.
//
// QMessageBox is deliberately NOT used: it forces a platform icon (the blue
// "?" balloon), platform button ordering and platform metrics, none of which
// can be brought in line with the rest of this chrome. This is a plain QDialog
// that states the question in words and offers the same SoftButtons found
// everywhere else.
//
// Frameless and square, to match two decisions already made elsewhere: the
// main window supplies its own title bar (so a native caption here would
// reintroduce the system-coloured strip), and any surface that is its own
// top-level OS window stays square (a border-radius rounds only what is
// painted, leaving real corner pixels behind — see theme_manager.hpp).
//
// Owned responsibility: presenting a question and reporting which button was
// chosen. It performs no action itself.

#include "design_tokens.hpp"
#include "controls.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QKeyEvent>

namespace screenplay::ui {

class ConfirmDialog : public QDialog {
public:
    // Deliberately not QDialog::Accepted/Rejected: a three-way question has no
    // meaningful "accepted", and naming the outcomes after their ROLE keeps
    // call sites readable.
    enum Choice { Cancelled = 0, Confirmed = 1, Alternate = 2 };

    struct Spec {
        QString title;
        QString body;
        QString confirm_text;     // primary action, e.g. "Save"
        QString alternate_text;   // secondary action, e.g. "Discard" ("" hides it)
        QString cancel_text = "Cancel";
        bool    alternate_is_destructive = false;
    };

    ConfirmDialog(QWidget* parent, Spec spec) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setModal(true);
        setFixedWidth(kWidth);

        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(Spacing::XL, Spacing::XL, Spacing::XL, Spacing::XL);
        col->setSpacing(Spacing::S);

        auto* title = new QLabel(spec.title, this);
        title->setFont(Typography::ui_font(Typography::Size::Title,
                                           Typography::Weight::Semibold));
        col->addWidget(title);

        auto* body = new QLabel(spec.body, this);
        body->setWordWrap(true);
        body->setFont(Typography::ui_font(Typography::Size::Body));
        col->addWidget(body);

        col->addSpacing(Spacing::M);

        // Buttons right-aligned, primary last — the direction the eye travels
        // to commit, and the same order the rest of the app uses.
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(Spacing::S);
        row->addStretch(1);

        if (!spec.cancel_text.isEmpty())
            row->addWidget(make_button(spec.cancel_text,
                                       SoftButton::Variant::Ghost, Cancelled));
        if (!spec.alternate_text.isEmpty())
            row->addWidget(make_button(spec.alternate_text,
                                       spec.alternate_is_destructive
                                           ? SoftButton::Variant::Danger
                                           : SoftButton::Variant::Soft,
                                       Alternate));
        row->addWidget(make_button(spec.confirm_text,
                                   SoftButton::Variant::Primary, Confirmed));
        col->addLayout(row);

        const ThemePalette& p = ThemeManager::instance().palette();
        title->setStyleSheet("color:" + ThemePalette::hex(p.Text) + ";");
        body->setStyleSheet("color:" + ThemePalette::hex(p.TextDim) + ";");
    }

    // Convenience: run it and report the choice.
    static Choice ask(QWidget* parent, Spec spec) {
        ConfirmDialog dlg(parent, std::move(spec));
        return static_cast<Choice>(dlg.exec());
    }

protected:
    // A plain card: fill + hairline. Square corners, for the reason in the
    // header comment.
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.fillRect(rect(), p.Card);
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

    // Escape cancels. QDialog's default reject() would return 0 anyway, but
    // being explicit keeps Cancelled's value from being an accident.
    void keyPressEvent(QKeyEvent* ev) override {
        if (ev->key() == Qt::Key_Escape) { done(Cancelled); return; }
        QDialog::keyPressEvent(ev);
    }

private:
    static constexpr int kWidth = 380;

    SoftButton* make_button(const QString& text, SoftButton::Variant v,
                            int result) {
        auto* b = new SoftButton(this);
        b->label(text)
         ->layout_mode(SoftButton::Layout::TextOnly)
         ->variant(v)
         ->height_token(ControlHeight::Button)
         ->text_size(Typography::Size::Body);
        QObject::connect(b, &QToolButton::clicked, this,
                         [this, result]{ done(result); });
        return b;
    }
};

} // namespace screenplay::ui
