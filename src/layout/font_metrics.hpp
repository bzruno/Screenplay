#pragma once
#include <string_view>
#include <vector>

namespace screenplay::layout {

struct LineMetrics {
    float width;
    float height;
    float ascent;
    float descent;
};

class IFontMetrics {
public:
    virtual ~IFontMetrics() = default;

    virtual LineMetrics measure(std::string_view text, float pt_size) const = 0;

    virtual std::vector<size_t> word_wrap(
        std::string_view text,
        float pt_size,
        float max_width) const = 0;

    virtual float line_height(float pt_size) const = 0;
};

} // namespace screenplay::layout
