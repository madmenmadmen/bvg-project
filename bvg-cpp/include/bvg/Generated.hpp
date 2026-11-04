// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <span>
#include <array>

#include "BvgExceptions.hpp"
#include "Gradient.hpp"
#include "Style.hpp"
#include "Node.hpp"

namespace bvg {
namespace generated {

constexpr uint32_t VERSION = 1;
constexpr size_t HEADER_SIZE = 32;
constexpr size_t CHUNK_SIZE_S = 12;
constexpr size_t CHUNK_SIZE_P = 15;

constexpr uint16_t FLAG_PERCHUNK = 1u << 0;
constexpr uint16_t FLAG_U32XY = 1u << 1;
constexpr uint16_t FLAG_HAS_THUMB = 1u << 2;
constexpr uint16_t FLAG_HAS_META = 1u << 3;

constexpr uint8_t CHUNK_PRE_RLE = 1u << 0;

// ---- little-endian helpers ----

inline void put_u16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8));
}
inline void put_u32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) v.push_back(uint8_t(x >> (i * 8)));
}
inline void put_u64(std::vector<uint8_t>& v, uint64_t x) {
    for (int i = 0; i < 8; i++) v.push_back(uint8_t(x >> (i * 8)));
}
inline void put_f32(std::vector<uint8_t>& v, float x) {
    uint32_t u; std::memcpy(&u, &x, 4);
    for (int i = 0; i < 4; i++) v.push_back(uint8_t(u >> (i * 8)));
}

inline uint16_t get_u16(std::span<const uint8_t> b, size_t& p) {
    uint16_t x = b[p] | (b[p+1] << 8); p += 2; return x;
}
inline uint32_t get_u32(std::span<const uint8_t> b, size_t& p) {
    uint32_t x = 0;
    for (int i = 0; i < 4; i++) x |= uint32_t(b[p+i]) << (i * 8);
    p += 4; return x;
}
inline uint64_t get_u64(std::span<const uint8_t> b, size_t& p) {
    uint64_t x = 0;
    for (int i = 0; i < 8; i++) x |= uint64_t(b[p+i]) << (i * 8);
    p += 8; return x;
}
inline float get_f32(std::span<const uint8_t> b, size_t& p) {
    uint32_t u = get_u32(b, p);
    float f; std::memcpy(&f, &u, 4); return f;
}

inline void need(std::span<const uint8_t> b, size_t p, size_t n,
                 const char* ctx) {
    if (p + n > b.size())
        throw BvgChunkException(std::string(ctx) + ": truncated");
}

// ---- Header ----

struct Header {
    uint8_t magic[4] = {'B','V','G',0};
    uint16_t version = VERSION;
    uint16_t flags = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t chunkCount = 0;
    uint8_t codecId = 0;
    uint8_t level = 0;
    uint16_t reserved = 0;
    uint64_t totalUncompressed = 0;
};

inline std::vector<uint8_t> packHeader(const Header& h) {
    std::vector<uint8_t> v; v.reserve(HEADER_SIZE);
    v.insert(v.end(), h.magic, h.magic + 4);
    put_u16(v, h.version);
    put_u16(v, h.flags);
    put_u32(v, h.width);
    put_u32(v, h.height);
    put_u32(v, h.chunkCount);
    v.push_back(h.codecId);
    v.push_back(h.level);
    put_u16(v, h.reserved);
    put_u64(v, h.totalUncompressed);
    return v;
}

inline Header unpackHeader(std::span<const uint8_t> b) {
    if (b.size() < HEADER_SIZE)
        throw BvgFormatException("file too small for BVG header");
    Header h;
    size_t p = 0;
    if (b[0] != 'B' || b[1] != 'V' || b[2] != 'G' || b[3] != 0)
        throw BvgFormatException("bad magic");
    p = 4;
    h.version = get_u16(b, p);
    if (h.version != VERSION)
        throw BvgVersionException("unsupported version");
    h.flags = get_u16(b, p);
    h.width = get_u32(b, p);
    h.height = get_u32(b, p);
    h.chunkCount = get_u32(b, p);
    h.codecId = b[p++];
    h.level = b[p++];
    h.reserved = get_u16(b, p);
    if (h.reserved != 0)
        throw BvgFormatException("reserved must be zero");
    h.totalUncompressed = get_u64(b, p);
    return h;
}

