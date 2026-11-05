#include "bvg/Cli.hpp"
#include "bvg/BvgFile.hpp"
#include "bvg/Codec.hpp"
#include "bvg/Generated.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <span>

namespace bvg {
namespace cli {

static std::string hex(const std::vector<uint8_t>& data) {
    std::ostringstream os;
    for (auto b : data) {
        os << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(b);
    }
    return os.str();
}

static std::string baseName(const std::string& p) {
    auto slash = p.find_last_of("/\\");
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

static long fileSize(const std::string& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return -1;
    return static_cast<long>(f.tellg());
}

static std::vector<uint8_t> readAll(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw BvgFormatException("cannot open " + p);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

static int findCodec(const std::string& name) {
    for (int i = 0; i <= 4; i++) {
        if (std::strcmp(codecName(i), name.c_str()) == 0) return i;
    }
    return -1;
}

int cmd_recompress(int argc, char** argv) {
    std::string input, output;
    std::string codecName = "auto";
    int level = 0;
    bool perChunk = true;
    bool rlePre = true;
    for (int i = 0; i < argc; i++) {
        if (std::strcmp(argv[i], "--codec") == 0 && i + 1 < argc) {
            codecName = argv[++i];
        } else if (std::strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            level = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--global-codec") == 0) {
            perChunk = false;
        } else if (std::strcmp(argv[i], "--per-chunk") == 0) {
            perChunk = true;
        } else if (std::strcmp(argv[i], "--no-rle-pre") == 0) {
            rlePre = false;
        } else if (input.empty()) {
            input = argv[i];
        } else {
            output = argv[i];
        }
    }
    if (input.empty() || output.empty()) {
        std::cerr << "usage: bvg recompress <in> <out> [--codec X] [--level N]\n";
        return 2;
    }
    int cid = -1;
    if (codecName != "auto") {
        cid = findCodec(codecName);
        if (cid < 0) {
            std::cerr << "unknown codec: " << codecName << "\n";
            return 2;
        }
    }
    try {
        auto f = BvgFile::read(input);
        for (auto& t : f.order) {
            f.meta[t] = ChunkMeta{(uint8_t)(cid >= 0 ? cid : 0),
                      (uint8_t)level, rlePre};
        }
        f.write(output, perChunk, rlePre, cid >= 0 ? cid : 0, level);
        auto inSize = std::filesystem::file_size(input);
        auto outSize = std::filesystem::file_size(output);
        std::printf("%s  %llu B\n%s  %llu B  (%.2fx smaller)\n",
                    input.c_str(), (unsigned long long)inSize,
                    output.c_str(), (unsigned long long)outSize,
                    outSize ? (double) inSize / outSize : 0.0);
        return 0;
    } catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

static int cmd_dump(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bvg-cpp-cli dump <file> [-o out.json]\n";
        return 2;
    }
    std::string in = argv[1];
    std::string out;
    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[++i];
        }
    }
    try {
        auto f = BvgFile::read(in);

        // копия порядка: getChunk мутирует chunks
        std::vector<std::string> order = f.order;

        std::ostringstream js;
        js << "{\n  \"chunks\": [\n";
        bool first = true;
        for (auto& t : order) {
            auto* raw = f.getChunk(t);
            if (!raw) continue;

            auto mit = f.meta.find(t);
            int codec   = (mit != f.meta.end()) ? mit->second.codec : 0;
            int level   = (mit != f.meta.end()) ? mit->second.level : 0;
            bool usePre = (mit != f.meta.end()) && mit->second.usePre;

            if (!first) js << ",\n";
            first = false;
            js << "    {\n";
            js << "      \"hex\": \"" << hex(*raw) << "\",\n";
            js << "      \"meta\": {\n";
            js << "        \"codec\": " << codec << ",\n";
            js << "        \"level\": " << level << ",\n";
            js << "        \"use_pre\": " << (usePre ? "true" : "false") << "\n";
            js << "      },\n";
            js << "      \"type\": \"" << t << "\"\n";
            js << "    }";
        }
        js << "\n  ],\n";
        js << "  \"file\": \"" << baseName(in) << "\",\n";
        js << "  \"height\": " << f.height << ",\n";
        js << "  \"width\": " << f.width << "\n";
        js << "}\n";

        if (!out.empty()) {
            std::ofstream o(out, std::ios::binary);
            o << js.str();
            std::cerr << "wrote " << out << "\n";
        } else {
            std::cout << js.str();
        }
        return 0;
    } catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

int cmd_info(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bvg-cpp-cli info <file>\n";
        return 2;
    }
    std::string p = argv[1];
    try {
        auto data = readAll(p);
        auto h = generated::unpackHeader(data);
        bool perChunk = (h.flags & generated::FLAG_PERCHUNK) != 0;
        bool hasThumb = (h.flags & generated::FLAG_HAS_THUMB) != 0;
        bool hasMeta  = (h.flags & generated::FLAG_HAS_META) != 0;

        std::cout << "BVG v" << h.version << "  " << p << "\n";
        std::cout << "  format kind          vector\n";
        std::cout << "  magic                b'BVG\\x00'\n";
        std::cout << "  canvas               " << h.width << " x " << h.height << "\n";
        std::cout << "  chunks               " << h.chunkCount << "\n";
        std::cout << "  global codec         " << codecName(h.codecId)
                  << " (id=" << (int)h.codecId << ", level=" << (int)h.level << ")\n";
        std::cout << "  per-chunk            " << perChunk << "\n";
        std::cout << "  has thumb            " << hasThumb << "\n";
        std::cout << "  has meta             " << hasMeta << "\n";
        std::cout << "  total uncompressed   " << h.totalUncompressed << " B\n";
        long sz = fileSize(p);
        if (sz >= 0) std::cout << "  file size            " << sz << " B\n";
        return 0;
    } catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

int cmd_list(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bvg-cpp-cli list <file>\n";
        return 2;
    }
    std::string p = argv[1];
    try {
        auto data = readAll(p);
        auto h = generated::unpackHeader(data);
        bool perChunk = (h.flags & generated::FLAG_PERCHUNK) != 0;
        auto entries = generated::unpackChunkTable(
            data, generated::HEADER_SIZE, h.chunkCount, perChunk);
        size_t rec = perChunk ? generated::CHUNK_SIZE_P
                              : generated::CHUNK_SIZE_S;
        size_t dataStart = generated::HEADER_SIZE + rec * h.chunkCount;

        std::cout << "BVG v" << h.version << "  " << p << "  "
                  << h.width << "x" << h.height << "  chunks="
                  << h.chunkCount << "  per_chunk=" << perChunk << "\n\n";

        std::printf("%-6s %-6s %4s %4s %10s %10s %9s\n",
                    "type", "codec", "lvl", "pre", "size", "raw", "comp/raw");
        std::cout << std::string(60, '-') << "\n";

        long long totalC = 0, totalR = 0;
        for (auto& e : entries) {
            std::string t((const char*)e.type, 4);
            int cid = perChunk ? e.codecId : h.codecId;
            int lv  = perChunk ? e.level  : h.level;
            bool pre = perChunk && ((e.flags & generated::CHUNK_PRE_RLE) != 0);
            std::vector<uint8_t> comp(e.size);
            std::memcpy(comp.data(), data.data() + dataStart + e.offset, e.size);
            long long rawSize = -1;
            try {
                auto raw = decompress(
                    std::span<const uint8_t>(comp.data(), comp.size()),
                    cid, lv, pre, 0);
                rawSize = (long long)raw.size();
            } catch (...) {}
            double ratio = rawSize > 0 ? (double)e.size / rawSize : 0;
            std::printf("%-6s %-6s %4d %4s %10u %10lld %8.2fx\n",
                        t.c_str(), codecName(cid), lv, pre ? "yes" : "no",
                        e.size, rawSize, ratio);
            totalC += e.size;
            if (rawSize > 0) totalR += rawSize;
        }
        std::cout << std::string(60, '-') << "\n";
        std::printf("%-6s %-6s %4s %4s %10lld %10lld %8.2fx\n",
                    "TOTAL", "", "", "", totalC, totalR,
                    totalR > 0 ? (double)totalC / totalR : 0);
        return 0;
    } catch (std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

int cmd_codecs(int, char**) {
    std::printf("%3s %-6s %-10s %8s %10s\n",
                "id", "name", "available", "default", "range");
    std::cout << std::string(42, '-') << "\n";
    for (int cid = 0; cid <= 4; cid++) {
        const char* name = codecName(cid);
        int lv = 0, lo = 0, hi = 0;
        switch (cid) {
            case 0: lv = 0; lo = 0; hi = 0;  break;
            case 1: lv = 6; lo = 1; hi = 9;  break;
            case 2: lv = 3; lo = 1; hi = 22; break;
            case 3: lv = 0; lo = 0; hi = 16; break;
            case 4: lv = 0; lo = 0; hi = 0;  break;
        }
        std::string rng = hi > 0
            ? (std::to_string(lo) + ".." + std::to_string(hi)) : "-";
        std::printf("%3d %-6s %-10s %8d %10s\n",
                    cid, name, "yes", lv, rng.c_str());
    }
    return 0;
}

int run(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bvg-cpp-cli <command> [args]\n";
        std::cerr << "commands:\n";
        std::cerr << "  info <file>              print header info\n";
        std::cerr << "  list <file>              list chunks\n";
        std::cerr << "  dump <file> [-o out]     dump contents as JSON\n";
        std::cerr << "  recompress <in> <out>    re-encode BVG file\n";
        std::cerr << "  codecs                   list codecs\n";
        std::cerr << "  --version                print version\n";
        return 2;
    }
    std::string cmd = argv[1];
    int n = argc - 1;
    char** args = argv + 1;
    if (cmd == "info")   return cmd_info(n, args);
    if (cmd == "list")   return cmd_list(n, args);
    if (cmd == "dump")   return cmd_dump(n, args);
    if (cmd == "codecs") return cmd_codecs(n, args);
    if (cmd == "recompress") return cmd_recompress(n, args);
    if (cmd == "--version" || cmd == "-V") {
        std::cout << "bvg " << generated::VERSION << ".0\n";
        return 0;
    }
    if (cmd == "--help" || cmd == "-h") {
        std::cerr << "usage: bvg-cpp-cli <command> [args]\n";
        return 0;
    }
    std::cerr << "unknown command: " << cmd << "\n";
    return 2;
}

} // namespace cli
} // namespace bvg