#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bvg/BvgExceptions.hpp"
#include "bvg/Generated.hpp"

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
    std::vector<std::string> order;
    std::map<std::string, ChunkMeta> meta;

    BvgFile() = default;
    BvgFile(int w, int h) : width(w), height(h) {}

    static BvgFile read(const std::filesystem::path& p);
    std::vector<uint8_t>* getChunk(const std::string& t);
    void setChunk(const std::string& t, std::vector<uint8_t> data,
                  int codec, int level, bool usePre);
    void write(const std::filesystem::path& p, bool perChunk,
               bool rlePre, int defCodec, int defLevel) const;
    static BvgFile read_from_bytes(const std::vector<uint8_t>& data);

private:
    std::vector<uint8_t> raw_;
    std::vector<generated::ChunkEntry> entries_;
    size_t dataStart_ = 0;
    bool perChunk_ = false;
    uint8_t headerCodec_ = 0;
    uint8_t headerLevel_ = 0;
    mutable std::set<std::string> dirty_;
};

} // namespace bvg