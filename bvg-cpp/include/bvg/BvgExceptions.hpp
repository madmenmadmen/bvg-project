// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
#pragma once
#include <stdexcept>
#include <string>

namespace bvg {

class BvgException : public std::runtime_error {
public:
    explicit BvgException(const std::string& msg) : std::runtime_error(msg) {}
};

class BvgFormatException : public BvgException {
public:
    explicit BvgFormatException(const std::string& msg) : BvgException(msg) {}
};

class BvgVersionException : public BvgFormatException {
public:
    explicit BvgVersionException(const std::string& msg)
        : BvgFormatException(msg) {}
};

class BvgCodecException : public BvgException {
public:
    explicit BvgCodecException(const std::string& msg) : BvgException(msg) {}
};

class BvgChunkException : public BvgException {
public:
    explicit BvgChunkException(const std::string& msg) : BvgException(msg) {}
};

} // namespace bvg