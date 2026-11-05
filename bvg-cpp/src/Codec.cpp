#include "bvg/Codec.hpp"
#include <cstring>
#include <algorithm>
#include <array>
#include <zlib.h>
#include <zstd.h>
#include <lz4frame.h>

namespace bvg {

static const char* NAMES[] = {"none","zlib","zstd","lz4","rle"};
static const int DEFAULT_LEVEL[] = {0,6,3,0,0};
static const int LEVEL_LO[] = {0,1,1,0,0};
static const int LEVEL_HI[] = {0,9,22,16,0};

const char* codecName(int cid) {
    return (cid >= 0 && cid <= 4) ? NAMES[cid] : "unknown";
}
int resolveLevel(int cid, int level) {
    if (cid < 0 || cid > 4) throw BvgCodecException("unknown codec");
    if (level == 0) return DEFAULT_LEVEL[cid];
    return std::max(LEVEL_LO[cid], std::min(LEVEL_HI[cid], level));
}
bool isAvailable(int cid) { return true; }

std::vector<uint8_t> rleEncode(std::span<const uint8_t> data) {
    std::vector<uint8_t> out;
    size_t i = 0, n = data.size();
    while (i < n) {
        uint8_t b = data[i];
        size_t j = i + 1;
        while (j < n && data[j] == b && (j - i) < 130) j++;
        size_t run = j - i;
        if (run >= 3) {
            out.push_back(0x80 | uint8_t(run - 3));
            out.push_back(b);
            i = j;
        } else {
            size_t start = i;
            i++;
            while (i < n && (i - start) < 128) {
                if (i + 2 < n && data[i] == data[i+1]
                    && data[i] == data[i+2]) break;
                i++;
            }
            size_t lit = i - start;
            out.push_back(uint8_t(lit - 1));
            out.insert(out.end(), data.begin() + start, data.begin() + i);
        }
    }
    return out;
}

std::vector<uint8_t> rleDecode(std::span<const uint8_t> data) {
    std::vector<uint8_t> out;
    size_t i = 0, n = data.size();
    while (i < n) {
        uint8_t h = data[i++];
        if (h & 0x80) {
            size_t run = (h & 0x7F) + 3;
            if (i >= n) throw BvgCodecException("RLE: truncated run");
            uint8_t b = data[i++];
            out.insert(out.end(), run, b);
        } else {
            size_t lit = h + 1;
            if (i + lit > n) throw BvgCodecException("RLE: truncated literal");
            out.insert(out.end(), data.begin() + i, data.begin() + i + lit);
            i += lit;
        }
    }
    return out;
}

std::vector<uint8_t> compress(std::span<const uint8_t> data,
                              int cid, int level, bool rlePre) {
    std::vector<uint8_t> work;
    if (rlePre && cid != 4) {
        work = rleEncode(data);
        data = work;
    }
    int lv = resolveLevel(cid, level);
    switch (cid) {
        case 0: return {data.begin(), data.end()};
        case 1: {
            uLongf outLen = compressBound(data.size());
            std::vector<uint8_t> out(outLen);
            int rc = ::compress2(out.data(), &outLen, data.data(),
                                 data.size(), lv);
            if (rc != Z_OK) throw BvgCodecException("zlib: compress");
            out.resize(outLen);
            return out;
        }
        case 2: {
            size_t bound = ZSTD_compressBound(data.size());
            std::vector<uint8_t> out(bound);
            size_t n = ZSTD_compress(out.data(), bound, data.data(),
                                     data.size(), lv);
            if (ZSTD_isError(n)) throw BvgCodecException("zstd: compress");
            out.resize(n);
            return out;
        }
        case 3: {
            LZ4F_preferences_t prefs{};
            prefs.compressionLevel = lv;
            size_t bound = LZ4F_compressFrameBound(data.size(), &prefs);
            std::vector<uint8_t> out(bound);
            size_t n = LZ4F_compressFrame(out.data(), bound, data.data(),
                                          data.size(), &prefs);
            if (LZ4F_isError(n)) throw BvgCodecException("lz4: compress");
            out.resize(n);
            return out;
        }
        case 4: return rleEncode(data);
    }
    throw BvgCodecException("unknown codec");
}

std::vector<uint8_t> decompress(std::span<const uint8_t> data,
                                int cid, int level, bool rlePre,
                                size_t rawHint) {
    std::vector<uint8_t> out;
    switch (cid) {
        case 0:  // none
            out.assign(data.begin(), data.end());
            break;

        case 1: {  // zlib
            z_stream zs{};
            if (inflateInit(&zs) != Z_OK)
                throw BvgCodecException("zlib: inflateInit");
            zs.next_in = const_cast<Bytef*>(data.data());
            zs.avail_in = static_cast<uInt>(data.size());
            out.clear();
            out.reserve(rawHint > 0 ? rawHint : 1024);
            std::array<uint8_t, 8192> buf{};
            int rc;
            do {
                zs.next_out = buf.data();
                zs.avail_out = static_cast<uInt>(buf.size());
                rc = inflate(&zs, Z_NO_FLUSH);
                if (rc == Z_STREAM_ERROR || rc == Z_DATA_ERROR
                    || rc == Z_MEM_ERROR || rc == Z_NEED_DICT) {
                    inflateEnd(&zs);
                    throw BvgCodecException("zlib: inflate error");
                }
                size_t produced = buf.size() - zs.avail_out;
                out.insert(out.end(), buf.data(), buf.data() + produced);
            } while (rc != Z_STREAM_END);
            inflateEnd(&zs);
            break;
        }

        case 2: {  // zstd
            unsigned long long contentSize =
                ZSTD_getFrameContentSize(data.data(), data.size());

            if (contentSize != ZSTD_CONTENTSIZE_UNKNOWN &&
                contentSize != ZSTD_CONTENTSIZE_ERROR) {
                // Точный размер известен — один вызов.
                out.resize(static_cast<size_t>(contentSize));
                size_t n = ZSTD_decompress(out.data(), out.size(),
                                           data.data(), data.size());
                if (ZSTD_isError(n))
                    throw BvgCodecException("zstd: decompress");
                out.resize(n);
                break;
            }

            ZSTD_DStream* ds = ZSTD_createDStream();
            if (!ds) throw BvgCodecException("zstd: createDStream");
            size_t init = ZSTD_initDStream(ds);
            if (ZSTD_isError(init)) {
                ZSTD_freeDStream(ds);
                throw BvgCodecException("zstd: initDStream");
            }
            out.clear();
            if (rawHint > 0) out.reserve(rawHint);
            std::vector<uint8_t> buf(ZSTD_DStreamOutSize());
            ZSTD_inBuffer in{data.data(), data.size(), 0};
            while (in.pos < in.size) {
                ZSTD_outBuffer ob{buf.data(), buf.size(), 0};
                size_t rc = ZSTD_decompressStream(ds, &ob, &in);
                if (ZSTD_isError(rc)) {
                    ZSTD_freeDStream(ds);
                    throw BvgCodecException("zstd: decompressStream");
                }
                out.insert(out.end(), buf.data(), buf.data() + ob.pos);
                if (rc == 0 && in.pos == in.size) break;  // frame complete
            }
            ZSTD_freeDStream(ds);
            break;
        }

        case 3: {  // lz4
            LZ4F_dctx* dctx = nullptr;
            size_t rc = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
            if (LZ4F_isError(rc)) {
                LZ4F_freeDecompressionContext(dctx);
                throw BvgCodecException("lz4: createDctx");
            }
            out.clear();
            if (rawHint > 0) out.reserve(rawHint);
            std::vector<uint8_t> buf(64 * 1024);
            size_t srcPos = 0;
            bool done = false;
            while (srcPos < data.size() && !done) {
                size_t dstSize = buf.size();
                size_t srcSize = data.size() - srcPos;
                rc = LZ4F_decompress(dctx, buf.data(), &dstSize,
                                     data.data() + srcPos, &srcSize, nullptr);
                if (LZ4F_isError(rc)) {
                    LZ4F_freeDecompressionContext(dctx);
                    throw BvgCodecException("lz4: decompress");
                }
                out.insert(out.end(), buf.data(), buf.data() + dstSize);
                srcPos += srcSize;
                if (rc == 0) done = true;  // frame complete
            }
            LZ4F_freeDecompressionContext(dctx);
            if (!done && srcPos < data.size())
                throw BvgCodecException("lz4: truncated frame");
            break;
        }
        case 4:
            return rleDecode(data);
        default:
            throw BvgCodecException("unknown codec");
    }
    if (rlePre && cid != 4)
        out = rleDecode(out);
    return out;
}

} // namespace bvg