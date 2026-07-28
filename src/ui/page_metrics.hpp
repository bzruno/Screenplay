#pragma once
// Where the paginated script lands on screen.
//
// Painting, hit-testing, scrolling, the caret badge and the SmartType popup
// all need the same answer to "which pixel is this line on?". Each used to
// work it out again from dpi, zoom, page size, the 40px gap and whether a
// cover precedes page one — five copies of one formula, free to drift apart.

#include "../layout/layout_engine.hpp"

#include <QRectF>
#include <algorithm>
#include <optional>

namespace screenplay::ui {

/// The visible area the pages are drawn into.
struct Viewport {
    float width     = 0.f;
    float height    = 0.f;
    float dpi_scale = 1.f;   // logicalDpiX() / 72
    float zoom      = 1.f;
    float scroll_y  = 0.f;
};

class PageMetrics {
public:
    /// Vertical space between sheets, and above the first one.
    static constexpr float kGapPx = 40.f;

    PageMetrics(const layout::PageGeometry& paper, const Viewport& view,
                bool has_cover)
        : paper_(paper), view_(view), has_cover_(has_cover) {}

    /// Layout points to screen pixels.
    float scale()       const { return view_.dpi_scale * view_.zoom; }
    float page_width()  const { return paper_.page_w * scale(); }
    float page_height() const { return paper_.page_h * scale(); }
    float gap()         const { return kGapPx; }

    /// Every sheet is centred, so they share one left edge.
    float left() const { return view_.width * .5f - page_width() * .5f; }

    /// Page-space x (points) to viewport x.
    float x_of(float layout_x) const { return left() + layout_x * scale(); }

    // ── Vertical placement ────────────────────────────────────────────────

    /// Distance from the document's top to page `index` (0-based, script
    /// pages only). Independent of scrolling.
    float document_page_top(size_t index) const {
        return kGapPx + cover_offset() + (float)index * page_step();
    }

    /// Viewport y of the top of page `index`.
    float page_top(size_t index) const {
        return document_page_top(index) - view_.scroll_y;
    }

    QRectF page_rect(size_t index) const {
        return QRectF(left(), page_top(index), page_width(), page_height());
    }

    float document_height(size_t page_count) const {
        return kGapPx + cover_offset() + (float)page_count * page_step();
    }

    /// Furthest the document can usefully scroll, so the last page cannot be
    /// pushed off the top of an empty viewport.
    float max_scroll(size_t page_count) const {
        if (page_count == 0) return 0.f;
        return std::max(0.f, document_height(page_count) - view_.height);
    }

    // ── Lines ─────────────────────────────────────────────────────────────

    float line_top(size_t page_index, const layout::VisualLine& line) const {
        return page_top(page_index) + line.y * scale();
    }
    float line_height(const layout::VisualLine& line) const {
        return line.height * scale();
    }

    /// A real line of the script, located on screen. `page_index` is the page
    /// it was found on, so callers can test it against that page's bounds.
    struct Placement {
        size_t                     page_index;
        float                      top;       // viewport y
        float                      height;
        const layout::VisualLine*  line;

        float bottom() const { return top + height; }
    };

    /// First line satisfying `matches`, skipping the virtual "(MORE)" and
    /// "(CONT'D)" lines that carry no text of their own. Empty when the
    /// layout holds no such line — an unpaginated or empty script.
    template <class Predicate>
    std::optional<Placement> find_line(const layout::PageList& pages,
                                       Predicate matches) const {
        for (size_t index = 0; index < pages.size(); ++index)
            for (const auto& line : pages[index].lines) {
                if (line.is_more || line.is_contd) continue;
                if (!matches(line)) continue;
                return Placement{ index, line_top(index, line),
                                  line_height(line), &line };
            }
        return std::nullopt;
    }

    /// Which page the viewport point `y` falls on, if any.
    std::optional<size_t> page_at(float y, size_t page_count) const {
        for (size_t index = 0; index < page_count; ++index) {
            const float top = page_top(index);
            if (y >= top && y <= top + page_height()) return index;
            if (top > view_.height) break;      // the rest are below the fold
        }
        return std::nullopt;
    }

private:
    float page_step()    const { return page_height() + kGapPx; }
    float cover_offset() const { return has_cover_ ? page_step() : 0.f; }

    layout::PageGeometry paper_;
    Viewport             view_;
    bool                 has_cover_ = false;
};

} // namespace screenplay::ui
