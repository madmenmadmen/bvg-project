#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "bvg/BvgExceptions.hpp"

namespace bvg {

struct ChunkMeta {
    uint8_t codec = 0;
    uint8_t level = 0;
    bool usePre = false;
};

class BvgFile {
public:
    int width = 0;
    int height = 0;
    std::map<std::string, std::vector<uint8_t>> chunks;
    std::vector<std::string> order;          // ← добавить
    std::map<std::string, ChunkMeta> meta;

    BvgFile() = default;
    BvgFile(int w, int h) : width(w), height(h) {}

    void setChunk(const std::string& t, std::vector<uint8_t> data,
                  int codec, int level, bool usePre);
    std::vector<uint8_t>* getChunk(const std::string& t);

    static BvgFile read(const std::filesystem::path& p);
    void write(const std::filesystem::path& p, bool perChunk,
               bool rlePre, int defCodec, int defLevel) const;
};

} // namespace bvg