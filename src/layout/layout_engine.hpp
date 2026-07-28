#pragma once
#include "../model/model.hpp"
#include "font_metrics.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace screenplay::layout {

// ─────────────────────────────────────────────────────────────────────────────
// Layout profiles — two official screenwriting standards.
//
//   USLetter        US Industry Standard (Final Draft / Fade In / Movie Magic).
//                   Letter paper, 8.5 × 11 in. This is the DEFAULT and the
//                   reference for the "1 page ≈ 1 minute of screen time" rule.
//
//   InternationalA4 Same visual layout adapted to A4 paper. The text column is
//                   kept practically identical (≈432 pt) so line breaks match
//                   the US profile; only the page size and margins differ.
//
// All measurements are typographic points (1 pt = 1/72 in). The engine is
// resolution-independent: nothing here depends on screen DPI.
// ─────────────────────────────────────────────────────────────────────────────
enum class LayoutProfile : uint8_t {
    USLetter,
    InternationalA4,
};

// Screenplay body font size and single-spaced line height (pt).
// 12 pt Courier at 6 lines/inch → 12 pt per line, leading 0.
inline constexpr float kFontSizePt   = 12.f;
inline constexpr float kLineHeightPt = 12.f;

// ─────────────────────────────────────────────────────────────────────────────
// Page geometry — paper size and margins for a profile (pt).
// ─────────────────────────────────────────────────────────────────────────────
struct PageGeometry {
    float page_w;
    float page_h;
    float margin_top;
    float margin_bot;
    float margin_left;
    float margin_right;

    float printable_w() const { return page_w - margin_left - margin_right; }
    float printable_h() const { return page_h - margin_top  - margin_bot;  }

    // Letter: 612 × 792 pt, margins 72/72/108/72 → text width 432 pt.
    static PageGeometry us_letter() {
        return { 612.f, 792.f, 72.f, 72.f, 108.f, 72.f };
    }

    // A4: 595.28 × 841.89 pt, margins 72/72/95/68 → text width 432.28 pt.
    static PageGeometry international_a4() {
        return { 595.28f, 841.89f, 72.f, 72.f, 95.f, 68.f };
    }

