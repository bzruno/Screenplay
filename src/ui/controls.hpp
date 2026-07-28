#pragma once
// ui/controls.hpp
// The app's button primitives. Qt Widgets' QSS has no transition property —
// :hover/:pressed rules snap instantly — so the redesign brief's "hover fades
// in 120ms, press in 100ms" cannot be expressed in a stylesheet at all. These
// controls custom-paint instead, interpolating their wash from real
// animations, and take every colour/radius/height from the design tokens.
//
// Deliberately NO Q_OBJECT: only src/*.cpp files are listed in CMake's
// SOURCES, so AUTOMOC does not reliably process headers that aren't compiled
// as their own translation unit. These classes therefore add no new
// signals/slots — they inherit everything they need from QToolButton (which
// already gives us setDefaultAction, checkable state and clicked/toggled).
//
// Owned responsibility: button painting + interaction feedback. Layout of
// button GROUPS belongs to floating_toolbar.hpp / app_header.hpp.

#include "design_tokens.hpp"
#include "theme_palette.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"
#include "icon_manager.hpp"

#include <QToolButton>
#include <QVariantAnimation>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QEnterEvent>
#include <QFontMetrics>
#include <optional>

namespace screenplay::ui {

// A 0→1 value that eases between states over a token-defined duration.
// Wraps QVariantAnimation so call sites never touch the animation object.
class AnimatedFloat {
public:
    AnimatedFloat(QWidget* owner, int duration_ms)
        : anim_(new QVariantAnimation(owner)) {
        anim_->setDuration(duration_ms);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        anim_->setStartValue(0.0);
        anim_->setEndValue(0.0);
        QObject::connect(anim_, &QVariantAnimation::valueChanged, owner,
                         [owner](const QVariant&) { owner->update(); });
    }

    void target(double to) {
        if (qFuzzyCompare(to + 1.0, target_ + 1.0)) return;
        target_ = to;
        anim_->stop();
        anim_->setStartValue(value());
        anim_->setEndValue(to);
        anim_->start();
    }

    double value() const {
        const QVariant v = anim_->currentValue();
        return v.isValid() ? v.toDouble() : target_;
    }

private:
    QVariantAnimation* anim_;
    double             target_ = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────
// ToolDivider — the hairline between button groups.
//
// A class rather than a styled QFrame per call site so that "every separator
// has exactly the same height and opacity" is true BY CONSTRUCTION and cannot
// be broken by someone tuning one of them later. QFrame::VLine was the old
// approach and it drew a 2px etched bevel (a Win32 tell); this draws a single
// low-alpha 1px line and nothing else.
// ─────────────────────────────────────────────────────────────────────────────
class ToolDivider : public QWidget {
public:
    // The line is deliberately shorter than the group it separates: stopping
    // well short of the card's padding is what makes it read as a hint rather
    // than a structural rule.
    static constexpr int kLineHeight = 22;
    static constexpr int kAlpha      = 60;   // of 255 — barely there

