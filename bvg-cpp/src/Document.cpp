#include "bvg/Document.hpp"
#include "bvg/Codec.hpp"
#include <unordered_map>

namespace bvg {

static int parseHex(const std::string& s) {
    std::string h = s[0] == '#' ? s.substr(1) : s;
    if (h.size() == 3) {
        std::string e;
        for (char c : h) { e.push_back(c); e.push_back(c); }
        h = e;
    }
    return std::stoi(h, nullptr, 16);
}

int Document::colorIdx(const std::string& hex) {
    int v = parseHex(hex);
    std::array<int,4> c{(v>>16)&0xFF, (v>>8)&0xFF, v&0xFF, 255};
    for (size_t i = 0; i < palette_.size(); i++)
        if (palette_[i] == c) return (int) i;
    palette_.push_back(c);
    return (int) palette_.size() - 1;
}

int Document::gradIdx(const Gradient& g) {
    for (size_t i = 0; i < grads_.size(); i++)
        if (grads_[i].kind == g.kind
            && grads_[i].stops.size() == g.stops.size())
            return (int) i;
    grads_.push_back(g);
    return (int) grads_.size() - 1;
}

int Document::styleIdx(int fillKind, int fillIdx, int opacity) {
    for (size_t i = 0; i < styles_.size(); i++) {
        auto& s = styles_[i];
        if (s.fillKind == fillKind && s.fillIdx == fillIdx
            && s.opacity == opacity) return (int) i;
    }
    Style s;
    s.fillKind = (uint8_t) fillKind;
    s.fillIdx = (uint16_t) fillIdx;
    s.opacity = (uint8_t) opacity;
    styles_.push_back(s);
    return (int) styles_.size() - 1;
}

Document& Document::rect(double x, double y, double w, double h,
                         const std::string& fill) {
    int ci = colorIdx(fill);
    int si = styleIdx(1, ci, 255);
    Node nd;
    nd.type = Node::Rect;
    nd.rect = {x, y, w, h};
    nd.style = (uint16_t) si;
    nodes_.push_back(nd);
    return *this;
}

Document& Document::rect(double x, double y, double w, double h,
                         const Gradient& g) {
    return rect(x, y, w, h, g, 255);
}

Document& Document::rect(double x, double y, double w, double h,
                         const Gradient& g, int opacity) {
    int gi = gradIdx(g);
    int si = styleIdx(2, gi, opacity);
    Node nd;
    nd.type = Node::Rect;
    nd.rect = {x, y, w, h};
    nd.style = (uint16_t) si;
    nodes_.push_back(nd);
    return *this;
}

BvgFile Document::toBvgFile() const {
    BvgFile f(width, height);
    if (!palette_.empty())
        f.setChunk("COLR", generated::packColr(palette_), 0, 0, false);
    if (!grads_.empty())
        f.setChunk("GRAD", generated::packGrad(grads_), 0, 0, false);
    if (!styles_.empty())
        f.setChunk("STYL", generated::packStyl(styles_), 0, 0, false);
    // SPEC 3.2: NODE is always present (possibly empty)
    f.setChunk("NODE", generated::packNode(nodes_), 0, 0, false);
    return f;
}

void Document::save(const std::filesystem::path& p,
                    int codec, int level) const {
    auto f = toBvgFile();
    f.write(p, true, true, codec, level);
}

} // namespace bvg