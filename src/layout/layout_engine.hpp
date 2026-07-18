#pragma once
#include "../model/model.hpp"
#include "font_metrics.hpp"
#include <vector>
#include <string>

namespace screenplay::layout {

// ─────────────────────────────────────────────────────────────────────────────
// Page geometry — A4 (595 x 842 pts) with exact WGA margins
// 1 inch = 72 pts
// ─────────────────────────────────────────────────────────────────────────────
struct PageGeometry {
    float page_w       = 595.f;   // A4: 8.27 in
    float page_h       = 842.f;   // A4: 11.69 in
    float margin_top   =  72.f;   // 1.0 in
    float margin_bot   =  72.f;   // 1.0 in
    float margin_left  = 108.f;   // 1.5 in
    float margin_right =  72.f;   // 1.0 in

    float printable_w() const { return page_w - margin_left - margin_right; }
    float printable_h() const { return page_h - margin_top  - margin_bot;  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-block formatting (WGA standard indentation in points)
// ─────────────────────────────────────────────────────────────────────────────
struct BlockFormat {
    float left_indent;    // extra indent from left margin
    float right_indent;   // extra indent from right margin
    float space_before;   // vertical space before block
    float space_after;    // vertical space after block
    bool  uppercase;
    bool  right_align;
};

inline BlockFormat format_for(screenplay::BlockType t) {
    //                         left   right  before after  upper  right
    switch (t) {
    case screenplay::BlockType::SceneHeading:
        return {   0,    0,   14,  7,  true,  false };
    case screenplay::BlockType::Action:
        return {   0,    0,    7,  7,  false, false };
    case screenplay::BlockType::Character:
        return { 144,   72,  14,  0,  true,  false };
    case screenplay::BlockType::Parenthetical:
        return { 108,  108,   0,  0,  false, false };
    case screenplay::BlockType::Dialogue:
        return { 108,  108,   0,  7,  false, false };
    case screenplay::BlockType::Transition:
        return {   0,    0,  14, 14,  true,  true  };
    case screenplay::BlockType::DualDialogue:
        return { 108,  108,   0,  7,  false, false };
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout output
// ─────────────────────────────────────────────────────────────────────────────
struct VisualLine {
    size_t      block_idx;
    size_t      line_in_block;
    std::string display_text;
    float       x, y;        // absolute position in page coordinates (pts)
    float       width, height;
    // Byte offsets into the owning block's text (same byte structure as display_text).
    // Non-final wrapped lines: cursor is on this line when byte_offset ∈ [start_offset, end_offset).
    // Final wrapped line: cursor is on this line when byte_offset ∈ [start_offset, end_offset].
    size_t      start_offset = 0;
    size_t      end_offset   = 0;
    bool        is_more  = false;   // virtual "(MORE)" at bottom of interrupted page
    bool        is_contd = false;   // virtual "NAME (CONT'D)" at top of continuation page
};

struct Page {
    int                     number;  // 1-based; page 1 shows NO number
    std::vector<VisualLine> lines;
};

using PageList = std::vector<Page>;

// ─────────────────────────────────────────────────────────────────────────────
// Layout engine — pure function, never touches Script
// ─────────────────────────────────────────────────────────────────────────────
class LayoutEngine {
public:
    explicit LayoutEngine(const IFontMetrics& metrics,
                          PageGeometry        geo            = {},
                          float               pt_size        = 12.f,
                          float               spacing_factor = 1.f)
        : metrics_(metrics), geo_(geo), pt_size_(pt_size),
          spacing_factor_(spacing_factor) {}

    PageList layout(const screenplay::Script& script) const;

    const PageGeometry& geometry() const { return geo_; }
    float               pt_size()  const { return pt_size_; }

private:
    const IFontMetrics& metrics_;
    PageGeometry        geo_;
    float               pt_size_;
    float               spacing_factor_ = 1.f;
};

} // namespace screenplay::layout
