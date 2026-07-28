#pragma once
// Finds and navigates occurrences of a query inside a Script.
//
// Pure logic: no widgets, no painting, no scrolling. The editor asks it what
// matched and where the cursor is; presenting that is the caller's job.

#include "../model/model.hpp"

#include <QString>
#include <optional>
#include <vector>

namespace screenplay::editor {

/// A hit inside one block's text, as UTF-8 byte offsets.
struct SearchMatch {
    size_t block_idx;
    size_t start_offset;
    size_t end_offset;
};

class ScriptSearch {
public:
    /// Matches every block, regardless of element type.
    static constexpr int kAnyType = -1;

    /// Recomputes every match. An empty query combined with a type filter
    /// selects whole blocks of that type, which is how browse-by-element works.
    void rebuild(const screenplay::Script& script, const QString& query,
                 int type_filter) {
        matches_.clear();
        current_ = -1;
        active_  = !query.isEmpty() || type_filter != kAnyType;
        if (!active_) return;

        const auto& blocks = script.blocks;
        for (size_t index = 0; index < blocks.size(); ++index) {
            if (!type_matches(blocks[index].type, type_filter)) continue;
            if (query.isEmpty()) add_whole_block(blocks[index], index);
            else                 add_query_hits(blocks[index], index, query);
        }
        if (!matches_.empty()) current_ = 0;
    }

    void clear() { *this = ScriptSearch{}; }

    bool active() const { return active_; }
    int  count()  const { return (int)matches_.size(); }
    int  current_index() const { return current_; }
    const std::vector<SearchMatch>& matches() const { return matches_; }

    /// Moves the cursor by `step`, wrapping around, and returns the block that
    /// should be brought into view. Empty when there is nothing to move to.
    std::optional<size_t> advance(int step) {
        if (matches_.empty()) return std::nullopt;
        const int total = (int)matches_.size();
        current_ = ((current_ + step) % total + total) % total;
        return matches_[(size_t)current_].block_idx;
    }

private:
    /// DualDialogue answers to a Dialogue filter: to the writer they are the
    /// same element, just laid out in two columns.
    static bool type_matches(screenplay::BlockType type, int filter) {
        if (filter == kAnyType) return true;
        const auto wanted = (screenplay::BlockType)filter;
        if (wanted == screenplay::BlockType::Dialogue &&
            type   == screenplay::BlockType::DualDialogue) return true;
        return type == wanted;
    }

    void add_whole_block(const screenplay::Block& block, size_t index) {
        if (!block.text.empty())
            matches_.push_back({ index, 0, block.text.size() });
    }

    void add_query_hits(const screenplay::Block& block, size_t index,
                        const QString& query) {
        const QString text = QString::fromStdString(block.text);
        int at = 0;
        while ((at = text.indexOf(query, at, Qt::CaseInsensitive)) != -1) {
            const size_t start = (size_t)text.left(at).toUtf8().size();
            const size_t end   = start
                + (size_t)text.mid(at, query.size()).toUtf8().size();
            matches_.push_back({ index, start, end });
            ++at;
        }
    }

    std::vector<SearchMatch> matches_;
    int  current_ = -1;
    bool active_  = false;
};

} // namespace screenplay::editor
