#pragma once
// SmartType suggestion list, anchored under the caret. Shows a scrolling
// window over the suggestions; the controller owns which one is selected.

#include "app_palette.hpp"
#include "design_tokens.hpp"
#include "screenplay_font.hpp"
#include "typography.hpp"
#include "../model/model.hpp"

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <algorithm>
#include <string>
#include <vector>

namespace screenplay::ui {

class AutocompletePopup : public QWidget {
    Q_OBJECT
public:
    explicit AutocompletePopup(QWidget* parent)
        : QWidget(parent, Qt::SubWindow | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        hide();
    }

    /// `anchor` is in parent coordinates and already points at the caret's
    /// bottom-left.
    void show_suggestions(const std::vector<std::string>& suggestions,
                          int selected, QPoint anchor) {
        if (suggestions.empty()) { hide(); return; }

        if (suggestions != suggestions_) {
            suggestions_   = suggestions;
            scroll_first_  = 0;
            measure();
        }
        selected_ = selected;

        clamp_scroll();
        setGeometry(anchor.x(), anchor.y(), width_, height_);
        show();
        raise();
        update();
    }

    void update_selection(int selected) {
        if (selected_ == selected) return;
        selected_ = selected;
        scroll_into_view();
        update();
    }

    /// Element being completed — tints the selected row with its accent.
    void set_block_type(screenplay::BlockType type) {
        if (block_type_ == type) return;
        block_type_ = type;
        update();
    }

    void hide_popup()      { hide(); }
    bool is_visible() const { return isVisible(); }

signals:
    void item_clicked(int index);

protected:
    void mousePressEvent(QMouseEvent* event) override {
        const int row   = (event->pos().y() - kPadV - kHeaderHeight) / kItemHeight;
        const int index = row + scroll_first_;
        if (index >= 0 && index < (int)suggestions_.size())
            emit item_clicked(index);
    }

    void wheelEvent(QWheelEvent* event) override {
        if (suggestions_.empty()) return;
        scroll_first_ += (event->angleDelta().y() > 0) ? -1 : 1;
        clamp_scroll();
        update();
        event->accept();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paint_card(painter);
        paint_header(painter);
        paint_items(painter);
        if (needs_scrollbar()) paint_scrollbar(painter);
    }

private:
    static constexpr int kHeaderHeight = 18;
    static constexpr int kMaxVisible   = 5;
    static constexpr int kItemHeight   = 22;
    static constexpr int kPadV         = 6;
    static constexpr int kPadH         = 12;
    static constexpr int kMaxWidth     = 300;
    static constexpr int kTextSize     = 13;
    static constexpr int kScrollbarW   = 3;

    bool needs_scrollbar() const {
        return (int)suggestions_.size() > kMaxVisible;
    }

    QFont suggestion_font() const {
        QFont font(ScreenplayFont::family());
        font.setPixelSize(kTextSize);
        font.setStyleHint(QFont::TypeWriter);
        ScreenplayFont::apply_render_quality(font);
        return font;
    }

    void measure() {
        const QFontMetrics metrics(suggestion_font());
        int widest = 0;
        for (const auto& suggestion : suggestions_)
            widest = std::max(widest,
                metrics.horizontalAdvance(QString::fromStdString(suggestion)));

        const int gutter = needs_scrollbar() ? kScrollbarW + 3 : 0;
        width_  = std::min(widest + kPadH * 2 + 10 + gutter, kMaxWidth);
        height_ = std::min((int)suggestions_.size(), kMaxVisible) * kItemHeight
                + kPadV * 2 + kHeaderHeight;
    }

    void clamp_scroll() {
        const int last_start = std::max(0, (int)suggestions_.size() - kMaxVisible);
        scroll_first_ = std::clamp(scroll_first_, 0, last_start);
    }

    void scroll_into_view() {
        if (selected_ < 0) return;
        if (selected_ < scroll_first_)
            scroll_first_ = selected_;
        else if (selected_ >= scroll_first_ + kMaxVisible)
            scroll_first_ = selected_ - kMaxVisible + 1;
        clamp_scroll();
    }

    void paint_card(QPainter& painter) const {
        QColor body(MD3::Bg1);
        body.setAlpha(245);
        QPainterPath card;
        card.addRoundedRect(rect(), Radius::Chip, Radius::Chip);
        painter.fillPath(card, body);
        painter.setPen(QPen(MD3::Outline, BorderWidth::Hairline));
        painter.drawPath(card);
    }

    void paint_header(QPainter& painter) const {
        QColor bar(MD3::Bg0);
        bar.setAlpha(245);

        QPainterPath rounded;
        rounded.addRoundedRect(QRectF(0, 0, width(), kHeaderHeight + 8),
                               Radius::Chip, Radius::Chip);
        painter.fillPath(rounded, bar);
        painter.fillRect(QRectF(0, kHeaderHeight / 2.0,
                                width(), kHeaderHeight / 2.0 + 1), bar);

        QFont label(Typography::family());
        label.setPixelSize(Typography::size_px(Typography::Size::Caption));
        label.setWeight(static_cast<QFont::Weight>(
            Typography::weight(Typography::Weight::Semibold)));
        ScreenplayFont::apply_render_quality(label);

        painter.setFont(label);
        painter.setPen(MD3::TextDim);
        painter.drawText(QRect(8, 0, width() - 16, kHeaderHeight),
                         Qt::AlignVCenter | Qt::AlignLeft, "SmartType");
        painter.setPen(QPen(MD3::Border, BorderWidth::Hairline));
        painter.drawLine(QPointF(0, kHeaderHeight), QPointF(width(), kHeaderHeight));
    }

    void paint_items(QPainter& painter) const {
        painter.setFont(suggestion_font());
        const int right_inset = needs_scrollbar() ? -12 : -6;
        const int last = std::min(scroll_first_ + kMaxVisible,
                                  (int)suggestions_.size());

        for (int index = scroll_first_; index < last; ++index) {
            const int row = index - scroll_first_;
            const QRect line(0, kPadV + kHeaderHeight + row * kItemHeight,
                             width(), kItemHeight);

            if (index == selected_) {
                const QColor accent = block_color(block_type_);
                QPainterPath highlight;
                highlight.addRoundedRect(line.adjusted(3, 1, -3, -1), 4, 4);
                painter.fillPath(highlight, accent);
                painter.setPen(contrast_text(accent));
            } else {
                painter.setPen(MD3::OnSurface);
            }
            painter.drawText(line.adjusted(10, 0, right_inset, 0),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             QString::fromStdString(suggestions_[(size_t)index]));
        }
    }

    void paint_scrollbar(QPainter& painter) const {
        const int   total  = (int)suggestions_.size();
        const int   x      = width() - 5;
        const int   top    = kHeaderHeight + kPadV;
        const int   track  = kMaxVisible * kItemHeight;
        const float thumb  = (float)kMaxVisible / total * track;
        const float offset = top + (float)scroll_first_ / total * track;

        QColor track_colour(MD3::Border);
        track_colour.setAlpha(120);
        painter.fillRect(QRectF(x, top, kScrollbarW, track), track_colour);

        QPainterPath handle;
        handle.addRoundedRect(QRectF(x, offset, kScrollbarW, thumb), 2, 2);
        painter.fillPath(handle, MD3::TextDim);
    }

    std::vector<std::string> suggestions_;
    screenplay::BlockType    block_type_   = screenplay::BlockType::Action;
    int selected_     = -1;
    int scroll_first_ = 0;
    int width_        = 0;
    int height_       = 0;
};

} // namespace screenplay::ui
