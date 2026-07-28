#pragma once
// Transient confirmation banner. One is visible at a time; it fades itself out.

#include "app_palette.hpp"
#include "design_tokens.hpp"
#include "typography.hpp"

#include <QWidget>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QFontMetrics>
#include <QTimer>
#include <algorithm>

namespace screenplay::ui {

class Toast : public QWidget {
    Q_OBJECT
public:
    enum class Kind { Info, Success, Error };

    static void show_toast(QWidget* window, const QString& text,
                           Kind kind = Kind::Success) {
        static QPointer<Toast> active;
        if (active) active->deleteLater();
        active = new Toast(window, text, kind);
        active->popup();
    }

private:
    static constexpr int kHeight     = 36;
    static constexpr int kBottomGap  = 52;
    static constexpr int kRiseFrom   = 12;
    static constexpr int kFadeMs     = 180;
    static constexpr int kHoldMs     = 2400;
    static constexpr int kDismissMs  = 200;

    Toast(QWidget* parent, const QString& text, Kind kind)
        : QWidget(parent), text_(text), kind_(kind) {
        setAttribute(Qt::WA_TransparentForMouseEvents);

        QFont font(Typography::family());
        font.setPixelSize(Typography::size_px(Typography::Size::Body));
        setFont(font);

        const int wanted = QFontMetrics(font).horizontalAdvance(text_) + 52;
        setFixedSize(std::min(wanted, parent->width() - 40), kHeight);

        opacity_ = new QGraphicsOpacityEffect(this);
        opacity_->setOpacity(0);
        setGraphicsEffect(opacity_);
    }

    void popup() {
        const QWidget* host = parentWidget();
        const int x     = (host->width() - width()) / 2;
        const int y_end = host->height() - height() - kBottomGap;

        move(x, y_end + kRiseFrom);
        show();
        raise();

        animate(opacity_, "opacity", 0.0, 1.0, kFadeMs);
        animate(this, "pos", QPoint(x, y_end + kRiseFrom), QPoint(x, y_end), kFadeMs);

        QTimer::singleShot(kHoldMs, this, [this] { dismiss(); });
    }

    void dismiss() {
        auto* out = animate(opacity_, "opacity", opacity_->opacity(), 0.0, kDismissMs);
        connect(out, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
    }

    QPropertyAnimation* animate(QObject* target, const char* property,
                                const QVariant& from, const QVariant& to, int ms) {
        auto* anim = new QPropertyAnimation(target, property, this);
        anim->setDuration(ms);
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        return anim;
    }

    QColor accent() const {
        switch (kind_) {
        case Kind::Success: return MD3::GoodAccent;
        case Kind::Error:   return MD3::WarnAccent;
        case Kind::Info:    break;
        }
        return MD3::Primary;
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QColor body(MD3::Bg1);
        body.setAlpha(246);
        QPainterPath shape;
        shape.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                             Radius::Chip, Radius::Chip);
        painter.fillPath(shape, body);
        painter.setPen(QPen(MD3::Border, BorderWidth::Hairline));
        painter.drawPath(shape);

        painter.setPen(Qt::NoPen);
        painter.setBrush(accent());
        painter.drawEllipse(QPointF(16, height() / 2.0), 4, 4);

        painter.setPen(MD3::Text);
        painter.drawText(rect().adjusted(30, 0, -12, 0),
                         Qt::AlignVCenter | Qt::AlignLeft, text_);
    }

    QString                 text_;
    Kind                    kind_;
    QGraphicsOpacityEffect* opacity_ = nullptr;
};

} // namespace screenplay::ui
