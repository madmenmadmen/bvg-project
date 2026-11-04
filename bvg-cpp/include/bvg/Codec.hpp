#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "Generated.hpp"

namespace bvg {

enum class CodecId : uint8_t {
    None = 0, Zlib = 1, Zstd = 2, Lz4 = 3, Rle = 4
};

const char* codecName(int cid);
int resolveLevel(int cid, int level);
bool isAvailable(int cid);

std::vector<uint8_t> compress(std::span<const uint8_t> data,
                              int cid, int level, bool rlePre);
std::vector<uint8_t> decompress(std::span<const uint8_t> data,
                                int cid, int level, bool rlePre,
                                size_t rawHint);

std::vector<uint8_t> rleEncode(std::span<const uint8_t> data);
std::vector<uint8_t> rleDecode(std::span<const uint8_t> data);

} // namespace bvg