    explicit ToolDivider(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(BorderWidth::Hairline);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    QSize sizeHint() const override {
        return { BorderWidth::Hairline, kLineHeight };
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QColor c = ThemeManager::instance().palette().Border;
        c.setAlpha(kAlpha);
        QPainter g(this);
        const int y0 = (height() - kLineHeight) / 2;
        g.fillRect(QRect(0, qMax(0, y0), width(), qMin(kLineHeight, height())), c);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// SoftButton — every clickable affordance in the new chrome.
// ─────────────────────────────────────────────────────────────────────────────
class SoftButton : public QToolButton {
public:
    enum class Variant {
        Ghost,    // transparent until hovered — toolbar/header default
        Soft,     // a quiet filled chip that is always visible
        Primary,  // filled with the brand accent — one per screen, at most
        Danger,   // transparent until hovered, then red — the window's close
    };
    enum class Layout {
        IconOnly,
        IconAboveText,   // the floating toolbar's element buttons
        IconLeftOfText,  // header actions ("Export"), sidebar entries
        TextOnly,
    };

    explicit SoftButton(QWidget* parent = nullptr)
        : QToolButton(parent) {
        setFocusPolicy(Qt::NoFocus);   // keyboard focus belongs to the canvas
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setToolButtonStyle(Qt::ToolButtonIconOnly);   // we paint it ourselves
    }

    // ── Configuration ────────────────────────────────────────────────────────
    SoftButton* variant(Variant v)      { variant_ = v;  update(); return this; }
    SoftButton* layout_mode(Layout l)   { layout_ = l;   updateGeometry(); return this; }
    SoftButton* radius(int r)           { radius_ = r;   update(); return this; }
    SoftButton* height_token(int h)     { height_ = h;   updateGeometry(); return this; }
    SoftButton* icon_px(int px)         { icon_px_ = px; updateGeometry(); return this; }

    // Painting the glyph ourselves (rather than storing a pre-coloured QIcon)
    // is what lets the icon tint track hover/checked state continuously.
    SoftButton* glyph(IconManager::Id id) { glyph_ = id; update(); return this; }

    // Text shown by the IconAboveText / IconLeftOfText / TextOnly layouts.
    // Kept separate from QToolButton::text() so a setDefaultAction() call
    // (which overwrites text()) can't silently relabel the button.
    SoftButton* label(const QString& t) { label_ = t; updateGeometry(); return this; }

    SoftButton* text_size(Typography::Size s) { text_size_ = s; updateGeometry(); return this; }

    QSize sizeHint() const override {
        const int h = height_;
        switch (layout_) {
        case Layout::IconOnly:
            return { h, h };
        case Layout::IconAboveText: {
            // Width follows the LABEL (plus padding), floored only by the icon
            // itself — not by the button's height. Flooring at `h` made every
            // short label ("Cena", "Ação") as wide as the button was tall,
            // which is what inflated the toolbar card past the page width.
            const QFontMetrics fm(label_font());
            const int w = qMax(fm.horizontalAdvance(label_) + Spacing::S * 2,
                               icon_px_ + Spacing::S * 2);
            return { w, h };
        }
        case Layout::IconLeftOfText: {
            const QFontMetrics fm(label_font());
            return { Spacing::L + icon_px_ + Spacing::S
                         + fm.horizontalAdvance(label_) + Spacing::L, h };
        }
        case Layout::TextOnly: {
            const QFontMetrics fm(label_font());
            return { fm.horizontalAdvance(label_) + Spacing::L * 2, h };
        }
        }
        return { h, h };
    }

    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void enterEvent(QEnterEvent* ev) override {
        hover_.target(1.0);
        QToolButton::enterEvent(ev);
    }
    void leaveEvent(QEvent* ev) override {
        hover_.target(0.0);
        press_.target(0.0);
        QToolButton::leaveEvent(ev);
    }
    void mousePressEvent(QMouseEvent* ev) override {
        press_.target(1.0);
        QToolButton::mousePressEvent(ev);
    }
    void mouseReleaseEvent(QMouseEvent* ev) override {
        press_.target(0.0);
        QToolButton::mouseReleaseEvent(ev);
    }

    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing, true);
        g.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const bool on  = isChecked();
        const bool off = !isEnabled();
        const double hv = off ? 0.0 : hover_.value();
        const double pr = off ? 0.0 : press_.value();

        // ── Surface ──────────────────────────────────────────────────────────
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const int    rad = effective_radius();
        QPainterPath shape;
        shape.addRoundedRect(r, rad, rad);

        QColor fill = base_fill(p, on);
        // Hover and press deepen the surface rather than swapping it, so the
        // transition reads as one continuous material.
        fill = blend(fill, wash_over(p, on), hv * 0.55 + pr * 0.45);
        if (fill.alpha() > 0) g.fillPath(shape, fill);

        if (variant_ == Variant::Soft && !on) {
            g.setPen(QPen(p.Border, BorderWidth::Hairline));
            g.drawPath(shape);
        }

        // ── Content ──────────────────────────────────────────────────────────
        const QColor fg = foreground(p, on, off, qMax(hv, pr));
        const QRect  box = rect();

        auto draw_glyph = [&](const QRect& target) {
            if (glyph_) {
                IconManager::make(*glyph_, fg).paint(&g, target, Qt::AlignCenter);
            } else if (!icon().isNull()) {
                icon().paint(&g, target, Qt::AlignCenter, isEnabled()
                                 ? QIcon::Normal : QIcon::Disabled);
            }
        };

        g.setPen(fg);
        g.setFont(label_font());

        switch (layout_) {
        case Layout::IconOnly:
            // Draw at icon_px_, NOT into the whole button: letting QIcon fill
            // the 40px box would render this glyph visibly larger and heavier
            // than the same family's glyph in an IconAboveText button beside
            // it, and one uniform icon size is a hard rule of the design.
            draw_glyph(QRect(box.x() + (box.width()  - icon_px_) / 2,
                             box.y() + (box.height() - icon_px_) / 2,
                             icon_px_, icon_px_));
            break;
        case Layout::IconAboveText: {
            const QFontMetrics fm(label_font());
            const int text_h = fm.height();
            const int pad    = (box.height() - icon_px_ - text_h - Spacing::XS) / 2;
            draw_glyph(QRect(box.x(), box.y() + pad, box.width(), icon_px_));
            g.drawText(QRect(box.x(), box.y() + pad + icon_px_ + Spacing::XS,
                             box.width(), text_h),
                       Qt::AlignHCenter | Qt::AlignVCenter, label_);
            break;
        }
        case Layout::IconLeftOfText: {
            draw_glyph(QRect(box.x() + Spacing::L,
                             box.y() + (box.height() - icon_px_) / 2,
                             icon_px_, icon_px_));
            g.drawText(box.adjusted(Spacing::L + icon_px_ + Spacing::S, 0,
                                    -Spacing::L, 0),
                       Qt::AlignLeft | Qt::AlignVCenter, label_);
            break;
        }
        case Layout::TextOnly:
            g.drawText(box, Qt::AlignCenter, label_);
            break;
        }
    }

private:
    int effective_radius() const {
        return radius_ == Radius::Pill ? qMin(width(), height()) / 2 : radius_;
    }

    QFont label_font() const {
        return Typography::ui_font(text_size_, Typography::Weight::Medium);
    }

    // The surface a button shows at rest, before any hover/press wash.
    QColor base_fill(const ThemePalette& p, bool on) const {
        switch (variant_) {
        case Variant::Primary: return p.Primary;
        case Variant::Danger:  return QColor(0, 0, 0, 0);
        case Variant::Soft:    return on ? p.HoverBg : p.Card;
        case Variant::Ghost:
        default:
            if (on) {
                // Checked ghost buttons (active block type, active B/I/U) get a
                // translucent accent wash — present, but never as loud as a
                // filled Primary control.
                QColor c = p.Primary;
                c.setAlpha(p.isDarkChrome() ? 52 : 34);
                return c;
            }
            return QColor(0, 0, 0, 0);
        }
    }

    // What hover/press blends the surface TOWARDS.
    QColor wash_over(const ThemePalette& p, bool on) const {
        if (variant_ == Variant::Danger) return p.DangerAccent;
        if (variant_ == Variant::Primary) {
            // Lighten on dark chrome, deepen on light — either way the accent
            // stays recognisably itself.
            return p.isDarkChrome() ? p.Primary.lighter(118) : p.Primary.darker(112);
        }
        if (on) {
            QColor c = p.Primary;
            c.setAlpha(p.isDarkChrome() ? 84 : 62);
            return c;
        }
        return p.HoverBg;
    }

    QColor foreground(const ThemePalette& p, bool on, bool disabled,
                      double active) const {
        if (disabled) return p.TextDim;
        if (variant_ == Variant::Primary) return p.OnPrimary();
        if (variant_ == Variant::Danger) {
            // Fades to the contrast colour in step with the red filling in, so
            // the glyph never sits mid-way as unreadable grey-on-red.
            return blend(p.Text, ThemePalette::contrastOn(p.DangerAccent),
                         qBound(0.0, active * 1.6, 1.0));
        }
        if (on) return p.Primary;
        return p.Text;
    }

    // Straight linear blend; `t` is clamped so an in-flight animation can
    // never overshoot into an out-of-range colour.
    static QColor blend(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        // Blending against a fully transparent base must not drag the result
        // towards black — interpolate premultiplied against b's own hue.
        if (a.alpha() == 0) {
            QColor out = b;
            out.setAlpha(int(b.alpha() * t));
            return out;
        }
        return QColor(int(a.red()   + (b.red()   - a.red())   * t),
                      int(a.green() + (b.green() - a.green()) * t),
                      int(a.blue()  + (b.blue()  - a.blue())  * t),
                      int(a.alpha() + (b.alpha() - a.alpha()) * t));
    }

    Variant  variant_ = Variant::Ghost;
    Layout   layout_  = Layout::IconOnly;
    int      radius_  = Radius::Button;
    int      height_  = ControlHeight::Button;
    int      icon_px_ = IconSize::Toolbar;
    QString  label_;
    Typography::Size text_size_ = Typography::Size::Caption;
    std::optional<IconManager::Id> glyph_;

    AnimatedFloat hover_{ this, AnimationDuration::Hover };
    AnimatedFloat press_{ this, AnimationDuration::Pressed };
};

} // namespace screenplay::ui
