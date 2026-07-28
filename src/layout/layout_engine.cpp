#include "layout_engine.hpp"
#include "../parsing/screenplay_parse.hpp"
#include <QString>   // Unicode-aware toUpper
#include <algorithm>
#include <cmath>

namespace screenplay::layout {

namespace {
std::string to_upper(const std::string& s) {
    return QString::fromStdString(s).toUpper().toStdString();
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Vertical rhythm is locked to the 12 pt baseline grid: every line advances by
// exactly kLineHeightPt and every inter-block gap is a multiple of the grid, so
// no fractional positions are ever produced. Horizontal measurement uses real
// Courier Prime glyph advances (via IFontMetrics) — never character counts.
// ─────────────────────────────────────────────────────────────────────────────
PageList LayoutEngine::layout(const screenplay::Script& script) const {
    const float lh      = kLineHeightPt;
    const float print_h = geo_.printable_h();

    PageList pages;
    Page  current_page;
    current_page.number = 1;
    int   page_number   = 1;
    float cursor_y      = 0.f;    // offset below margin_top
    float prev_after    = 0.f;    // space_after of the previous block (margin collapse)
    // Type of the previous block, so a wrylie in the middle of a speech can
    // be set tight against the line it interrupts (see continues_speech).
    auto  prev_type     = screenplay::BlockType::Action;
    bool  first_on_page = true;

    auto new_page = [&] {
        pages.push_back(std::move(current_page));
        current_page        = {};
        current_page.number = ++page_number;
        cursor_y            = 0.f;
        first_on_page       = true;
    };

    // Wrap display text into lines, recording each line's start byte offset.
    auto wrap = [&](const std::string& display, float col_w,
                    std::vector<std::string>& lines,
                    std::vector<size_t>& starts) {
        auto breaks = metrics_.word_wrap(display, pt_size_, col_w);
        size_t off = 0;
        for (size_t br : breaks) {
            lines.push_back(display.substr(off, br - off));
            starts.push_back(off);
            off = br;
        }
        if (off < display.size()) {
            lines.push_back(display.substr(off));
            starts.push_back(off);
        }
        if (lines.empty()) { lines.emplace_back(); starts.push_back(0); }
    };

    // How many wrapped lines a block occupies (footprint estimation).
    auto line_count = [&](size_t bi) -> int {
        const auto& b   = script.blocks[bi];
        const auto  fmt = format_for(b.type, profile_);
        std::string disp = fmt.uppercase ? to_upper(b.text) : b.text;
        return (int)metrics_.word_wrap(disp, pt_size_, fmt.col_width(geo_)).size() + 1;
    };

    // Uppercased NAME (without any Character Extension) of the Character block
    // that owns the dialogue at/before bi — so a "(V.O.)" cue continues as
    // "JOÃO (CONT'D)", never "JOÃO (V.O.) (CONT'D)".
    auto owning_character = [&](size_t bi) -> std::string {
        for (int ci = (int)bi; ci >= 0; --ci)
            if (script.blocks[ci].type == screenplay::BlockType::Character)
                return to_upper(parse::parse_character_cue(script.blocks[ci].text).name);
        return {};
    };

    const BlockFormat char_fmt =
        format_for(screenplay::BlockType::Character, profile_);

    for (size_t bi = 0; bi < script.blocks.size(); ++bi) {
        const auto& block = script.blocks[bi];
        auto fmt = format_for(block.type, profile_);

        // "FADE IN:" is the one Transition the industry convention sets flush
        // LEFT (it opens the script/scene, unlike CUT TO:/DISSOLVE TO:/
        // FADE OUT., which close one and stay right-aligned).
        if (block.type == screenplay::BlockType::Transition) {
            std::string t = parse::fold(parse::trim(block.text));
            while (!t.empty() && (t.back() == ':' || t.back() == '.' || t.back() == ' '))
                t.pop_back();
            if (t == "fade in") fmt.right_align = false;
        }

        // An explicit author override wins over both the type default and the
        // FADE IN heuristic above — it is the writer's stated intent, so it is
        // applied last.
        if (block.align != screenplay::BlockAlign::Default &&
                supports_alignment(block.type)) {
            fmt.right_align  = (block.align == screenplay::BlockAlign::Right);
            fmt.center_align = (block.align == screenplay::BlockAlign::Center);
        }

        // ── DualDialogue: two columns, treated as one unbreakable unit ────
        if (block.type == screenplay::BlockType::DualDialogue) {
            constexpr float gutter = 18.f;
            const float half_w = (geo_.printable_w() - gutter) * 0.5f;

            std::vector<std::string> lcol, rcol;
            std::vector<size_t>      lst,  rst;
            wrap(block.text, half_w, lcol, lst);

            const bool has_right =
                bi + 1 < script.blocks.size() &&
                script.blocks[bi + 1].type == screenplay::BlockType::DualDialogue;
            const screenplay::Block* rblk =
                has_right ? &script.blocks[bi + 1] : nullptr;
            if (rblk) wrap(rblk->text, half_w, rcol, rst);

            const int rows =
                (int)std::max(lcol.size(), rcol.size());

            float gap = (first_on_page || continues_speech(prev_type, block.type))
                            ? 0.f
                            : std::max(prev_after, fmt.space_before);
            if (!first_on_page && cursor_y + gap + rows * lh > print_h) {
                new_page();
                gap = 0.f;
            }
            cursor_y += gap;
            const float top = cursor_y;

            const float lx = geo_.margin_left;
            for (size_t li = 0; li < lcol.size(); ++li) {
                size_t end = (li + 1 < lst.size()) ? lst[li + 1] : block.text.size();
                current_page.lines.push_back({
                    bi, li, lcol[li],
                    lx, geo_.margin_top + top + (float)li * lh,
                    metrics_.measure(lcol[li], pt_size_).width, lh,
                    lst[li], end });
            }
            if (rblk) {
                const float rx = geo_.margin_left + half_w + gutter;
                for (size_t li = 0; li < rcol.size(); ++li) {
                    size_t end = (li + 1 < rst.size()) ? rst[li + 1]
                                                       : rblk->text.size();
                    current_page.lines.push_back({
                        bi + 1, li, rcol[li],
                        rx, geo_.margin_top + top + (float)li * lh,
                        metrics_.measure(rcol[li], pt_size_).width, lh,
                        rst[li], end });
                }
                ++bi;   // consume the right-hand block
            }
            cursor_y      = top + rows * lh;
            prev_after    = fmt.space_after;
            prev_type     = block.type;
            first_on_page = false;
            continue;
        }

        // ── Normal block ──────────────────────────────────────────────────
        const std::string display = fmt.uppercase ? to_upper(block.text)
                                                   : block.text;
        std::vector<std::string> wrapped;
        std::vector<size_t>      starts;
        wrap(display, fmt.col_width(geo_), wrapped, starts);
        const int nlines = (int)wrapped.size();

        // Emit helpers (capture the block's wrapped state by reference).
        auto emit_line = [&](int li) {
            float x = fmt.left;
            if (fmt.right_align) {
                auto lm = metrics_.measure(wrapped[li], pt_size_);
                x = geo_.page_w - fmt.right - lm.width;
            } else if (fmt.center_align) {
                // Centred within this block's OWN text column, not the page —
                // so a centred Action still respects the screenplay's margins.
                auto lm = metrics_.measure(wrapped[li], pt_size_);
                x = fmt.left + (fmt.col_width(geo_) - lm.width) * 0.5f;
            }
            size_t line_end = (li + 1 < (int)starts.size()) ? starts[li + 1]
                                                            : display.size();
            current_page.lines.push_back({
                bi, (size_t)li, wrapped[li],
                x, geo_.margin_top + cursor_y,
                metrics_.measure(wrapped[li], pt_size_).width, lh,
                starts[li], line_end });
            cursor_y += lh;
        };
        auto emit_continuation = [&](bool contd, const std::string& name) {
            VisualLine v;
            v.block_idx     = bi;
            v.line_in_block = (size_t)nlines;   // sentinel: not user-editable
            v.display_text  = contd ? (name + " (CONT'D)") : std::string("(MORE)");
            v.x             = char_fmt.left;    // aligned to the Character column
            v.y             = geo_.margin_top + cursor_y;
            v.width         = 0.f;
            v.height        = lh;
            v.start_offset  = display.size();
            v.end_offset    = display.size();
            v.is_more       = !contd;
            v.is_contd      = contd;
            current_page.lines.push_back(v);
            if (contd) cursor_y += lh;
        };

        // Vertical gap before this block (collapsed margins).
        float gap = (first_on_page || continues_speech(prev_type, block.type))
                        ? 0.f
                        : std::max(prev_after, fmt.space_before);

        // An author-forced page break wins over every fitting rule below: it
        // states where the reader turns the page, which pagination cannot
        // infer. Skipped when the page is already fresh, so forcing a break on
        // a block that happens to land at the top never yields a blank sheet.
        if (block.page_break_before && !first_on_page) {
            new_page();
            gap = 0.f;
        }

        // Only Action and Dialogue may break across a page; every other block
        // is placed as an atomic unit (keep-with-next protects the cues).
        const bool splittable =
            block.type == screenplay::BlockType::Action ||
            block.type == screenplay::BlockType::Dialogue;

        // A cue opening a dialogue must carry enough dialogue with it that the
        // continuation can either finish or split legally: up to 2 lines (one
        // line of dialogue + room for "(MORE)"), fewer if the dialogue is short.
        auto open_lines = [&](size_t di) -> float {
            return (float)std::min(line_count(di), 2) * lh;
        };

        // Mandatory footprint of a keep-with-next unit: the block plus the
        // companion lines that must never be split away from it.
        float footprint = nlines * lh;
        if (fmt.keep_with_next && bi + 1 < script.blocks.size()) {
            const auto ntype = script.blocks[bi + 1].type;
            const auto nfmt  = format_for(ntype, profile_);
            if (block.type == screenplay::BlockType::Character) {
                if (ntype == screenplay::BlockType::Parenthetical) {
                    footprint += std::max(fmt.space_after, nfmt.space_before)
                               + line_count(bi + 1) * lh;
                    if (bi + 2 < script.blocks.size() &&
                        script.blocks[bi + 2].type == screenplay::BlockType::Dialogue) {
                        const auto dfmt =
                            format_for(screenplay::BlockType::Dialogue, profile_);
                        footprint += std::max(nfmt.space_after, dfmt.space_before)
                                   + open_lines(bi + 2);
                    }
                } else if (ntype == screenplay::BlockType::Dialogue ||
                           ntype == screenplay::BlockType::DualDialogue) {
                    footprint += std::max(fmt.space_after, nfmt.space_before)
                               + open_lines(bi + 1);
                }
            } else {
                // Scene Heading / Transition / Parenthetical: + first next line.
                footprint += std::max(fmt.space_after, nfmt.space_before) + lh;
            }
        }

        // Decide whether the block starts on this page or is pushed to the next.
        if (!first_on_page) {
            const float avail_h = print_h - cursor_y - gap;
            bool push;
            if (!splittable) {
                push = footprint > avail_h;      // atomic unit must fit whole
            } else {
                const bool whole_fits = nlines * lh <= avail_h;
                // Min lines to legally OPEN a split here: dialogue needs a line
                // plus its "(MORE)"; action needs its orphan count.
                const int  min_open =
                    std::min(nlines,
                             block.type == screenplay::BlockType::Dialogue
                                 ? 2 : fmt.orphan);
                push = !whole_fits && (min_open * lh > avail_h);
            }
            if (push) { new_page(); gap = 0.f; }
        }
        cursor_y += gap;

        // ── Place wrapped lines, splitting across pages where the rules allow.
        int placed = 0;
        while (placed < nlines) {
            const int avail     = (int)std::floor((print_h - cursor_y) / lh + 1e-4f);
            const int remaining = nlines - placed;

            if (avail >= remaining) {
                for (int i = 0; i < remaining; ++i) emit_line(placed + i);
                placed = nlines;
                break;
            }

            const bool is_dialogue =
                block.type == screenplay::BlockType::Dialogue ||
                block.type == screenplay::BlockType::Parenthetical;

            // Dialogue splits with the (MORE)/(CONT'D) convention (min 1 line
            // each side). Action splits with a 2-line widow/orphan guard.
            const int min_here = is_dialogue ? 1 : fmt.orphan;
            const int min_next = is_dialogue ? 1 : fmt.widow;

            int take = avail;
            if (is_dialogue) take -= 1;                      // room for "(MORE)"
            if (remaining - take < min_next) take = remaining - min_next;

            if (take < min_here) {
                // Can't satisfy the minimum here → carry the rest to a new page.
                new_page();
                continue;
            }

            for (int i = 0; i < take; ++i) emit_line(placed + i);
            placed += take;

            if (is_dialogue) {
                const std::string name = owning_character(bi);
                if (!name.empty()) emit_continuation(false, {});
                new_page();
                if (!name.empty()) emit_continuation(true, name);
            } else {
                new_page();
            }
        }

        prev_after    = fmt.space_after;
        prev_type     = block.type;
        first_on_page = false;
    }

    pages.push_back(std::move(current_page));

    if (pages.empty()) {
        Page p; p.number = 1;
        pages.push_back(std::move(p));
    }
    return pages;
}

} // namespace screenplay::layout