// ---- Chunk table ----

struct ChunkEntry {
    uint8_t type[4]{};
    uint32_t offset = 0;
    uint32_t size = 0;
    uint8_t codecId = 0;
    uint8_t level = 0;
    uint8_t flags = 0;
};

inline std::vector<uint8_t> packChunkTable(
        const std::vector<ChunkEntry>& es, bool perChunk) {
    size_t rec = perChunk ? CHUNK_SIZE_P : CHUNK_SIZE_S;
    std::vector<uint8_t> v; v.reserve(es.size() * rec);
    for (auto& e : es) {
        v.insert(v.end(), e.type, e.type + 4);
        put_u32(v, e.offset);
        put_u32(v, e.size);
        if (perChunk) {
            v.push_back(e.codecId);
            v.push_back(e.level);
            v.push_back(e.flags);
        }
    }
    return v;
}

inline std::vector<ChunkEntry> unpackChunkTable(
        std::span<const uint8_t> b, size_t off,
        uint32_t count, bool perChunk) {
    size_t rec = perChunk ? CHUNK_SIZE_P : CHUNK_SIZE_S;
    if (off + size_t(count) * rec > b.size())
        throw BvgFormatException("chunk table truncated");
    std::vector<ChunkEntry> out; out.reserve(count);
    size_t p = off;
    for (uint32_t i = 0; i < count; i++) {
        ChunkEntry e;
        for (int k = 0; k < 4; k++) e.type[k] = b[p++];
        e.offset = get_u32(b, p);
        e.size = get_u32(b, p);
        if (perChunk) {
            e.codecId = b[p++];
            e.level = b[p++];
            e.flags = b[p++];
        }
        out.push_back(e);
    }
    return out;
}

// ---- COLR ----

inline std::vector<uint8_t> packColr(
        const std::vector<std::array<int,4>>& colors) {
    std::vector<uint8_t> v; v.reserve(2 + colors.size()*4);
    put_u16(v, (uint16_t) colors.size());
    for (auto& c : colors) {
        v.push_back((uint8_t) c[0]);
        v.push_back((uint8_t) c[1]);
        v.push_back((uint8_t) c[2]);
        v.push_back((uint8_t) c[3]);
    }
    return v;
}

inline std::vector<std::array<int,4>> unpackColr(
        std::span<const uint8_t> b) {
    need(b, 0, 2, "COLR");
    size_t p = 0;
    uint16_t n = get_u16(b, p);
    need(b, p, size_t(n)*4, "COLR");
    std::vector<std::array<int,4>> out; out.reserve(n);
    for (uint16_t i = 0; i < n; i++) {
        out.push_back({b[p], b[p+1], b[p+2], b[p+3]}); p += 4;
    }
    return out;
}

// ---- GRAD ----

inline std::vector<uint8_t> packGrad(
        const std::vector<Gradient>& gs) {
    std::vector<uint8_t> v; v.reserve(2);
    put_u16(v, (uint16_t) gs.size());
    for (auto& g : gs) {
        uint8_t flags = (uint8_t)(g.flags | (g.hasTransform ? 1 : 0));
        v.push_back(g.kind);
        v.push_back(flags);
        put_u16(v, (uint16_t) g.stops.size());
        for (auto& s : g.stops) {
            put_f32(v, s.offset);
            put_u16(v, s.colorIdx);
            if (s.colorIdx == 0xFFFF) {
                v.push_back((uint8_t) s.rgba[0]);
                v.push_back((uint8_t) s.rgba[1]);
                v.push_back((uint8_t) s.rgba[2]);
                v.push_back((uint8_t) s.rgba[3]);
            }
        }
        if (g.kind == Gradient::Linear) {
            for (int i = 0; i < 4; i++) put_f32(v, (float) g.geometry[i]);
        } else if (g.kind == Gradient::Radial) {
            for (int i = 0; i < 5; i++) put_f32(v, (float) g.geometry[i]);
        } else if (g.kind == Gradient::Conic) {
            for (int i = 0; i < 4; i++) put_f32(v, (float) g.geometry[i]);
        } else if (g.kind == Gradient::Pattern) {
            put_u16(v, (uint16_t) (int) g.geometry[0]);
            for (int i = 1; i < 5; i++) put_f32(v, (float) g.geometry[i]);
        } else {
            throw BvgChunkException("GRAD: bad kind");
        }
        if (g.hasTransform) {
            for (double t : g.transform) put_f32(v, (float) t);
        }
    }
    return v;
}

