#pragma once
// Eased vertical scrolling, paced to the display the window is actually on.
//
// Owns the scroll position and the animation that moves it. Knows nothing
// about pages or layout: the owner supplies the upper bound and reacts to
// each new position through the frame hook.

#include <QEasingCurve>
#include <QElapsedTimer>
#include <QScreen>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>

namespace screenplay::ui {

class ScrollAnimator {
public:
    /// Total duration of an eased scroll, regardless of how many frames the
    /// display manages to show inside it.
    static constexpr int kDurationMs = 170;

    /// Returns the largest valid scroll position; the owner knows the
    /// document height, this class does not.
    using MaxPosition = std::function<float()>;
    /// Called after every change of position, animated or not.
    using OnFrame     = std::function<void()>;

    ScrollAnimator(QWidget* host, MaxPosition max_position, OnFrame on_frame)
        : host_(host), max_position_(std::move(max_position)),
          on_frame_(std::move(on_frame)) {
        timer_.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer_, &QTimer::timeout, &timer_,
                         [this] { tick(); });
    }

    float position() const { return position_; }

    /// Where the scroll is heading — the current position when idle. Chaining
    /// wheel notches off this makes successive notches accumulate instead of
    /// restarting from wherever the last animation happened to be.
    float destination() const { return running_ ? target_ : position_; }

    bool running() const { return running_; }

    /// Eases towards `target`, re-pacing to the display first so moving the
    /// window to another monitor is picked up without any signal plumbing.
    void animate_to(float target) {
        sync_to_display();
        frames_  = 0;
        from_    = position_;
        target_  = clamped(target);
        running_ = true;
        elapsed_.start();
        timer_.start();
    }

    /// Jumps straight there, abandoning any animation. For input that is
    /// already continuous (scrollbar drag, trackpad), where easing on top of
    /// the gesture would only add lag.
    void jump_to(float position) {
        stop();
        set_position(clamped(position));
    }

    void nudge(float delta) { jump_to(position_ + delta); }

    /// Pulls the position back inside the bounds after the document or the
    /// viewport changed shape. Does not notify — the caller is already
    /// repainting.
    void clamp_to_bounds() { position_ = clamped(position_); }

    void stop() {
        timer_.stop();
        running_ = false;
    }

    /// How long the last repaint took. The tick rate is never allowed to
    /// out-run it: firing faster than the canvas can paint starves the event
    /// loop of time for input and reads as stutter, not smoothness.
    void note_paint_cost(qint64 ms) {
        paint_cost_ms_ = paint_cost_ms_ * (1.f - kCostSmoothing)
                       + (float)ms * kCostSmoothing;
    }

    /// Milliseconds between frames as currently paced.
    int frame_interval_ms() const { return timer_.interval(); }

    /// The smoothed repaint cost the pacing is backing off to.
    float paint_cost_ms() const { return paint_cost_ms_; }

    /// Frames actually delivered during the last animation, and how many the
    /// pacing aimed for. A gap between them means the event loop could not
    /// keep up — which is what "it feels like 60 Hz on a 144 Hz screen" is.
    int frames_last_run() const { return frames_; }
    int frames_expected() const {
        const int interval = std::max(1, timer_.interval());
        return kDurationMs / interval;
    }

private:
    /// A 60Hz panel wants ~17ms and a 240Hz one ~4ms; anything outside this
    /// range is a display reporting nonsense, not a real refresh rate.
    static constexpr int   kMinIntervalMs = 4;
    static constexpr int   kMaxIntervalMs = 34;
    static constexpr float kCostSmoothing = 0.25f;

    /// One tick per refresh: the display cannot show more, and every extra
    /// tick is a repaint nobody sees. Backed off when this machine's paint is
    /// slower than its monitor is fast.
    void sync_to_display() {
        const QScreen* screen = host_ ? host_->screen() : nullptr;
        const qreal hz = (screen && screen->refreshRate() > 1.0)
                             ? screen->refreshRate() : 60.0;
        const int per_refresh = (int)std::lround(1000.0 / hz);
        timer_.setInterval(std::clamp(per_refresh, kMinIntervalMs,
                                      kMaxIntervalMs));
    }

    float clamped(float position) const {
        const float top = max_position_ ? max_position_() : 0.f;
        return std::clamp(position, 0.f, std::max(0.f, top));
    }

    void set_position(float position) {
        position_ = position;
        if (on_frame_) on_frame_();
    }

    /// Driven by wall-clock elapsed time rather than a fixed step per tick, so
    /// the scroll always takes exactly kDurationMs and a faster display simply
    /// gets more real frames inside it.
    void tick() {
        const qint64 ms = elapsed_.elapsed();
        if (ms >= kDurationMs) {
            stop();
            set_position(target_);
            return;
        }
        ++frames_;
        const qreal eased =
            easing_.valueForProgress((qreal)ms / (qreal)kDurationMs);
        set_position(from_ + (target_ - from_) * (float)eased);
    }

    QWidget*    host_ = nullptr;
    MaxPosition max_position_;
    OnFrame     on_frame_;

    QTimer        timer_;
    QElapsedTimer elapsed_;
    QEasingCurve  easing_{ QEasingCurve::OutCubic };

    float position_ = 0.f;
    float from_     = 0.f;
    float target_   = 0.f;
    bool  running_  = false;
    float paint_cost_ms_ = 0.f;
    int   frames_        = 0;
};

} // namespace screenplay::ui
