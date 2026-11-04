#include "bvg/BvgFile.hpp"
#include "bvg/Codec.hpp"
#include <fstream>

namespace bvg {

void BvgFile::setChunk(const std::string& t, std::vector<uint8_t> data,
                       int codec, int level, bool usePre) {
    if (!chunks.count(t)) order.push_back(t);
    chunks[t] = std::move(data);
    meta[t] = ChunkMeta{(uint8_t)codec, (uint8_t)level, usePre};
}

std::vector<uint8_t>* BvgFile::getChunk(const std::string& t) {
    auto it = chunks.find(t);
    return it == chunks.end() ? nullptr : &it->second;
}

static std::vector<uint8_t> readFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw BvgFormatException("cannot open");
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

BvgFile BvgFile::read(const std::filesystem::path& p) {
    auto data = readFile(p);
    auto h = generated::unpackHeader(data);
    bool perChunk = (h.flags & generated::FLAG_PERCHUNK) != 0;
    size_t rec = perChunk ? generated::CHUNK_SIZE_P
                          : generated::CHUNK_SIZE_S;
    size_t tableOff = generated::HEADER_SIZE;
    size_t dataStart = tableOff + rec * h.chunkCount;

    auto entries = generated::unpackChunkTable(data, tableOff,
                                               h.chunkCount, perChunk);
    BvgFile f((int) h.width, (int) h.height);
    for (auto& e : entries) {
        std::string t((const char*) e.type, 4);
        int cid = perChunk ? e.codecId : h.codecId;
        int lv = perChunk ? e.level : h.level;
        bool pre = perChunk ? (e.flags & generated::CHUNK_PRE_RLE) != 0
                            : false;
        size_t abs = dataStart + e.offset;
        std::span<const uint8_t> comp(data.data() + abs, e.size);
        auto raw = decompress(comp, cid, lv, pre,
                              h.totalUncompressed);
        f.setChunk(t, std::move(raw), cid, lv, pre);
    }
    return f;
}

void BvgFile::write(const std::filesystem::path& p, bool perChunk,
                    bool rlePre, int defCodec, int defLevel) const {
    std::vector<generated::ChunkEntry> entries;
    std::vector<std::vector<uint8_t>> comps;
    uint64_t totalRaw = 0;
    uint32_t off = 0;
    int globalCodec = 0, globalLevel = 0;
    bool first = true;

    // preserve insertion order: use `order` if set, else chunks key order
    std::vector<std::string> ordered;
    if (!order.empty()) {
        ordered = order;
    } else {
        for (auto& kv : chunks) ordered.push_back(kv.first);
    }

    for (auto& t : ordered) {
        auto it = chunks.find(t);
        if (it == chunks.end()) continue;
        auto& raw = it->second;
        int cid = defCodec, lv = defLevel;
        bool pre = rlePre;
        auto mit = meta.find(t);
        if (perChunk && mit != meta.end()) {
            cid = mit->second.codec;
            lv = mit->second.level;
            pre = mit->second.usePre;
        } else if (!perChunk) {
            pre = false;
        }
        if (first) { globalCodec = cid; globalLevel = lv; first = false; }
        auto comp = compress(raw, cid, lv, pre);
        generated::ChunkEntry e{};
        std::memcpy(e.type, t.data(), 4);
        e.offset = off;
        e.size = (uint32_t) comp.size();
        e.codecId = (uint8_t) cid;
        e.level = (uint8_t) lv;
        e.flags = pre ? generated::CHUNK_PRE_RLE : 0;
        entries.push_back(e);
        off += comp.size();
        totalRaw += raw.size();
        comps.push_back(std::move(comp));
    }

    // SPEC 7: FLAG_U32XY is always set by writers in v1
    uint16_t flags = generated::FLAG_U32XY;
    if (perChunk) flags |= generated::FLAG_PERCHUNK;
    if (chunks.count("THMB")) flags |= generated::FLAG_HAS_THUMB;
    if (chunks.count("META")) flags |= generated::FLAG_HAS_META;

    generated::Header h;
    h.flags = flags;
    h.width = width; h.height = height;
    h.chunkCount = (uint32_t) entries.size();
    h.codecId = (uint8_t) (perChunk ? 0 : globalCodec);
    h.level = (uint8_t) (perChunk ? 0 : globalLevel);
    h.totalUncompressed = totalRaw;

    auto header = generated::packHeader(h);
    auto table = generated::packChunkTable(entries, perChunk);

    std::ofstream f(p, std::ios::binary);
    f.write((const char*) header.data(), header.size());
    f.write((const char*) table.data(), table.size());
    for (auto& c : comps)
        f.write((const char*) c.data(), c.size());
}

} // namespace bvg