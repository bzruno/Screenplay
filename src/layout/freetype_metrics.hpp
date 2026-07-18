#pragma once
#include "font_metrics.hpp"
#include <stdexcept>
#include <string>

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

    void set_size(float pt_size) const;
};

} // namespace screenplay::layout