inline std::vector<Gradient> unpackGrad(std::span<const uint8_t> b) {
    need(b, 0, 2, "GRAD");
    size_t p = 0;
    uint16_t n = get_u16(b, p);
    std::vector<Gradient> out; out.reserve(n);
    for (uint16_t i = 0; i < n; i++) {
        need(b, p, 4, "GRAD");
        Gradient g;
        g.kind = b[p++];
        uint8_t flags = b[p++];
        g.flags = flags;
        uint16_t sc = get_u16(b, p);
        for (uint16_t j = 0; j < sc; j++) {
            need(b, p, 6, "GRAD");
            GradientStop st;
            st.offset = get_f32(b, p);
            st.colorIdx = get_u16(b, p);
            if (st.colorIdx == 0xFFFF) {
                need(b, p, 4, "GRAD");
                st.rgba = {b[p], b[p+1], b[p+2], b[p+3]}; p += 4;
            }
            g.stops.push_back(st);
        }
        if (g.kind == Gradient::Linear || g.kind == Gradient::Conic) {
            need(b, p, 16, "GRAD");
            g.geometry.resize(4);
            for (int k = 0; k < 4; k++) g.geometry[k] = get_f32(b, p);
        } else if (g.kind == Gradient::Radial) {
            need(b, p, 20, "GRAD");
            g.geometry.resize(5);
            for (int k = 0; k < 5; k++) g.geometry[k] = get_f32(b, p);
        } else if (g.kind == Gradient::Pattern) {
            need(b, p, 18, "GRAD");
            g.geometry.resize(5);
            g.geometry[0] = get_u16(b, p);
            for (int k = 1; k < 5; k++) g.geometry[k] = get_f32(b, p);
        } else {
            throw BvgChunkException("GRAD: bad kind");
        }
        if (flags & 1) {
            need(b, p, 24, "GRAD");
            g.hasTransform = true;
            for (int k = 0; k < 6; k++) g.transform[k] = get_f32(b, p);
        }
        out.push_back(std::move(g));
    }
    return out;
}

// ---- STYL ----

inline std::vector<uint8_t> packStyl(const std::vector<Style>& ss) {
    std::vector<uint8_t> v;
    put_u16(v, (uint16_t) ss.size());
    for (auto& s : ss) {
        v.push_back(s.fillKind);
        put_u16(v, s.fillIdx);
        v.push_back(s.strokeKind);
        put_u16(v, s.strokeIdx);
        put_f32(v, s.strokeWidth);
        v.push_back(s.cap);
        v.push_back(s.join);
        put_f32(v, s.miterLimit);
        v.push_back((uint8_t) s.dash.size());
        for (float d : s.dash) put_f32(v, d);
        v.push_back(s.opacity);
        v.push_back(s.blendMode);
    }
    return v;
}

