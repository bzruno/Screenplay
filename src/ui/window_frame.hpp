#pragma once
// ui/window_frame.hpp
// Everything needed to replace the OS title bar with our own, without
// replacing the behaviour that comes with it.
//
// Two pieces:
//   FrameGeometry  — pure edge maths: which resize edge (if any) a point is on,
//                    and the cursor that belongs to it.
//   WindowControls — the minimise / maximise / close cluster.
//
// The MOVE and RESIZE gestures deliberately delegate to Qt's
// QWindow::startSystemMove() / startSystemResize(), NOT to manual geometry
// arithmetic. Those hand the gesture to the window manager, so a frameless
// window keeps everything users expect for free: Aero Snap, snap layouts,
// drag-to-edge, the resize outline, multi-monitor DPI handling and
// double-click-to-maximise. Re-implementing drag by hand (the usual
// "pos() - drag_start_" trick) silently throws all of that away.
//
// Owned responsibility: window-frame interaction and the control cluster's
// look. It never decides window POLICY — the caller owns the QWidget.

#include "design_tokens.hpp"
#include "controls.hpp"
#include "icon_manager.hpp"

#include <QWidget>
#include <QHBoxLayout>
#include <QWindow>
#include <Qt>

namespace screenplay::ui {

// ─────────────────────────────────────────────────────────────────────────────
// FrameGeometry — resize-edge hit testing.
// ─────────────────────────────────────────────────────────────────────────────
struct FrameGeometry {
    // How far inside the window edge still counts as "grab to resize".
    // Deliberately a little wider than the 4px Windows itself uses: with no
    // visible frame to aim at, a too-thin band feels broken.
    static constexpr int kGrip = 6;

    static Qt::Edges edges_at(const QSize& size, const QPoint& pos,
                              int grip = kGrip) {
        Qt::Edges e;
        if (pos.x() <= grip)                    e |= Qt::LeftEdge;
        if (pos.x() >= size.width()  - grip)    e |= Qt::RightEdge;
        if (pos.y() <= grip)                    e |= Qt::TopEdge;
        if (pos.y() >= size.height() - grip)    e |= Qt::BottomEdge;
        return e;
    }

    static Qt::CursorShape cursor_for(Qt::Edges e) {
        const bool l = e & Qt::LeftEdge,  r = e & Qt::RightEdge;
        const bool t = e & Qt::TopEdge,   b = e & Qt::BottomEdge;
        if ((l && t) || (r && b)) return Qt::SizeFDiagCursor;
        if ((r && t) || (l && b)) return Qt::SizeBDiagCursor;
        if (l || r)               return Qt::SizeHorCursor;
        if (t || b)               return Qt::SizeVerCursor;
        return Qt::ArrowCursor;
    }

    // Starts a native resize if `pos` is on an edge. Returns true if it did.
    // No-op while maximised — the edges aren't grabbable in that state and
    // letting the WM try produces a visible flicker.
    static bool begin_resize(QWidget* window, const QPoint& pos) {
        if (!window || window->isMaximized() || window->isFullScreen())
            return false;
        const Qt::Edges e = edges_at(window->size(), pos);
        if (!e) return false;
        if (QWindow* h = window->windowHandle()) {
            h->startSystemResize(e);
            return true;
        }
        return false;
    }

    // Hands a drag on the title area to the window manager.
    static bool begin_move(QWidget* window) {
        if (!window) return false;
        if (QWindow* h = window->windowHandle()) {
            h->startSystemMove();
            return true;
        }
        return false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// WindowControls — minimise / maximise / close.
// ─────────────────────────────────────────────────────────────────────────────
class WindowControls : public QWidget {
public:
    // Full-bleed cells, exactly like the OS ones they replace: each button
    // fills its whole cell top to bottom with the glyph centred, so the entire
    // rectangle is clickable rather than just a rounded island inside it.
    // That is also why the radius is 0 and the spacing between them is 0 — a
    // gap would be a dead strip the click falls through.
    static constexpr int kButtonW = 46;
    static constexpr int kGlyphPx = IconSize::Chrome;

    explicit WindowControls(QWidget* parent = nullptr) : QWidget(parent) {
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        min_   = make(IconManager::Id::WinMinimize,
                      SoftButton::Variant::Ghost,  "Minimize");
        max_   = make(IconManager::Id::WinMaximize,
                      SoftButton::Variant::Ghost,  "Maximize");
        close_ = make(IconManager::Id::Close,
                      SoftButton::Variant::Danger, "Close");
        row->addWidget(min_);
        row->addWidget(max_);
        row->addWidget(close_);

        QObject::connect(min_, &QToolButton::clicked, this, [this]{
            if (auto* w = window()) w->showMinimized();
        });
        QObject::connect(max_, &QToolButton::clicked, this, [this]{
            auto* w = window();
            if (!w) return;
            if (w->isMaximized()) w->showNormal();
            else                  w->showMaximized();
            sync_state();
        });
        QObject::connect(close_, &QToolButton::clicked, this, [this]{
            if (auto* w = window()) w->close();
        });
    }

    // Keeps the middle glyph honest about the window's actual state. Call from
    // the host's changeEvent on WindowStateChange.
    void sync_state() {
        auto* w = window();
        const bool maxed = w && (w->isMaximized() || w->isFullScreen());
        max_->glyph(maxed ? IconManager::Id::WinRestore
                          : IconManager::Id::WinMaximize);
        max_->setToolTip(maxed ? "Restore" : "Maximize");
    }

private:
    SoftButton* make(IconManager::Id glyph, SoftButton::Variant v,
                     const char* tip) {
        auto* b = new SoftButton(this);
        b->glyph(glyph)
         ->layout_mode(SoftButton::Layout::IconOnly)
         ->variant(v)
         ->radius(0)                     // square: the cell IS the target
         ->icon_px(kGlyphPx);
        b->setFixedWidth(kButtonW);
        // Height is driven by the layout, not fixed, so the cell always spans
        // the full title-bar height however the header is sized.
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        b->setToolTip(tip);
        b->setCursor(Qt::ArrowCursor);   // window chrome, not app content
        return b;
    }

    SoftButton* min_   = nullptr;
    SoftButton* max_   = nullptr;
    SoftButton* close_ = nullptr;
};

} // namespace screenplay::ui
