#include "layout_engine.hpp"
#include <QString>   // Unicode-aware toUpper
#include <algorithm>
#include <cctype>

namespace screenplay::layout {

PageList LayoutEngine::layout(const screenplay::Script& script) const {
    PageList pages;
    Page     current_page;
    current_page.number = 1;   // ← start at page 1 immediately (no flush trick)
    float cursor_y = 0.f;
    int   page_number = 1;

    const float lh      = metrics_.line_height(pt_size_);
    const float print_h = geo_.printable_h();
    const float print_w = geo_.printable_w();

    // Open a new page and reset cursor
    auto new_page = [&] {
        pages.push_back(std::move(current_page));
        current_page        = {};
        current_page.number = ++page_number;
        cursor_y            = 0.f;
    };

    // Helper: how many lines does a block need?
    auto block_line_count = [&](size_t bi) -> size_t {
        const auto& b   = script.blocks[bi];
        const auto  fmt = format_for(b.type);
        float col_w = print_w - fmt.left_indent - fmt.right_indent;
        auto breaks = metrics_.word_wrap(b.text, pt_size_, col_w);
        return breaks.size() + 1;
    };

    for (size_t bi = 0; bi < script.blocks.size(); ++bi) {
        const auto& block = script.blocks[bi];
        const auto  fmt   = format_for(block.type);

        // ── DualDialogue: render two columns side-by-side ─────────────────
        if (block.type == screenplay::BlockType::DualDialogue) {
            const float half_w = (print_w - 36.f) * 0.5f;   // ~190 pt per col, 36 pt gap

            // Helper: wrap one DualDialogue block text into lines
            auto wrap_col = [&](const std::string& txt,
                                std::vector<std::string>& out_lines,
                                std::vector<size_t>&      out_starts) {
                auto brks = metrics_.word_wrap(txt, pt_size_, half_w);
                size_t off = 0;
                for (size_t br : brks) {
                    out_lines.push_back(txt.substr(off, br - off));
                    out_starts.push_back(off);
                    off = br;
                }
                if (off < txt.size()) {
                    out_lines.push_back(txt.substr(off));
                    out_starts.push_back(off);
                }
                if (out_lines.empty()) { out_lines.emplace_back(); out_starts.push_back(0); }
            };

            std::vector<std::string> left_lines; std::vector<size_t> left_starts;
            wrap_col(block.text, left_lines, left_starts);

            // Page break before the pair
            if (cursor_y + fmt.space_before * spacing_factor_ + lh > print_h) new_page();
            cursor_y += fmt.space_before * spacing_factor_;
            float pair_top_y = cursor_y;

            // Emit left column
            for (size_t li = 0; li < left_lines.size(); ++li) {
                if (cursor_y + lh > print_h) { new_page(); pair_top_y = cursor_y; }
                float x = geo_.margin_left;
                float y = geo_.margin_top + pair_top_y + (float)li * lh;
                size_t end_off = (li + 1 < left_starts.size())
                    ? left_starts[li + 1] : block.text.size();
                current_page.lines.push_back({
                    bi, li, left_lines[li], x, y,
                    metrics_.measure(left_lines[li], pt_size_).width, lh,
                    left_starts[li], end_off });
            }

            size_t right_nlines = 0;

            // Emit right column if paired
            if (bi + 1 < script.blocks.size() &&
                script.blocks[bi + 1].type == screenplay::BlockType::DualDialogue) {
                const auto& rblk = script.blocks[bi + 1];
                std::vector<std::string> right_lines; std::vector<size_t> right_starts;
                wrap_col(rblk.text, right_lines, right_starts);
                right_nlines = right_lines.size();
                float rx = geo_.margin_left + half_w + 36.f;
                for (size_t li = 0; li < right_lines.size(); ++li) {
                    float y = geo_.margin_top + pair_top_y + (float)li * lh;
                    size_t end_off = (li + 1 < right_starts.size())
                        ? right_starts[li + 1] : rblk.text.size();
                    current_page.lines.push_back({
                        bi + 1, li, right_lines[li], rx, y,
                        metrics_.measure(right_lines[li], pt_size_).width, lh,
                        right_starts[li], end_off });
                }
                ++bi;   // skip right block; for-loop ++bi makes total +2
            }

            cursor_y = pair_top_y
                + lh * (float)std::max(left_lines.size(), right_nlines)
                + fmt.space_after * spacing_factor_;
            continue;
        }

        // ── Build display text ────────────────────────────────────────────
        std::string display = block.text;
        if (fmt.uppercase)
            display = QString::fromStdString(display).toUpper().toStdString();

        // ── Word-wrap ─────────────────────────────────────────────────────
        float col_w = print_w - fmt.left_indent - fmt.right_indent;
        auto  breaks = metrics_.word_wrap(display, pt_size_, col_w);

        // Build wrapped lines and record the byte offset where each line starts
        // (relative to display / block.text — same byte structure for Latin UTF-8).
        std::vector<std::string> wrapped;
        std::vector<size_t>      line_starts;   // start byte offset of each wrapped line
        {
            size_t off = 0;
            for (size_t br : breaks) {
                wrapped.push_back(display.substr(off, br - off));
                line_starts.push_back(off);
                off = br;
            }
            if (off < display.size()) {
                wrapped.push_back(display.substr(off));
                line_starts.push_back(off);
            }
            if (wrapped.empty()) {
                wrapped.emplace_back();
                line_starts.push_back(0);
            }
        }

        // ── Page-break decision ───────────────────────────────────────────
        // Rule: Character must never be separated from its Dialogue/Parenthetical
        bool keep_with_next =
            (block.type == screenplay::BlockType::Character)
            && (bi + 1 < script.blocks.size())
            && (script.blocks[bi + 1].type == screenplay::BlockType::Dialogue ||
                script.blocks[bi + 1].type == screenplay::BlockType::Parenthetical ||
                script.blocks[bi + 1].type == screenplay::BlockType::DualDialogue);

        if (keep_with_next && bi + 1 < script.blocks.size()) {
            // Require space for Character + first line of Dialogue on same page
            size_t next_lines = block_line_count(bi + 1);
            const auto& nfmt  = format_for(script.blocks[bi + 1].type);
            float next_needed = nfmt.space_before * spacing_factor_
                                + lh * static_cast<float>(std::min(next_lines, size_t(2)))
                                + nfmt.space_after * spacing_factor_;
            if (cursor_y + fmt.space_before * spacing_factor_ + lh + next_needed > print_h)
                new_page();
        } else {
            // Normal block: break if first line doesn't fit
            if (cursor_y + fmt.space_before * spacing_factor_ + lh > print_h)
                new_page();
        }

        // ── Emit space_before ─────────────────────────────────────────────
        cursor_y += fmt.space_before;

        // ── Emit wrapped lines ────────────────────────────────────────────
        for (size_t li = 0; li < wrapped.size(); ++li) {
            // Widow prevention / page break with MORE-CONT'D for dialogue
            if (cursor_y + lh > print_h) {
                bool is_dia = (block.type == screenplay::BlockType::Dialogue
                            || block.type == screenplay::BlockType::Parenthetical);
                std::string char_name_contd;
                if (is_dia && li > 0) {
                    for (int ci = (int)bi - 1; ci >= 0; --ci) {
                        if (script.blocks[ci].type == screenplay::BlockType::Character) {
                            char_name_contd =
                                QString::fromStdString(script.blocks[ci].text)
                                    .toUpper().toStdString();
                            break;
                        }
                    }
                }
                if (!char_name_contd.empty()) {
                    // Insert "(MORE)" at the bottom of the current page
                    VisualLine more_vl;
                    more_vl.block_idx     = bi;
                    more_vl.line_in_block = wrapped.size();   // sentinel: not editable
                    more_vl.display_text  = "(MORE)";
                    more_vl.x             = geo_.margin_left + 144.f; // Character indent
                    more_vl.y             = geo_.margin_top
                                           + std::min(cursor_y, print_h - lh);
                    more_vl.width         = 0.f;
                    more_vl.height        = lh;
                    more_vl.start_offset  = display.size();   // past-end sentinel
                    more_vl.end_offset    = display.size();
                    more_vl.is_more       = true;
                    current_page.lines.push_back(more_vl);
                }
                new_page();
                if (!char_name_contd.empty()) {
                    // Insert "CHARNAME (CONT'D)" at the top of the new page
                    std::string contd_text = char_name_contd + " (CONT'D)";
                    VisualLine contd_vl;
                    contd_vl.block_idx     = bi;
                    contd_vl.line_in_block = wrapped.size();   // sentinel
                    contd_vl.display_text  = contd_text;
                    contd_vl.x             = geo_.margin_left + 144.f;
                    contd_vl.y             = geo_.margin_top + cursor_y; // cursor_y == 0
                    contd_vl.width         = 0.f;
                    contd_vl.height        = lh;
                    contd_vl.start_offset  = display.size();
                    contd_vl.end_offset    = display.size();
                    contd_vl.is_contd      = true;
                    current_page.lines.push_back(contd_vl);
                    cursor_y += lh;
                }
            }

            float x = geo_.margin_left + fmt.left_indent;
            float y = geo_.margin_top  + cursor_y;

            if (fmt.right_align) {
                auto lm = metrics_.measure(wrapped[li], pt_size_);
                x = geo_.page_w - geo_.margin_right - lm.width;
            }

            size_t line_end = (li + 1 < line_starts.size())
                ? line_starts[li + 1]
                : display.size();

            current_page.lines.push_back({
                bi,
                li,
                wrapped[li],
                x, y,
                metrics_.measure(wrapped[li], pt_size_).width,
                lh,
                line_starts[li],
                line_end
            });
            cursor_y += lh;
        }

        cursor_y += fmt.space_after * spacing_factor_;
    }

    // Always flush last page (even if empty — shows blank page 1 on new doc)
    pages.push_back(std::move(current_page));

    // Safety: if nothing was added at all, ensure at least one page
    if (pages.empty()) {
        Page p; p.number = 1;
        pages.push_back(std::move(p));
    }

    return pages;
}

} // namespace screenplay::layout