inline std::vector<Style> unpackStyl(std::span<const uint8_t> b) {
    need(b, 0, 2, "STYL");
    size_t p = 0;
    uint16_t n = get_u16(b, p);
    std::vector<Style> out; out.reserve(n);
    for (uint16_t i = 0; i < n; i++) {
        need(b, p, 16, "STYL");
        Style s;
        s.fillKind = b[p++];
        s.fillIdx = get_u16(b, p);
        s.strokeKind = b[p++];
        s.strokeIdx = get_u16(b, p);
        s.strokeWidth = get_f32(b, p);
        s.cap = b[p++];
        s.join = b[p++];
        s.miterLimit = get_f32(b, p);
        uint8_t dc = b[p++];
        need(b, p, size_t(dc) * 4 + 2, "STYL");
        s.dash.resize(dc);
        for (int k = 0; k < dc; k++) s.dash[k] = get_f32(b, p);
        s.opacity = b[p++];
        s.blendMode = b[p++];
        out.push_back(std::move(s));
    }
    return out;
}

// ---- NODE ----

inline std::vector<uint8_t> packNode(const std::vector<Node>& ns) {
    std::vector<uint8_t> v;
    for (auto& nd : ns) {
        uint8_t flags = nd.hasTransform ? Node::HasTransform : 0;
        v.push_back(nd.type);
        v.push_back(flags);
        put_u16(v, nd.style);
        put_u16(v, nd.depth);
        if (flags == Node::HasTransform) {
            for (double t : nd.transform) put_f32(v, (float) t);
        }
        switch (nd.type) {
            case Node::Group:
            case Node::Def:
                put_u16(v, nd.childCount);
                break;
            case Node::Rect:
                for (double x : nd.rect) put_f32(v, (float) x);
                break;
            case Node::Ellipse:
                for (double x : nd.ellipse) put_f32(v, (float) x);
                break;
            case Node::Line:
                for (double x : nd.line) put_f32(v, (float) x);
                break;
            case Node::Polyline:
            case Node::Polygon: {
                put_u16(v, (uint16_t) nd.points.size());
                for (auto& pt : nd.points) {
                    put_f32(v, (float) pt[0]);
                    put_f32(v, (float) pt[1]);
                }
                break;
            }
            case Node::Path: {
                put_u16(v, (uint16_t) nd.path.size());
                for (auto& c : nd.path) {
                    v.push_back(c.op);
                    if (c.op == 4) {
                        if (c.args.size() != 7)
                            throw BvgChunkException("NODE: arc args must be 7");
                        put_f32(v, (float) c.args[0]);
                        put_f32(v, (float) c.args[1]);
                        put_f32(v, (float) c.args[2]);
                        v.push_back((uint8_t) c.args[3]);
                        v.push_back((uint8_t) c.args[4]);
                        put_f32(v, (float) c.args[5]);
                        put_f32(v, (float) c.args[6]);
                    } else {
                        for (double a : c.args) put_f32(v, (float) a);
                    }
                }
                break;
            }
            case Node::Text: {
                put_u16(v, nd.font);
                put_f32(v, (float) nd.tx);
                put_f32(v, (float) nd.ty);
                put_u16(v, (uint16_t) nd.text.size());
                v.insert(v.end(), nd.text.begin(), nd.text.end());
                break;
            }
            case Node::Image:
                put_u16(v, nd.imageIdx);
                for (double x : nd.rect) put_f32(v, (float) x);
                break;
            case Node::Use:
                put_u16(v, nd.defIdx);
                put_f32(v, (float) nd.tx);
                put_f32(v, (float) nd.ty);
                break;
            default:
                throw BvgChunkException("NODE: bad type");
        }
    }
    return v;
}

