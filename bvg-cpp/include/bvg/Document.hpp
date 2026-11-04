#pragma once
#include <string>
#include <vector>
#include "BvgFile.hpp"
#include "Gradient.hpp"
#include "Style.hpp"
#include "Node.hpp"

namespace bvg {

class Document {
public:
    int width, height;
    Document(int w, int h) : width(w), height(h) {}

    Document& rect(double x, double y, double w, double h,
                   const std::string& fill);
    Document& rect(double x, double y, double w, double h,
                   const Gradient& g);
    Document& rect(double x, double y, double w, double h,
                   const Gradient& g, int opacity);

    BvgFile toBvgFile() const;
    void save(const std::filesystem::path& p, int codec, int level) const;

private:
    std::vector<std::array<int,4>> palette_;
    std::vector<Gradient> grads_;
    std::vector<Style> styles_;
    std::vector<Node> nodes_;
    int colorIdx(const std::string& hex);
    int gradIdx(const Gradient& g);
    int styleIdx(int fillKind, int fillIdx, int opacity);
};

} // namespace bvg