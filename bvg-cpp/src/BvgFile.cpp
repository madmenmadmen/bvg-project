#include "bvg/BvgFile.hpp"
#include "bvg/Codec.hpp"
#include "bvg/Generated.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <span>

namespace bvg {

static std::vector<uint8_t> readFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw BvgFormatException("cannot open");
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

BvgFile BvgFile::read(const std::filesystem::path& p) {
    BvgFile f;
    f.raw_ = readFile(p);
    auto h = generated::unpackHeader(f.raw_);
    f.perChunk_ = (h.flags & generated::FLAG_PERCHUNK) != 0;
    size_t rec = f.perChunk_ ? generated::CHUNK_SIZE_P
                             : generated::CHUNK_SIZE_S;
    size_t tableOff = generated::HEADER_SIZE;
    f.dataStart_ = tableOff + rec * h.chunkCount;
    f.entries_ = generated::unpackChunkTable(
        f.raw_, tableOff, h.chunkCount, f.perChunk_);
    f.width = (int) h.width;
    f.height = (int) h.height;
    f.headerCodec_ = h.codecId;
    f.headerLevel_ = h.level;
    for (auto& e : f.entries_) {
        std::string t((const char*) e.type, 4);
        f.order.push_back(t);
        f.meta[t] = ChunkMeta{
            (uint8_t)(f.perChunk_ ? e.codecId : h.codecId),
            (uint8_t)(f.perChunk_ ? e.level   : h.level),
            f.perChunk_ && (e.flags & generated::CHUNK_PRE_RLE) != 0,
        };
    }
    return f;
}

std::vector<uint8_t>* BvgFile::getChunk(const std::string& t) {
    auto it = chunks.find(t);
    if (it != chunks.end()) return &it->second;
    if (raw_.empty()) return nullptr;
    for (auto& e : entries_) {
        if (std::string((const char*) e.type, 4) != t) continue;
        int cid = perChunk_ ? e.codecId : headerCodec_;
        int lv  = perChunk_ ? e.level   : headerLevel_;
        bool pre = perChunk_ && (e.flags & generated::CHUNK_PRE_RLE) != 0;
        size_t abs = dataStart_ + e.offset;
        if (abs + e.size > raw_.size())
            throw BvgChunkException("chunk out of bounds");
        std::span<const uint8_t> comp(raw_.data() + abs, e.size);
        auto raw = decompress(comp, cid, lv, pre, /*rawHint=*/0);
        auto [ins, _] = chunks.emplace(t, std::move(raw));
        return &ins->second;
    }
    return nullptr;
}

void BvgFile::setChunk(const std::string& t, std::vector<uint8_t> data,
                       int codec, int level, bool usePre) {
    if (!chunks.count(t)) order.push_back(t);
    chunks[t] = std::move(data);
    meta[t] = ChunkMeta{(uint8_t)codec, (uint8_t)level, usePre};
    dirty_.insert(t);
}

void BvgFile::write(const std::filesystem::path& p, bool perChunk,
                    bool rlePre, int defCodec, int defLevel) const {
    std::vector<generated::ChunkEntry> entries;
    std::vector<std::vector<uint8_t>> comps;
    uint64_t totalRaw = 0;
    uint32_t off = 0;
    int globalCodec = 0, globalLevel = 0;
    bool first = true;

    std::vector<std::string> ordered;
    if (!order.empty()) {
        ordered = order;
    } else {
        for (auto& kv : chunks) ordered.push_back(kv.first);
    }

    for (auto& t : ordered) {
        std::vector<uint8_t> comp;
        int cid, lv;
        bool pre;

        if (dirty_.count(t) == 0 && !raw_.empty()) {
            // Копируем байт-в-байт из исходного файла.
            auto ent = std::find_if(entries_.begin(), entries_.end(),
                [&](const generated::ChunkEntry& e) {
                    return std::string((const char*) e.type, 4) == t;
                });
            if (ent == entries_.end()) continue;
            size_t abs = dataStart_ + ent->offset;
            comp.assign(raw_.begin() + abs, raw_.begin() + abs + ent->size);
            cid = perChunk_ ? ent->codecId : headerCodec_;
            lv  = perChunk_ ? ent->level   : headerLevel_;
            pre = perChunk_ && (ent->flags & generated::CHUNK_PRE_RLE) != 0;
            // rawLen для total_uncompressed — декомпрессим только для подсчёта.
            std::span<const uint8_t> cs(comp.data(), comp.size());
            auto decoded = decompress(cs, cid, lv, pre, 0);
            totalRaw += decoded.size();
        } else {
            // Кодируем из chunks[t].
            auto it = chunks.find(t);
            if (it == chunks.end()) continue;
            auto& rawPayload = it->second;
            cid = defCodec; lv = defLevel; pre = rlePre;
            auto mit = meta.find(t);
            if (perChunk && mit != meta.end()) {
                cid = mit->second.codec;
                lv  = mit->second.level;
                pre = mit->second.usePre;
            } else if (!perChunk) {
                pre = false;
            }
            comp = compress(rawPayload, cid, lv, pre);
            totalRaw += rawPayload.size();
        }

        if (first) { globalCodec = cid; globalLevel = lv; first = false; }
        generated::ChunkEntry e{};
        std::memcpy(e.type, t.data(), 4);
        e.offset = off;
        e.size = (uint32_t) comp.size();
        e.codecId = (uint8_t) cid;
        e.level = (uint8_t) lv;
        e.flags = pre ? generated::CHUNK_PRE_RLE : 0;
        entries.push_back(e);
        off += comp.size();
        comps.push_back(std::move(comp));
    }

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