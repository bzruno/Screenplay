#pragma once
#include "model.hpp"
#include <deque>
#include <optional>
#include <functional>

namespace screenplay {

struct Command {
    Script before;
    Script after;
    Cursor cursor_before;
    Cursor cursor_after;
};

class UndoStack {
public:
    static constexpr size_t kMaxDepth = 200;

    void push(Command cmd) {
        if (redo_top_ > 0) {
            history_.erase(history_.end() - static_cast<ptrdiff_t>(redo_top_),
                           history_.end());
            redo_top_ = 0;
        }
        history_.push_back(std::move(cmd));
        if (history_.size() > kMaxDepth)
            history_.pop_front();
    }

    std::optional<Command> undo() {
        if (history_.size() <= redo_top_) return std::nullopt;
        ++redo_top_;
        return history_[history_.size() - redo_top_];
    }

    std::optional<Command> redo() {
        if (redo_top_ == 0) return std::nullopt;
        auto& cmd = history_[history_.size() - redo_top_];
        --redo_top_;
        return cmd;
    }

    bool can_undo() const { return history_.size() > redo_top_; }
    bool can_redo() const { return redo_top_ > 0; }

private:
    std::deque<Command> history_;
    size_t redo_top_ = 0;
};

} // namespace screenplay
