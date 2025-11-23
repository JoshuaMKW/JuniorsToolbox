#pragma once

#include <vector>
#include <stdint.h>
#include <string>
#include <vector>

using u32 = uint32_t;
using u16 = uint16_t;
using u8  = uint8_t;
using s32 = int32_t;
using s16 = int16_t;
using s8  = int8_t;

constexpr u32 roundDown(u32 in, u32 align) { return align ? in & ~(align - 1) : in; };
constexpr u32 roundUp(u32 in, u32 align) {
    return align ? roundDown(in + (align - 1), align) : in;
};

namespace librii::szs {

    bool isDataYaz0Compressed(const std::vector<u8> &src);

    std::string getLastError();

    u32 getWorstEncodingSize(u32 src);
    u32 getWorstEncodingSize(const std::vector<u8> &src);
    u32 getWorstEncodingSize(const std::string &src);

    u32 getExpandedSize(const std::vector<u8> &src);

    bool decode(std::vector<u8> &dst, const std::vector<u8> &src);
    // For streaming purposes
    bool decode(std::string &dst, const std::vector<u8> &src);
    bool decodeFirstChunk(std::vector<u8> &dst, const std::vector<u8> &src_chunk);

    std::vector<u8> encode(const std::vector<u8> &buf);
    std::vector<u8> encodeFast(const std::vector<u8> &src);

    int encodeBoyerMooreHorspool(const u8 *src, u8 *dst, int srcSize);

}  // namespace rlibrii::szs
