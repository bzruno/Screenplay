#pragma once
// ui/app_header.hpp
// AppHeader — the single header row that replaces the old logo/title strip.
// Reads as a modern dashboard header, not a menu bar: sidebar toggle and
// document identity on the left, the mode switch centred, primary actions on
// the right, generous air throughout and no bottom rule.
//
// It owns only presentation. Every action it exposes is a QAction supplied by
// MainWindow, and the document name edit is handed back so MainWindow can wire
// renaming exactly as before.
//
// Owned responsibility: header layout + styling. No file/document logic.

#include "design_tokens.hpp"
#include "controls.hpp"
#include "window_frame.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QAction>

namespace screenplay::ui {

class AppHeader : public QWidget {
public:
    // One button size and two gap values for the whole header — the "grid
    // consistente" the brief asks for. Matches the writing capsule's metrics
    // so the two surfaces are visibly the same system.
    static constexpr int kButtonPx  = ControlHeight::Button;   // 40
    static constexpr int kGlyphPx   = IconSize::Chrome;
    static constexpr int kButtonGap = ButtonRhythm::WithinGroup;
    static constexpr int kGroupGap  = ButtonRhythm::BetweenGroups;

    // How long a screenplay title may get. The field is capped here AND sized
    // to fit this many characters, so a title at the limit is never clipped.
    static constexpr int kTitleMaxChars  = 70;
    static constexpr int kTitleMinWidth  = 160;

    explicit AppHeader(QWidget* parent = nullptr) : QWidget(parent) {
        // ONE flat horizontal row on the shared backdrop. No card, no border,
        // no shadow, no second row: the header is chrome and chrome should
        // disappear while writing. The only floating surface in the window is
        // the writing capsule below.
        //
        // Layout is a three-part balance —
        //     [ ☰ · file · history ]   <stretch>   TITLE   <stretch>   [ tools · Export ]
        // so the document's name lands optically centred and reads as the
        // focal point of the application.
        auto* row = new QHBoxLayout(this);
        // No vertical margin and no right margin: the window controls have to
        // reach the very top, bottom and right edge of the title bar so their
        // whole cell is clickable — a margin there would be a dead strip.
        // Everything else keeps its height from its own fixed button size.
        row->setContentsMargins(Spacing::L, 0, 0, 0);
        row->setSpacing(kGroupGap);

        // ── Left: the main menu ─────────────────────────────────────────────
        // The hamburger IS the menu bar — every File/Edit/Format/… menu hangs
        // off it, so no menu row is ever drawn.
        menu_btn_ = new SoftButton(this);
        menu_btn_->glyph(IconManager::Id::Menu)
                 ->layout_mode(SoftButton::Layout::IconOnly)
                 ->variant(SoftButton::Variant::Ghost)
                 ->height_token(kButtonPx)
                 ->icon_px(kGlyphPx);
        menu_btn_->setPopupMode(QToolButton::InstantPopup);
        menu_btn_->setToolTip("Menu");
        row->addWidget(menu_btn_);

        // ── Left: document + history commands ───────────────────────────────
        doc_actions_ = new QHBoxLayout;
        doc_actions_->setContentsMargins(0, 0, 0, 0);
        doc_actions_->setSpacing(kButtonGap);
        row->addLayout(doc_actions_);

        // ── Centre: the document's name, the focal point ────────────────────
        row->addStretch(1);
        name_edit_ = new QLineEdit(this);
        name_edit_->setFrame(false);
        name_edit_->setText("Untitled");
        name_edit_->setAlignment(Qt::AlignCenter);
        name_edit_->setMaxLength(kTitleMaxChars);
        name_edit_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        // The font is set programmatically rather than only via the stylesheet
        // so QFontMetrics below measures what is actually drawn — a stylesheet
        // font-size does not reach QWidget::font().
        name_edit_->setFont(Typography::ui_font(Typography::Size::Title,
                                                Typography::Weight::Semibold));
        name_edit_->setToolTip(
            "Document name \xe2\x80\x94 click to rename "
            "(renames the file when saved)");
        // The field grows with its content up to the 70-character cap, so a
        // short name stays compact and a long one is fully readable instead of
        // being clipped mid-title.
        QObject::connect(name_edit_, &QLineEdit::textChanged, name_edit_,
                         [this](const QString&){ fit_name_width(); });
        row->addWidget(name_edit_);
        // The saved-state line is NOT repeated here: the status strip already
        // carries it, and a second stacked line is exactly the vertical bulk
        // this header is meant to shed. set_subtitle() still updates it.
        subtitle_ = new QLabel(this);
        subtitle_->hide();
        row->addStretch(1);

        // ── Right: auxiliary tools, then the one primary action ─────────────
        aux_actions_ = new QHBoxLayout;
        aux_actions_->setContentsMargins(0, 0, 0, 0);
        aux_actions_->setSpacing(kButtonGap);
        row->addLayout(aux_actions_);

        theme_btn_ = new SoftButton(this);
        theme_btn_->glyph(IconManager::Id::Sun)
                  ->layout_mode(SoftButton::Layout::IconOnly)
                  ->variant(SoftButton::Variant::Ghost)
                  ->height_token(kButtonPx)
                  ->icon_px(kGlyphPx);
        theme_btn_->setPopupMode(QToolButton::InstantPopup);
        theme_btn_->setToolTip("Theme (Light / Dark / Chamber)");
        // Inside the auxiliary group, not beside it: added to `row` it picked
        // up the between-groups gap and sat visibly apart from the tools it
        // belongs with.
        aux_actions_->addWidget(theme_btn_);

        // ── Far right: the window's own controls ─────────────────────────────
        // The app supplies its own title bar, so minimise/maximise/close live
        // here, set slightly apart from the app's buttons so the two sets never
        // read as one row of equals.
        row->addSpacing(Spacing::M);
        win_ctl_ = new WindowControls(this);
        row->addWidget(win_ctl_);        // no alignment flag: fills the height

        restyle();
    }