    static PageGeometry for_profile(LayoutProfile p) {
        return p == LayoutProfile::InternationalA4 ? international_a4()
                                                   : us_letter();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-block formatting. `left`/`right` are absolute insets measured from the
// page's left and right edges (NOT from the margins), so the numbers match the
// official profile tables directly. The text column width is therefore
// page_w - left - right.
// ─────────────────────────────────────────────────────────────────────────────
struct BlockFormat {
    float left;             // inset from page LEFT edge (pt)
    float right;            // inset from page RIGHT edge (pt)
    float space_before;     // vertical space before block (pt, multiple of grid)
    float space_after;      // vertical space after block (pt, multiple of grid)
    bool  uppercase;
    bool  right_align;
    bool  keep_with_next;   // never left alone at the bottom of a page
    int   widow;            // min lines that must remain after an in-block break
    int   orphan;           // min lines that must precede an in-block break
    // Centres each line within this block's own text column. Never set by
    // format_for() — only by an author's BlockAlign::Center override, applied
    // in LayoutEngine::layout(). Mutually exclusive with right_align, which
    // takes precedence if both are somehow set.
    bool  center_align = false;

    float col_width(const PageGeometry& g) const {
        return g.page_w - left - right;
    }
};

// The "may this type be realigned?" rule belongs to the document model (see
// screenplay::supports_alignment in model/model.hpp) — re-exported here so
// layout code can call it unqualified alongside the other format helpers.
using screenplay::supports_alignment;

// Whether `next` continues the speech `prev` belongs to.
//
// The 12 pt after Dialogue is the gap to the NEXT SPEAKER, not a gap between
// the parts of one speech. A wrylie dropped into the middle of a line —
// "Estou em casa." / "(pega o celular)" / "Agora!" — is one continuous speech
// and is set tight; letting Dialogue's space_after collapse into it opens a
// blank line that no screenplay has.
inline bool continues_speech(screenplay::BlockType prev,
                             screenplay::BlockType next) {
    using BT = screenplay::BlockType;
    const bool prev_speaks = prev == BT::Character || prev == BT::Parenthetical
                          || prev == BT::Dialogue  || prev == BT::DualDialogue;
    const bool next_speaks = next == BT::Parenthetical || next == BT::Dialogue
                          || next == BT::DualDialogue;
    return prev_speaks && next_speaks;
}

// Formatting for a block type under a given profile. `uppercase`/`right_align`
// do not vary by profile, so the single-argument default is safe for callers
// that only inspect those flags.
inline BlockFormat format_for(screenplay::BlockType t,
                              LayoutProfile profile = LayoutProfile::USLetter) {
    const bool a4 = (profile == LayoutProfile::InternationalA4);

    // Right margin inset of the active profile (used by Transition alignment).
    const float page_left  = a4 ? 95.f : 108.f;
    const float page_right = a4 ? 68.f : 72.f;

    //             left               right               before after  upper  ralign keep   widow orphan
    switch (t) {
    case screenplay::BlockType::SceneHeading:
        return { page_left,           page_right,          12.f,  12.f,  true,  false, true,  0, 0 };
    case screenplay::BlockType::Action:
        return { page_left,           page_right,          12.f,  12.f,  false, false, false, 2, 2 };
    case screenplay::BlockType::Character:
        return { a4 ? 252.f : 266.f,  a4 ? 136.f : 144.f,  12.f,   0.f,  true,  false, true,  0, 0 };
    case screenplay::BlockType::Parenthetical:
        return { a4 ? 209.f : 223.f,  a4 ? 170.f : 180.f,   0.f,   0.f,  false, false, true,  0, 0 };
    case screenplay::BlockType::Dialogue:
        return { a4 ? 166.f : 180.f,  a4 ? 136.f : 144.f,   0.f,  12.f,  false, false, false, 2, 2 };
    case screenplay::BlockType::Transition:
        return { page_left,           page_right,          12.f,  12.f,  true,  true,  true,  0, 0 };
    case screenplay::BlockType::DualDialogue:
        return { a4 ? 166.f : 180.f,  a4 ? 136.f : 144.f,   0.f,  12.f,  false, false, false, 2, 2 };
    // A shot is an instruction to the camera, so it is uppercase like a scene
    // heading and never separated from the action it introduces.
    case screenplay::BlockType::Shot:
        return { page_left,           page_right,          12.f,  12.f,  true,  false, true,  0, 0 };
    // General text behaves like Action: same column, same breaking rules.
    case screenplay::BlockType::General:
        return { page_left,           page_right,          12.f,  12.f,  false, false, false, 2, 2 };
    // An act break divides the script; it is centred and travels as one unit
    // with whatever opens the act.
    case screenplay::BlockType::ActBreak: {
        BlockFormat act { page_left, page_right, 24.f, 24.f, true, false, true, 0, 0 };
        act.center_align = true;
        return act;
    }
    }
    return { page_left, page_right, 12.f, 12.f, false, false, false, 2, 2 };
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
// Layout engine — pure function, never touches Script. Vertical rhythm is
// locked to the 12 pt baseline grid; horizontal measurement uses real Courier
// Prime glyph advances via IFontMetrics (never character counts).
// ─────────────────────────────────────────────────────────────────────────────
class LayoutEngine {
public:
    explicit LayoutEngine(const IFontMetrics& metrics,
                          LayoutProfile       profile = LayoutProfile::USLetter,
                          float               pt_size = kFontSizePt)
        : metrics_(metrics),
          profile_(profile),
          geo_(PageGeometry::for_profile(profile)),
          pt_size_(pt_size) {}

    // Explicit-geometry constructor — used by tests to inject a custom page.
    // The block indentation still comes from `profile`.
    LayoutEngine(const IFontMetrics& metrics,
                 PageGeometry        geo,
                 float               pt_size = kFontSizePt,
                 LayoutProfile       profile = LayoutProfile::USLetter)
        : metrics_(metrics), profile_(profile), geo_(geo), pt_size_(pt_size) {}

    PageList layout(const screenplay::Script& script) const;

    LayoutProfile       profile()  const { return profile_; }
    void set_profile(LayoutProfile p) {
        profile_ = p;
        geo_     = PageGeometry::for_profile(p);
    }

    const PageGeometry& geometry() const { return geo_; }
    float               pt_size()  const { return pt_size_; }

private:
    const IFontMetrics& metrics_;
    LayoutProfile       profile_;
    PageGeometry        geo_;
    float               pt_size_;
};

} // namespace screenplay::layout
