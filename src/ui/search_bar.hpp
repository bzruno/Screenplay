#pragma once
// Find bar floating over the canvas. Owns the query and the element filter;
// the canvas owns the matches and the navigation.

#include "app_palette.hpp"
#include "design_tokens.hpp"
#include "icon_manager.hpp"
#include "typography.hpp"
#include "../config/ui_strings.hpp"
#include "../model/model.hpp"

#include <QWidget>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QToolButton>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace screenplay::ui {

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(kHeight);

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(Spacing::S + 2, Spacing::XS, Spacing::S + 2, Spacing::XS);
        row->setSpacing(Spacing::XS);

        build_query_field(row);
        build_type_filter(row);
        build_match_counter(row);

        previous_ = build_button(row, IconManager::Id::ChevronUp,
                                 "Previous match (Shift+Enter)");
        next_     = build_button(row, IconManager::Id::ChevronDown,
                                 "Next match (Enter)");
        close_    = build_button(row, IconManager::Id::Close,
                                 "Close search (Esc)");
        adjustSize();

        connect(query_, &QLineEdit::textChanged, this, &SearchBar::query_changed);
        connect(type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { emit query_changed(query_->text()); });
        connect(next_,     &QToolButton::clicked, this, &SearchBar::next_requested);
        connect(previous_, &QToolButton::clicked, this, &SearchBar::prev_requested);
        connect(close_,    &QToolButton::clicked, this, &SearchBar::close_requested);

        restyle();
    }

    QLineEdit* edit() const { return query_; }

    /// -1 for every element, otherwise a screenplay::BlockType value.
    int type_filter() const { return type_->currentData().toInt(); }

    void focus_edit() {
        query_->setFocus();
        query_->selectAll();
    }

    void set_match_info(int current, int total) {
        const bool none = (total == 0);
        counter_->setText(none ? (query_->text().isEmpty() ? "\xe2\x80\x94" : "0")
                               : QString("%1/%2").arg(current + 1).arg(total));
        counter_->setStyleSheet(
            "color:" + MD3::hx(none ? MD3::WarnAccent : MD3::GoodAccent) + ";");
    }

    void restyle() {
        previous_->setIcon(IconManager::make(IconManager::Id::ChevronUp, MD3::Text));
        next_    ->setIcon(IconManager::make(IconManager::Id::ChevronDown, MD3::Text));
        close_   ->setIcon(IconManager::make(IconManager::Id::Close, MD3::Text));
        update();
    }

signals:
    void query_changed(const QString& text);
    void next_requested();
    void prev_requested();
    void close_requested();

protected:
    /// Painted rather than styled: a QSS type selector would need the mangled
    /// `screenplay--ui--SearchBar` name once this class lives in a namespace.
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath card;
        card.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            Radius::Card, Radius::Card);
        painter.fillPath(card, MD3::Bg1);
        painter.setPen(QPen(MD3::Border, BorderWidth::Hairline));
        painter.drawPath(card);
    }

private:
    static constexpr int kHeight      = 36;
    static constexpr int kQueryWidth  = 210;   // floor, not the width
    // Everything in the field that is not the text: the stylesheet's 12px
    // padding either side, the 1px border either side, the clear button with
    // its margin, and slack.
    //
    // The slack is not padding for its own sake. Sized to the exact advance
    // width the field fitted the hint with nothing to spare, and a caret, a
    // rounding difference or a substituted glyph then clipped it again —
    // which is how this bug came back after being "fixed".
    static constexpr int kClearButton = 28;
    static constexpr int kSlack       = 12;
    static constexpr int kQueryChrome =
        Spacing::M * 2 + BorderWidth::Hairline * 2 + kClearButton + kSlack;
    static constexpr int kControl     = 26;
    static constexpr int kCounterWidth = 54;
    static constexpr int kIconPx      = 14;

    void build_query_field(QHBoxLayout* row) {
        query_ = new QLineEdit(this);
        const QString hint = config::tr_ui("Search\xe2\x80\xa6 (Enter / Shift+Enter)");
        query_->setPlaceholderText(hint);
        query_->setClearButtonEnabled(true);

        // The field is set to the font it is PAINTED in before being measured.
        //
        // The theme's stylesheet carries `font-size`, and a stylesheet font
        // size never reaches QWidget::font() — so measuring the placeholder
        // with the default font sized the box for smaller text than the box
        // would actually show, and the hint was still clipped. Setting the
        // font explicitly is what makes the measurement true.
        query_->setFont(Typography::ui_font(Typography::Size::Body));
        const int text = (int)std::ceil(
            QFontMetricsF(query_->font()).horizontalAdvance(hint));
        query_->setFixedWidth(std::max(kQueryWidth, text + kQueryChrome));
        row->addWidget(query_);
    }

    /// An empty query plus a type lists every block of that type.
    void build_type_filter(QHBoxLayout* row) {
        using BT = screenplay::BlockType;
        type_ = new QComboBox(this);
        type_->setToolTip(config::tr_ui("Search only inside this element type"));
        type_->addItem(config::tr_ui("All"), -1);
        type_->addItem(config::tr_ui("Scene"),         (int)BT::SceneHeading);
        type_->addItem(config::tr_ui("Action"),        (int)BT::Action);
        type_->addItem(config::tr_ui("Character"),     (int)BT::Character);
        type_->addItem(config::tr_ui("Dialogue"),      (int)BT::Dialogue);
        type_->addItem(config::tr_ui("Paren."),        (int)BT::Parenthetical);
        type_->addItem(config::tr_ui("Transition"),    (int)BT::Transition);
        type_->setFixedHeight(kControl);
        row->addWidget(type_);
    }

    void build_match_counter(QHBoxLayout* row) {
        counter_ = new QLabel("\xe2\x80\x94", this);
        counter_->setFixedWidth(kCounterWidth);
        counter_->setAlignment(Qt::AlignCenter);
        row->addWidget(counter_);
    }

    QToolButton* build_button(QHBoxLayout* row, IconManager::Id icon,
                              const char* tooltip) {
        auto* button = new QToolButton(this);
        button->setIcon(IconManager::make(icon, MD3::Text));
        button->setIconSize(QSize(kIconPx, kIconPx));
        button->setFixedSize(kControl, kControl);
        button->setToolTip(config::tr_ui(tooltip));
        button->setFocusPolicy(Qt::NoFocus);   // typing stays in the query field
        row->addWidget(button);
        return button;
    }

    QLineEdit*   query_    = nullptr;
    QComboBox*   type_     = nullptr;
    QLabel*      counter_  = nullptr;
    QToolButton* previous_ = nullptr;
    QToolButton* next_     = nullptr;
    QToolButton* close_    = nullptr;
};

} // namespace screenplay::ui
