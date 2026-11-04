// GENERATED FILE - DO NOT EDIT
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bvg {

struct PathCmd {
    uint8_t op = 0;
    std::vector<double> args;
};

struct Node {
    enum Type : uint8_t {
        Group = 0, Rect = 1, Ellipse = 2, Line = 3,
        Polyline = 4, Polygon = 5, Path = 6, Text = 7,
        Image = 8, Use = 9, Def = 10
    };
    enum Flags : uint8_t { HasTransform = 1 };

    uint8_t type = 0;
    uint8_t flags = 0;
    uint16_t style = 0xFFFF;
    uint16_t depth = 0;
    bool hasTransform = false;
    std::array<double, 6> transform{};

    // payload
    uint16_t childCount = 0;
    std::array<double, 4> rect{};
    std::array<double, 4> ellipse{};
    std::array<double, 4> line{};
    std::vector<std::array<double, 2>> points;
    std::vector<PathCmd> path;
    uint16_t font = 0;
    double tx = 0, ty = 0;
    std::string text;
    uint16_t imageIdx = 0;
    uint16_t defIdx = 0;
};

} // namespace bvg