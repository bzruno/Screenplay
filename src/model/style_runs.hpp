#pragma once
// model/style_runs.hpp
// StyleRuns — a sorted, disjoint set of half-open byte ranges [start, end)
// within a Block's text where one style attribute (bold, italic, or
// underline — each tracked as its own independent StyleRuns) is active.
// This is what lets "select one word, press Bold" style only that word,
// instead of the whole block.
//
// Pure C++ / std only — no Qt, no UI. Every text-mutating operation in
// EditorController must keep these ranges in sync via shift_insert() /
// shift_erase() (or split/merge them directly when a block itself splits or
// merges), the same way it already keeps Cursor/undo state in sync.

#include <cstddef>
#include <utility>
#include <vector>
#include <algorithm>

namespace screenplay {

using StyleRuns = std::vector<std::pair<size_t, size_t>>;   // sorted, disjoint [start,end)

// True when [s, e) is entirely covered by `runs`. Empty ranges are trivially covered.
inline bool style_covers(const StyleRuns& runs, size_t s, size_t e) {
    if (s >= e) return true;
    size_t cur = s;
    for (const auto& [rs, re] : runs) {
        if (re <= cur) continue;         // entirely before the gap we still need
        if (rs > cur) return false;      // gap between `cur` and this run
        cur = std::max(cur, re);
        if (cur >= e) return true;
    }
    return cur >= e;
}

// Unions [s, e) into `runs`, merging with any overlapping/adjacent runs.
inline void style_add(StyleRuns& runs, size_t s, size_t e) {
    if (s >= e) return;
    StyleRuns out;
    out.reserve(runs.size() + 1);
    bool inserted = false;
    for (const auto& r : runs) {
        if (r.second < s) { out.push_back(r); continue; }           // strictly before, no touch
        if (r.first > e) {                                          // strictly after
            if (!inserted) { out.emplace_back(s, e); inserted = true; }
            out.push_back(r);
            continue;
        }
        // Overlaps or touches [s, e) — fold into the pending merge.
        s = std::min(s, r.first);
        e = std::max(e, r.second);
    }
    if (!inserted) out.emplace_back(s, e);
    runs = std::move(out);
}

// Subtracts [s, e) from `runs`, splitting any run that only partially overlaps.
inline void style_remove(StyleRuns& runs, size_t s, size_t e) {
    if (s >= e) return;
    StyleRuns out;
    out.reserve(runs.size());
    for (const auto& [rs, re] : runs) {
        if (re <= s || rs >= e) { out.emplace_back(rs, re); continue; }  // no overlap
        if (rs < s) out.emplace_back(rs, s);       // surviving head
        if (re > e) out.emplace_back(e, re);       // surviving tail
    }
    runs = std::move(out);
}

// Shifts every offset >= `at` forward by `count` — call after inserting
// `count` bytes of PLAIN (unstyled) text at byte position `at`.
inline void style_shift_insert(StyleRuns& runs, size_t at, size_t count) {
    if (count == 0) return;
    for (auto& [rs, re] : runs) {
        if (rs >= at) rs += count;
        if (re >= at) re += count;
    }
}

// Removes the byte range [at, at+count) — call after erasing `count` bytes
// starting at `at`. A run that straddles the erased region collapses to one
// contiguous surviving span (deleting the middle of a bold word keeps it one
// bold run, just shorter); a run entirely inside the erased region vanishes;
// everything after shifts back by `count`.
inline void style_shift_erase(StyleRuns& runs, size_t at, size_t count) {
    if (count == 0) return;
    const size_t end = at + count;
    StyleRuns out;
    out.reserve(runs.size());
    for (auto [rs, re] : runs) {
        if (re <= at)  { out.emplace_back(rs, re); continue; }                 // entirely before
        if (rs >= end) { out.emplace_back(rs - count, re - count); continue; } // entirely after
        // Overlaps the erased range. Surviving start: the run's own start if
        // it began before `at` (that head is untouched), else `at` (nothing
        // of the run survives before the erasure). Surviving end: `at` if the
        // run didn't reach past `end` (its tail was entirely erased), else
        // its end shifted back by `count` (the surviving tail closes the gap).
        const size_t new_rs = (rs < at)  ? rs : at;
        const size_t new_re = (re <= end) ? at : (re - count);
        if (new_rs < new_re) out.emplace_back(new_rs, new_re);
    }
    runs = std::move(out);
}

// Extracts the portion of `runs` within [s, e), re-based so it starts at 0 —
// used when text moves from one block to another (Enter splitting a block,
// paste's tail, Backspace/Delete merging blocks).
inline StyleRuns style_extract(const StyleRuns& runs, size_t s, size_t e) {
    StyleRuns out;
    for (auto [rs, re] : runs) {
        rs = std::max(rs, s);
        re = std::min(re, e);
        if (rs < re) out.emplace_back(rs - s, re - s);
    }
    return out;
}

// Appends `src` (already relative to its own text) onto `dst`, offsetting
// every entry by `base` — used when one block's tail text (and its runs)
// is appended onto another block's surviving text.
inline void style_append_shifted(StyleRuns& dst, const StyleRuns& src, size_t base) {
    for (auto [rs, re] : src) style_add(dst, rs + base, re + base);
}

} // namespace screenplay
