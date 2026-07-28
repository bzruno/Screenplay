#pragma once
#include "font_metrics.hpp"
#include <stdexcept>
#include <string>
#include <cstdint>
#include <unordered_map>

// Forward-declare FreeType types to avoid exposing ft2build.h in the header
struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace screenplay::layout {

class FreeTypeMetrics final : public IFontMetrics {
public:
    explicit FreeTypeMetrics(const char* font_path);
    ~FreeTypeMetrics() override;

    // Non-copyable
    FreeTypeMetrics(const FreeTypeMetrics&) = delete;
    FreeTypeMetrics& operator=(const FreeTypeMetrics&) = delete;

    LineMetrics measure(std::string_view text, float pt_size) const override;

    std::vector<size_t> word_wrap(
        std::string_view text,
        float pt_size,
        float max_width) const override;

    float line_height(float pt_size) const override;

private:
    FT_LibraryRec_* library_ = nullptr;
    FT_FaceRec_*    face_    = nullptr;
    float           units_per_em_ = 1000.f;

    // Pagination re-measures the same handful of distinct characters many
    // times over (word_wrap + measure + footprint lookahead all re-scan the
    // same block text). FT_Set_Char_Size is comparatively expensive and
    // FT_Get_Char_Index/FT_Get_Advance do real glyph work — caching the
    // per-codepoint advance (invalidated only when pt_size actually changes)
    // turns a document-sized number of FreeType calls into a small, bounded
    // one (one per distinct character actually used).
    mutable float    cached_size_ = -1.f;
    mutable std::unordered_map<uint32_t, int32_t> advance_cache_;

    void   set_size(float pt_size) const;
    int32_t glyph_advance(uint32_t codepoint) const;   // 26.6-independent, matches FT_Fixed range
};

} // namespace screenplay::layout