    // ── Secondary action rows ────────────────────────────────────────────────
    // Header buttons are one step smaller than the writing toolbar's: they are
    // reached occasionally, and keeping them quieter is what stops the header
    // from turning back into a button bar.
    SoftButton* add_document_action(QAction* act, IconManager::Id glyph) {
        return add_to(doc_actions_, act, glyph);
    }
    SoftButton* add_aux_action(QAction* act, IconManager::Id glyph) {
        return add_to(aux_actions_, act, glyph);
    }

    // ── Accessors MainWindow wires behaviour onto ────────────────────────────
    SoftButton*       menu_button()   const { return menu_btn_; }
    SoftButton*       theme_button()  const { return theme_btn_; }
    QLineEdit*        name_edit()     const { return name_edit_; }

    // The quiet second line under the title: "Saved just now",
    // "Unsaved changes", "Read-only"… Tone selects its colour.
    enum class Tone { Neutral, Good, Warn };
    void set_subtitle(const QString& text, Tone tone = Tone::Neutral) {
        subtitle_->setText(text);
        subtitle_tone_ = tone;
        restyle_subtitle();
    }

    void set_theme_glyph(IconManager::Id id) { theme_btn_->glyph(id); }

    // Re-applies every colour-dependent style. Called on each theme change.
    void restyle() {
        const ThemePalette& p = ThemeManager::instance().palette();
        using Ty = Typography;

        name_edit_->setStyleSheet(
            QString("QLineEdit { background:transparent; border:none; padding:0;"
                    "            color:%1; font-family:'%2'; font-size:%3px;"
                    "            font-weight:600;"
                    "            selection-background-color:%4; }")
                .arg(ThemePalette::hex(p.Text), Ty::family())
                .arg(Ty::size_px(Ty::Size::Title))
                .arg(ThemePalette::hex(p.SelectionBg)));

        restyle_subtitle();
        fit_name_width();      // the font may have changed with the theme
        update();
    }

    WindowControls* window_controls() const { return win_ctl_; }

protected:
    // Flat backdrop plus ONE hairline along the bottom. The uniform window
    // colour left header, page area and footer visually merged; a single 1px
    // rule on the edge facing the page is the least ink that still says where
    // the chrome ends. No card, no shadow, nothing heavier.
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.fillRect(rect(), p.Bg0);
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawLine(0, height() - 1, width(), height() - 1);
    }

