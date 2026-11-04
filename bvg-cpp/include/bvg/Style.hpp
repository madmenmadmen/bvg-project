// GENERATED FILE - DO NOT EDIT
#pragma once
#include <vector>

namespace bvg {

struct Style {
    uint8_t fillKind = 0;
    uint16_t fillIdx = 0;
    uint8_t strokeKind = 0;
    uint16_t strokeIdx = 0;
    float strokeWidth = 0.0f;
    uint8_t cap = 0;
    uint8_t join = 0;
    float miterLimit = 4.0f;
    std::vector<float> dash;
    uint8_t opacity = 255;
    uint8_t blendMode = 0;
};

} // namespace bvg