#pragma once
// Locked scene numbers.
//
// Before production, scene numbers are just the scenes' positions: insert one
// and everything after it renumbers. The moment a script is distributed that
// becomes destructive — the schedule, the call sheets, the shot lists and the
// script supervisor's notes all key off the numbers that went out. So the
// numbers get LOCKED, and any scene added later takes a letter suffix in the
// gap it was inserted into: 1, 1A, 1B, 2. Nothing already issued ever moves.
//
// Pure logic: no Qt, no widgets, no undo. The caller owns those.

#include "../model/model.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace screenplay::production {

/// Position of a scene heading in the block list, with the number it shows.
struct SceneNumber {
    size_t      block_idx;
    std::string label;
};

namespace detail {

inline bool is_heading(const screenplay::Block& b) {
    return b.type == screenplay::BlockType::SceneHeading;
}

/// "1A" -> "1", "A5" -> "5", "12" -> "12". The digits are the anchor a
/// suffixed scene hangs off; the letters say where in the gap it sits.
inline std::string digits_of(const std::string& label) {
    std::string out;
    for (char c : label)
        if (c >= '0' && c <= '9') out += c;
    return out;
}

/// A, B, ... Z, AA, AB, ... — spreadsheet-column order, so an unlimited number
/// of inserts into one gap still sorts correctly.
inline std::string suffix_at(int index) {
    std::string out;
    ++index;
    while (index > 0) {
        --index;
        out.insert(out.begin(), (char)('A' + index % 26));
        index /= 26;
    }
    return out;
}

inline bool taken(const std::vector<std::string>& used, const std::string& label) {
    return std::find(used.begin(), used.end(), label) != used.end();
}

/// True for a label whose letters come FIRST ("A1"), i.e. a scene that sits
/// ahead of the number it hangs off rather than after it.
inline bool is_prefixed(const std::string& label) {
    return !label.empty() && (label[0] < '0' || label[0] > '9');
}

/// The next free label for a scene inserted into the gap after `anchor`.
/// `before_first` builds "A1", "B1" instead of "1A" — a scene inserted ahead
/// of scene one has to sort BEFORE it.
inline std::string next_free(const std::vector<std::string>& used,
                             const std::string& anchor, bool before_first) {
    const std::string base = anchor.empty() ? std::string("1") : digits_of(anchor);
    for (int i = 0; ; ++i) {
        const std::string suffix = suffix_at(i);
        const std::string label  = before_first ? suffix + base : base + suffix;
        if (!taken(used, label)) return label;
    }
}

} // namespace detail

/// Every scene heading's number, in document order.
///
/// Unlocked, these are the positions 1..N. Locked, they are whatever is stored
/// on each block — including the letter suffixes — so what the writer sees on
/// the page is exactly what was distributed.
inline std::vector<SceneNumber> scene_numbers(const screenplay::Script& script) {
    std::vector<SceneNumber> out;
    int position = 0;
    for (size_t i = 0; i < script.blocks.size(); ++i) {
        if (!detail::is_heading(script.blocks[i])) continue;
        ++position;
        out.push_back({ i, script.scenes_locked
                               ? script.blocks[i].scene_number
                               : std::to_string(position) });
    }
    return out;
}

/// Gives every unnumbered scene a label without disturbing the numbered ones.
///
/// Called both when locking (nothing is numbered yet, so this is a plain 1..N)
/// and after every later edit (only the newly inserted scenes are unnumbered,
/// so only they get suffixes).
inline void assign_missing_numbers(screenplay::Script& script) {
    std::vector<std::string> used;
    for (const auto& b : script.blocks)
        if (detail::is_heading(b) && !b.scene_number.empty())
            used.push_back(b.scene_number);

    const bool first_pass = used.empty();
    int  sequential = 0;
    std::string anchor;          // last numbered scene seen

    for (auto& block : script.blocks) {
        if (!detail::is_heading(block)) continue;
        if (!block.scene_number.empty()) { anchor = block.scene_number; continue; }

        if (first_pass) {
            block.scene_number = std::to_string(++sequential);
        } else {
            // Following a prefixed label means we are still ahead of scene one,
            // so this scene needs a prefix too — "B1", never "1A", which would
            // sort after the scene it is supposed to precede.
            const bool ahead = anchor.empty() || detail::is_prefixed(anchor);
            block.scene_number = detail::next_free(used, anchor, ahead);
        }
        used.push_back(block.scene_number);
        anchor = block.scene_number;
    }
}

/// Freezes the current numbering. From here on the numbers belong to the
/// production, not to the scenes' positions.
inline void lock_scenes(screenplay::Script& script) {
    for (auto& block : script.blocks)
        if (detail::is_heading(block)) block.scene_number.clear();
    script.scenes_locked = true;
    assign_missing_numbers(script);
}

/// Hands numbering back to position order. Only safe before distribution —
/// afterwards it silently invalidates every schedule built from the old set.
inline void unlock_scenes(screenplay::Script& script) {
    script.scenes_locked = false;
    for (auto& block : script.blocks)
        if (detail::is_heading(block)) block.scene_number.clear();
}

/// What an omitted scene's heading reads on the page. The number stays; the
/// content is gone.
inline const char* kOmittedText = "OMITTED";

} // namespace screenplay::production
