#pragma once
// ui/status_strip.hpp
// StatusStrip — the minimal bottom bar. A plain QWidget rather than a
// QStatusBar so none of Qt's built-in chrome (the sunken frame, the size grip,
// the item borders) can leak through. Groups are separated by generous space,
// never by vertical rules.
//
// Owned responsibility: laying out and styling the status readouts. The VALUES
// are pushed in by MainWindow, which owns the statistics.

#include "design_tokens.hpp"
#include "controls.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <vector>

namespace screenplay::ui {

class StatusStrip : public QWidget {
public:
    explicit StatusStrip(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(ChromeHeight::StatusBar);
        row_ = new QHBoxLayout(this);
        row_->setContentsMargins(Spacing::XL, 0, Spacing::L, 0);
        row_->setSpacing(0);

        // No element readout here: the caret's element is already named on
        // the badge beside its own line on the page, and repeating it in the
        // corner is noise the footer does not need.
        message_ = add_label(Emphasis::Dim);
        row_->addStretch(1);

        saved_   = add_readout();
        words_   = add_readout();
        scenes_  = add_readout();
        runtime_ = add_readout();
        page_    = add_readout();

        row_->addSpacing(Spacing::L);

        zoom_out_ = new SoftButton(this);
        zoom_out_->glyph(IconManager::Id::ZoomOut)
                 ->layout_mode(SoftButton::Layout::IconOnly)
                 ->height_token(ControlHeight::Compact)
                 ->icon_px(IconSize::Chrome)
                 ->radius(Radius::Button);
        zoom_out_->setToolTip("Zoom out (Ctrl+-)");
        row_->addWidget(zoom_out_);

        zoom_ = add_label(Emphasis::Normal);
        zoom_->setFixedWidth(44);
        zoom_->setAlignment(Qt::AlignCenter);
        zoom_->setToolTip("Zoom (Ctrl+scroll on the page)");

        zoom_in_ = new SoftButton(this);
        zoom_in_->glyph(IconManager::Id::ZoomIn)
                ->layout_mode(SoftButton::Layout::IconOnly)
                ->height_token(ControlHeight::Compact)
                ->icon_px(IconSize::Chrome)
                ->radius(Radius::Button);
        zoom_in_->setToolTip("Zoom in (Ctrl++)");
        row_->addWidget(zoom_in_);

        restyle();
    }

    // ── Values ───────────────────────────────────────────────────────────────
    void set_message(const QString& t)    { message_->setText(t); }
    void set_saved(const QString& t)      { saved_->setText(t); }
    void set_words(const QString& t)      { words_->setText(t); }
    void set_scenes(const QString& t)     { scenes_->setText(t); }
    void set_runtime(const QString& t)    { runtime_->setText(t); }
    void set_page(const QString& t)       { page_->setText(t); }
    void set_zoom(const QString& t)       { zoom_->setText(t); }

    SoftButton* zoom_in()     const { return zoom_in_; }
    SoftButton* zoom_out()    const { return zoom_out_; }

    void restyle() {
        const ThemePalette& p = ThemeManager::instance().palette();
        using Ty = Typography;
        for (auto& [lbl, emph] : labels_) {
            const QColor c = emph == Emphasis::Accent ? p.Primary
                           : emph == Emphasis::Dim    ? p.TextDim
                                                      : p.Text;
            lbl->setStyleSheet(
                QString("color:%1; font-family:'%2'; font-size:%3px;%4")
                    .arg(ThemePalette::hex(c), Ty::family())
                    .arg(Ty::size_px(Ty::Size::BodySmall))
                    .arg(emph == Emphasis::Accent ? "font-weight:600;" : ""));
        }
        update();
    }

protected:
    // Backdrop plus ONE hairline along the top. With the window painted a
    // single uniform colour the regions had become indistinguishable, so each
    // structural surface now states where it begins — a 1px rule, nothing
    // heavier, and only on the edge that faces the page.
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.fillRect(rect(), p.Bg0);
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawLine(0, 0, width(), 0);
    }

private:
    enum class Emphasis { Normal, Dim, Accent };

    QLabel* add_label(Emphasis e) {
        auto* l = new QLabel(this);
        row_->addWidget(l);
        labels_.emplace_back(l, e);
        return l;
    }

    // A right-side readout: dim text with generous space before the next one.
    QLabel* add_readout() {
        auto* l = add_label(Emphasis::Dim);
        row_->addSpacing(Spacing::XL);
        return l;
    }

    QHBoxLayout* row_ = nullptr;
    std::vector<std::pair<QLabel*, Emphasis>> labels_;

    QLabel* message_    = nullptr;
    QLabel* saved_      = nullptr;
    QLabel* words_      = nullptr;
    QLabel* scenes_     = nullptr;
    QLabel* runtime_    = nullptr;
    QLabel* page_       = nullptr;
    QLabel* zoom_       = nullptr;
    SoftButton* zoom_in_  = nullptr;
    SoftButton* zoom_out_ = nullptr;
};

} // namespace screenplay::ui
