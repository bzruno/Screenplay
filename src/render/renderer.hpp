#pragma once
// Configuration the canvas paints from. The renderer itself is Qt code in
// ScreenplayCanvas: an abstract render target was tried and dropped, because
// the page look depends on QPainter throughout.

#include "../model/model.hpp"
#include <string>
#include <vector>

namespace screenplay::render {

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

} // namespace screenplay::render