    // ── The header IS the title bar ──────────────────────────────────────────
    // Dragging empty header space moves the window and double-clicking it
    // maximises, exactly as a native caption does. Both gestures are handed to
    // the window manager (see window_frame.hpp) so snapping still works.
    //
    // `childAt()` is what keeps this from stealing clicks: any press that
    // landed on a button or the name field is left to that widget.
    // The title's ceiling depends on how much room the header has, so it has
    // to be re-derived whenever the window is resized.
    void resizeEvent(QResizeEvent* ev) override {
        QWidget::resizeEvent(ev);
        fit_name_width();
    }

    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton && is_bare_caption(ev->position().toPoint())) {
            FrameGeometry::begin_move(window());
            return;
        }
        QWidget::mousePressEvent(ev);
    }

    void mouseDoubleClickEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton && is_bare_caption(ev->position().toPoint())) {
            if (auto* w = window()) {
                if (w->isMaximized()) w->showNormal();
                else                  w->showMaximized();
                if (win_ctl_) win_ctl_->sync_state();
            }
            return;
        }
        QWidget::mouseDoubleClickEvent(ev);
    }

private:
    // Sizes the name field to its text, between a comfortable minimum and the
    // width a full 70-character title needs. Measured with the real font, and
    // clamped to what the header can actually spare so a long title shrinks
    // the field rather than shoving the buttons off the window.
    void fit_name_width() {
        if (!name_edit_) return;
        const QFontMetrics fm(name_edit_->font());
        const int pad     = Spacing::L * 2;
        const int content = fm.horizontalAdvance(name_edit_->text()) + pad;
        // 'n' is a fair stand-in for average title text; 'M' would reserve far
        // more room than any real title needs.
        const int cap     = fm.horizontalAdvance(QString(kTitleMaxChars, 'n')) + pad;
        const int room    = qMax(kTitleMinWidth, width() - kReservedForButtons);
        name_edit_->setFixedWidth(qBound(kTitleMinWidth,
                                         qMin(content, cap),
                                         qMax(kTitleMinWidth, qMin(cap, room))));
    }

    // Rough width the left and right button clusters need; the title never
    // grows into it.
    static constexpr int kReservedForButtons = 720;

    // True when `pos` is header background rather than a control — i.e. a spot
    // where a drag should move the window. The name field is excluded so
    // click-to-rename keeps working.
    bool is_bare_caption(const QPoint& pos) const {
        const QWidget* hit = childAt(pos);
        return hit == nullptr || hit == subtitle_;
    }

    // Every header button is the same box: icon-only, uniform size, uniform
    // gap. Captions were what forced the old two-row header; the name and
    // shortcut live in the tooltip instead, exactly as in the writing capsule.
    SoftButton* add_to(QHBoxLayout* row, QAction* act, IconManager::Id glyph) {
        auto* b = new SoftButton(this);
        b->glyph(glyph)
         ->layout_mode(SoftButton::Layout::IconOnly)
         ->variant(SoftButton::Variant::Ghost)
         ->height_token(kButtonPx)
         ->icon_px(kGlyphPx);
        b->setDefaultAction(act);
        b->setToolTip(act->toolTip());
        row->addWidget(b);
        return b;
    }

    void restyle_subtitle() {
        const ThemePalette& p = ThemeManager::instance().palette();
        using Ty = Typography;
        const QColor c = subtitle_tone_ == Tone::Good ? p.GoodAccent
                       : subtitle_tone_ == Tone::Warn ? p.WarnAccent
                                                      : p.TextDim;
        subtitle_->setStyleSheet(
            QString("color:%1; font-family:'%2'; font-size:%3px;")
                .arg(ThemePalette::hex(c), Ty::family())
                .arg(Ty::size_px(Ty::Size::Caption)));
    }

    QHBoxLayout*      doc_actions_ = nullptr;
    QHBoxLayout*      aux_actions_ = nullptr;
    WindowControls*   win_ctl_     = nullptr;
    SoftButton*       menu_btn_   = nullptr;
    SoftButton*       theme_btn_  = nullptr;
    QLineEdit*        name_edit_  = nullptr;
    QLabel*           subtitle_   = nullptr;
    Tone              subtitle_tone_ = Tone::Neutral;
};

} // namespace screenplay::ui