inline std::vector<Node> unpackNode(std::span<const uint8_t> b) {
    std::vector<Node> out;
    size_t p = 0;
    while (p < b.size()) {
        need(b, p, 6, "NODE");
        Node nd;
        nd.type = b[p++];
        nd.flags = b[p++];
        nd.style = get_u16(b, p);
        nd.depth = get_u16(b, p);
        if (nd.flags & Node::HasTransform) {
            need(b, p, 24, "NODE");
            nd.hasTransform = true;
            for (int k = 0; k < 6; k++) nd.transform[k] = get_f32(b, p);
        }
        switch (nd.type) {
            case Node::Group:
            case Node::Def:
                need(b, p, 2, "NODE");
                nd.childCount = get_u16(b, p);
                break;
            case Node::Rect:
                need(b, p, 16, "NODE");
                for (int k = 0; k < 4; k++) nd.rect[k] = get_f32(b, p);
                break;
            case Node::Ellipse:
                need(b, p, 16, "NODE");
                for (int k = 0; k < 4; k++) nd.ellipse[k] = get_f32(b, p);
                break;
            case Node::Line:
                need(b, p, 16, "NODE");
                for (int k = 0; k < 4; k++) nd.line[k] = get_f32(b, p);
                break;
            case Node::Polyline:
            case Node::Polygon: {
                need(b, p, 2, "NODE");
                uint16_t n = get_u16(b, p);
                need(b, p, size_t(n) * 8, "NODE");
                nd.points.resize(n);
                for (uint16_t i = 0; i < n; i++) {
                    nd.points[i][0] = get_f32(b, p);
                    nd.points[i][1] = get_f32(b, p);
                }
                break;
            }
            case Node::Path: {
                need(b, p, 2, "NODE");
                uint16_t n = get_u16(b, p);
                nd.path.resize(n);
                for (uint16_t i = 0; i < n; i++) {
                    need(b, p, 1, "NODE");
                    PathCmd c;
                    c.op = b[p++];
                    if (c.op == 4) {
                        need(b, p, 22, "NODE");
                        c.args.resize(7);
                        c.args[0] = get_f32(b, p);
                        c.args[1] = get_f32(b, p);
                        c.args[2] = get_f32(b, p);
                        c.args[3] = b[p++];
                        c.args[4] = b[p++];
                        c.args[5] = get_f32(b, p);
                        c.args[6] = get_f32(b, p);
                    } else {
                        int argc = c.op == 0 || c.op == 1 ? 2 :
                                   c.op == 2 ? 4 :
                                   c.op == 3 ? 6 : 0;
                        need(b, p, size_t(argc) * 4, "NODE");
                        c.args.resize(argc);
                        for (int k = 0; k < argc; k++) c.args[k] = get_f32(b, p);
                    }
                    nd.path[i] = std::move(c);
                }
                break;
            }
            case Node::Text: {
                need(b, p, 12, "NODE");
                nd.font = get_u16(b, p);
                nd.tx = get_f32(b, p);
                nd.ty = get_f32(b, p);
                uint16_t len = get_u16(b, p);
                need(b, p, len, "NODE");
                nd.text.assign((const char*) &b[p], len);
                p += len;
                break;
            }
            case Node::Image:
                need(b, p, 18, "NODE");
                nd.imageIdx = get_u16(b, p);
                for (int k = 0; k < 4; k++) nd.rect[k] = get_f32(b, p);
                break;
            case Node::Use:
                need(b, p, 10, "NODE");
                nd.defIdx = get_u16(b, p);
                nd.tx = get_f32(b, p);
                nd.ty = get_f32(b, p);
                break;
            default:
                throw BvgChunkException("NODE: bad type");
        }
        out.push_back(std::move(nd));
    }
    return out;
}

// ---------- IMGS ----------

constexpr uint8_t IMG_PNG = 0;
constexpr uint8_t IMG_JPEG = 1;
constexpr uint8_t IMG_RAW_RGBA = 2;

struct Image {
    uint8_t format = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> data;
};

inline std::vector<Image> unpackImages(std::span<const uint8_t> b) {
    need(b, 0, 2, "IMGS");
    size_t p = 0;
    uint16_t n = get_u16(b, p);
    std::vector<Image> out; out.reserve(n);
    for (uint16_t i = 0; i < n; ++i) {
        need(b, p, 9, "IMGS");
        Image im;
        im.format = b[p++];
        im.width = get_u16(b, p);
        im.height = get_u16(b, p);
        uint32_t dlen = get_u32(b, p);
        need(b, p, dlen, "IMGS");
        im.data.assign(b.begin() + p, b.begin() + p + dlen);
        p += dlen;
        out.push_back(std::move(im));
    }
    return out;
}

} // namespace generated
} // namespace bvg