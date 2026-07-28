#include "freetype_metrics.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

#include "../utf8_utils.hpp"
#include <algorithm>
#include <stdexcept>

namespace screenplay::layout {

FreeTypeMetrics::FreeTypeMetrics(const char* font_path) {
    if (FT_Init_FreeType(&library_))
        throw std::runtime_error("FreeType: init failed");
    if (FT_New_Face(library_, font_path, 0, &face_))
        throw std::runtime_error(std::string("FreeType: could not load font: ") + font_path);
    units_per_em_ = static_cast<float>(face_->units_per_EM);
}

FreeTypeMetrics::~FreeTypeMetrics() {
    if (face_)    FT_Done_Face(face_);
    if (library_) FT_Done_FreeType(library_);
}

void FreeTypeMetrics::set_size(float pt_size) const {
    if (cached_size_ == pt_size) return;   // already at this size — nothing to do
    cached_size_ = pt_size;
    advance_cache_.clear();   // advances scale with size; stale entries would be wrong
    // 72 DPI — caller applies screen DPI scale externally
    FT_Set_Char_Size(face_,
        static_cast<FT_F26Dot6>(pt_size * 64), 0,
        72, 72);
}

int32_t FreeTypeMetrics::glyph_advance(uint32_t cp) const {
    auto it = advance_cache_.find(cp);
    if (it != advance_cache_.end()) return it->second;
    FT_UInt  glyph_idx = FT_Get_Char_Index(face_, cp);
    FT_Fixed advance   = 0;
    FT_Get_Advance(face_, glyph_idx, FT_LOAD_NO_HINTING, &advance);
    const int32_t a = static_cast<int32_t>(advance);
    advance_cache_.emplace(cp, a);
    return a;
}

LineMetrics FreeTypeMetrics::measure(std::string_view text, float pt_size) const {
    set_size(pt_size);

    float width = 0.f;
    size_t i = 0;
    while (i < text.size()) {
        size_t cp_len;
        uint32_t cp = screenplay::utf8::decode(text, i, cp_len);
        if (cp_len == 0) break;

        width += static_cast<float>(glyph_advance(cp) >> 16);
        i += cp_len;
    }

    float scale   = pt_size / units_per_em_;
    float ascent  =  static_cast<float>(face_->ascender)  * scale;
    float descent = -static_cast<float>(face_->descender) * scale; // positive
    return { width, ascent + descent, ascent, descent };
}

float FreeTypeMetrics::line_height(float pt_size) const {
    set_size(pt_size);
    float scale = pt_size / units_per_em_;
    return static_cast<float>(face_->height) * scale;
}

std::vector<size_t> FreeTypeMetrics::word_wrap(
    std::string_view text,
    float /*pt_size*/,       // advances are resolution-independent here
    float max_width) const
{
    std::vector<size_t> breaks;

    size_t last_space = std::string_view::npos;
    float  line_w     = 0.f;

    size_t i = 0;
    while (i < text.size()) {
        size_t cp_len;
        uint32_t cp = screenplay::utf8::decode(text, i, cp_len);
        if (cp_len == 0) break;

        if (cp == '\n') {
            breaks.push_back(i + cp_len);
            last_space = std::string_view::npos;
            line_w = 0.f;
            i += cp_len;
            continue;
        }

        float glyph_w = static_cast<float>(glyph_advance(cp) >> 16);
        line_w += glyph_w;

        if (cp == ' ') last_space = i;

        if (line_w > max_width) {
            size_t break_at = (last_space != std::string_view::npos)
                ? last_space + 1
                : i;
            breaks.push_back(break_at);

            // Remeasure from break_at up to and including current codepoint
            line_w = 0.f;
            for (size_t j = break_at; j < i + cp_len; ) {
                size_t jlen;
                uint32_t jcp = screenplay::utf8::decode(text, j, jlen);
                if (jlen == 0) break;
                line_w += static_cast<float>(glyph_advance(jcp) >> 16);
                j += jlen;
            }

            last_space = std::string_view::npos;
        }

        i += cp_len;
    }
    return breaks;
}

} // namespace screenplay::layout
