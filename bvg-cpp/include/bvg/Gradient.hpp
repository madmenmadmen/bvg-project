// GENERATED FILE - DO NOT EDIT
#pragma once
#include <vector>
#include <array>

namespace bvg {

struct GradientStop {
    float offset = 0.0f;
    uint16_t colorIdx = 0xFFFF;
    std::array<int, 4> rgba{0, 0, 0, 255};
};

struct Gradient {
    enum Kind : uint8_t {
        Linear = 0, Radial = 1, Conic = 2, Pattern = 3
    };
    uint8_t kind = Linear;
    std::vector<GradientStop> stops;
    std::vector<double> geometry;
    uint8_t flags = 0;
    bool hasTransform = false;
    std::array<double, 6> transform{};
};

} // namespace bvg