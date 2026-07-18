#pragma once
#include "../layout/layout_engine.hpp"
#include "../model/model.hpp"
#include <string_view>
#include <string>

namespace screenplay::render {

// ── Color ─────────────────────────────────────────────────────────────────────
struct Color {
    uint8_t r, g, b, a = 255;

    static constexpr Color white()      { return {255,255,255,255}; }
    static constexpr Color black()      { return {  0,  0,  0,255}; }
    static constexpr Color text()       { return { 20, 20, 20,255}; }
    static constexpr Color shadow()     { return {  0,  0,  0, 50}; }
    static constexpr Color page_num()   { return {120,120,120,255}; }
    static constexpr Color selection()  { return { 41,121,255, 80}; }
    static constexpr Color caret()      { return { 33, 33,255,255}; }
    static constexpr Color suggest_bg() { return {240,240,255,255}; }
    static constexpr Color suggest_sel(){ return { 41,121,255,200}; }
    static constexpr Color canvas_bg()  { return {230,230,230,255}; }
};

// ── Abstract render target ────────────────────────────────────────────────────
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual void clear(Color c) = 0;
    virtual void fill_rect(float x, float y, float w, float h, Color c) = 0;
    virtual void draw_text(float x, float y, std::string_view text,
                           float pt_size, Color c) = 0;
    virtual void draw_line(float x1, float y1, float x2, float y2,
                           float thickness, Color c) = 0;
    virtual void push_clip(float x, float y, float w, float h) = 0;
    virtual void pop_clip() = 0;

    // Points → screen pixels (applies zoom + device DPI)
    virtual float pt_to_px(float pt) const = 0;
};

// ── Renderer config ───────────────────────────────────────────────────────────
struct RenderConfig {
    float                 zoom          = 1.0f;
    float                 page_gap_px   = 24.f;
    float                 scroll_y_px   = 0.f;
    int                   canvas_w_px   = 0;
    int                   canvas_h_px   = 0;
    screenplay::Cursor    cursor;
    bool                  show_cursor   = true;
    int                   suggestion_selected = -1;    // for popup highlight
    std::vector<std::string> suggestions;
};

// ── Stateless renderer ────────────────────────────────────────────────────────
class Renderer {
public:
    void render(IRenderTarget&                     target,
                const layout::PageList&            pages,
                const layout::PageGeometry&        geo,
                const layout::IFontMetrics&        metrics,
                float                              pt_size,
                const RenderConfig&                cfg) const
    {
        // ── Canvas background ─────────────────────────────────────────────
        target.clear(Color::canvas_bg());

        const float zoom      = cfg.zoom;
        const float page_w_px = target.pt_to_px(geo.page_w) * zoom;
        const float page_h_px = target.pt_to_px(geo.page_h) * zoom;
        const float canvas_cx = cfg.canvas_w_px * 0.5f;

        float page_top = cfg.page_gap_px - cfg.scroll_y_px;

        for (const auto& page : pages) {
            const float page_left = canvas_cx - page_w_px * 0.5f;

            // Out-of-view culling
            if (page_top + page_h_px < 0) {
                page_top += page_h_px + cfg.page_gap_px;
                continue;
            }
            if (page_top > cfg.canvas_h_px) break;

            // ── Drop shadow ───────────────────────────────────────────────
            target.fill_rect(page_left + 4, page_top + 4,
                             page_w_px, page_h_px, Color::shadow());

            // ── Page white background ─────────────────────────────────────
            target.fill_rect(page_left, page_top,
                             page_w_px, page_h_px, Color::white());

            // ── Page number (from page 2, top-right) ──────────────────────
            if (page.number > 1) {
                std::string pnum = std::to_string(page.number) + ".";
                float num_x = page_left
                    + target.pt_to_px(geo.page_w - geo.margin_right) * zoom - 36.f;
                float num_y = page_top
                    + target.pt_to_px(geo.margin_top * 0.5f) * zoom;
                target.draw_text(num_x, num_y, pnum,
                                 pt_size * zoom, Color::page_num());
            }

            // ── Clip to page ──────────────────────────────────────────────
            target.push_clip(page_left, page_top, page_w_px, page_h_px);

            for (const auto& vl : page.lines) {
                float x = page_left + vl.x * zoom;
                float y = page_top  + vl.y * zoom - target.pt_to_px(geo.margin_top) * zoom;

                // Actually: vl.y is already absolute in the page (includes margin_top)
                // Recalculate properly:
                x = page_left + vl.x * zoom;
                y = page_top  + (vl.y - geo.margin_top) * zoom + target.pt_to_px(geo.margin_top) * zoom;
                // Simplify: vl.y is page-local (already includes margin_top offset)
                x = page_left + vl.x * zoom;
                y = page_top  + vl.y * zoom;

                target.draw_text(x, y, vl.display_text,
                                 pt_size * zoom, Color::text());

                // ── Draw caret ────────────────────────────────────────────
                if (cfg.show_cursor &&
                    vl.block_idx == cfg.cursor.block_idx)
                {
                    // Determine if cursor falls on this wrapped line
                    // (simplified: draw on first line of the block)
                    if (vl.line_in_block == 0) {
                        float caret_x = x;
                        // Measure text up to cursor offset
                        std::string pre = vl.display_text.substr(
                            0, std::min(cfg.cursor.byte_offset,
                                        vl.display_text.size()));
                        auto lm = metrics.measure(pre, pt_size);
                        caret_x += lm.width * zoom;

                        float caret_h = vl.height * zoom;
                        target.draw_line(caret_x, y,
                                         caret_x, y + caret_h,
                                         1.5f, Color::caret());
                    }
                }
            }

            target.pop_clip();

            // ── Autocomplete popup ────────────────────────────────────────
            if (!cfg.suggestions.empty()) {
                draw_suggestions(target, cfg, page_left, page_top,
                                 pages, geo, pt_size, zoom);
            }

            page_top += page_h_px + cfg.page_gap_px;
        }
    }

private:
    void draw_suggestions(IRenderTarget&           target,
                          const RenderConfig&      cfg,
                          float                    page_left,
                          float                    page_top,
                          const layout::PageList&  /*pages*/,
                          const layout::PageGeometry& geo,
                          float                    pt_size,
                          float                    zoom) const
    {
        // Simple popup below caret — position heuristic
        float popup_x  = page_left + (geo.margin_left + 144) * zoom;
        float popup_y  = page_top  + (geo.margin_top + 60)   * zoom;
        float item_h   = (pt_size + 6.f) * zoom;
        float popup_w  = 200.f * zoom;
        float popup_h  = item_h * static_cast<float>(cfg.suggestions.size());

        // Background
        target.fill_rect(popup_x, popup_y, popup_w, popup_h,
                         Color::suggest_bg());
        target.draw_line(popup_x, popup_y, popup_x + popup_w, popup_y,
                         1.f, {200,200,200,255});

        for (size_t i = 0; i < cfg.suggestions.size(); ++i) {
            float iy = popup_y + static_cast<float>(i) * item_h;
            if ((int)i == cfg.suggestion_selected)
                target.fill_rect(popup_x, iy, popup_w, item_h,
                                 Color::suggest_sel());
            target.draw_text(popup_x + 6.f * zoom, iy + 2.f * zoom,
                             cfg.suggestions[i],
                             pt_size * zoom,
                             (int)i == cfg.suggestion_selected
                                 ? Color::white() : Color::text());
        }
    }
};

} // namespace screenplay::render
