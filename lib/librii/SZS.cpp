// The following code was taken from https://github.com/riidefi/RiiStudio and modified

#include "SZS.hpp"

#include <algorithm>
#include <vector>

static std::string s_last_error = "";

namespace librii::szs {

    std::vector<u8> encode(const std::vector<u8> &buf) {
        std::vector<u8> tmp(getWorstEncodingSize(buf));
        int sz = encodeBoyerMooreHorspool(buf.data(), tmp.data(), buf.size());
        if (sz < 0 || sz > tmp.size()) {
            s_last_error = "encodeBoyerMooreHorspool failed";
            return {};
        }
        s_last_error.clear();
        tmp.resize(sz);
        return tmp;
    }

    std::vector<u8> encode(const std::string &buf) {
        std::vector<u8> tmp(getWorstEncodingSize(buf));
        int sz = encodeBoyerMooreHorspool((u8*)buf.data(), tmp.data(), buf.size());
        if (sz < 0 || sz > tmp.size()) {
            s_last_error = "encodeBoyerMooreHorspool failed";
            return {};
        }
        s_last_error.clear();
        tmp.resize(sz);
        return tmp;
    }

    u32 getExpandedSize(const std::vector<u8> &src) {
        if (src.size() < 8) {
            s_last_error = "File too small to be a YAZ0 file";
            return 0;
        }

        if (!(src[0] == 'Y' && src[1] == 'a' && src[2] == 'z' && src[3] == '0')) {
            s_last_error = "Data is not a YAZ0 file";
            return 0;
        }

        s_last_error.clear();
        return (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
    }

    bool decode(std::vector<u8> &dst, const std::vector<u8> &src) {
        u32 exp = getExpandedSize(src);
        if (exp == 0) {
            s_last_error = "Source is not a SZS compressed file!";
            return false;
        }
        if (dst.size() < exp) {
            s_last_error = "Result buffer is too small!";
            return false;
        }

        int in_position  = 0x10;
        int out_position = 0;

        const auto take8 = [&]() {
            const u8 result = src[in_position];
            ++in_position;
            return result;
        };
        const auto take16 = [&]() {
            const u32 high = src[in_position];
            ++in_position;
            const u32 low = src[in_position];
            ++in_position;
            return (high << 8) | low;
        };
        const auto give8 = [&](u8 val) {
            dst[out_position] = val;
            ++out_position;
        };

        const auto read_group = [&](bool raw) {
            if (raw) {
                give8(take8());
                return;
            }

            const u16 group   = take16();
            const int reverse = (group & 0xfff) + 1;
            const int g_size  = group >> 12;

            int size = g_size ? g_size + 2 : take8() + 18;

            // Invalid data could buffer overflow

            for (int i = 0; i < size; ++i) {
                give8(dst[out_position - reverse]);
            }
        };

        const auto read_chunk = [&]() {
            const u8 header = take8();

            for (int i = 0; i < 8; ++i) {
                if (in_position >= src.size() || out_position >= dst.size())
                    return;
                read_group(header & (1 << (7 - i)));
            }
        };

        while (in_position < src.size() && out_position < dst.size())
            read_chunk();

        // if (out_position < dst.size())
        //   return llvm::createStringError(
        //       std::errc::executable_format_error,
        //       "Truncated source file: the file could not be decompressed fully");

        // if (in_position != src.size())
        //   return llvm::createStringError(
        //       std::errc::no_buffer_space,
        //       "Invalid YAZ0 header: file is larger than reported");

        s_last_error.clear();
        return true;
    }

    bool decode(std::string &dst, const std::vector<u8> &src) {
        u32 exp = getExpandedSize(src);
        if (exp == 0) {
            s_last_error = "Source is not a SZS compressed file!";
            return false;
        }
        if (dst.size() < exp) {
            s_last_error = "Result buffer is too small!";
            return false;
        }

        int in_position  = 0x10;
        int out_position = 0;

        const auto take8 = [&]() {
            const u8 result = src[in_position];
            ++in_position;
            return result;
        };
        const auto take16 = [&]() {
            const u32 high = src[in_position];
            ++in_position;
            const u32 low = src[in_position];
            ++in_position;
            return (high << 8) | low;
        };
        const auto give8 = [&](u8 val) {
            dst[out_position] = val;
            ++out_position;
        };

        const auto read_group = [&](bool raw) {
            if (raw) {
                give8(take8());
                return;
            }

            const u16 group   = take16();
            const int reverse = (group & 0xfff) + 1;
            const int g_size  = group >> 12;

            int size = g_size ? g_size + 2 : take8() + 18;

            // Invalid data could buffer overflow

            for (int i = 0; i < size; ++i) {
                give8(dst[out_position - reverse]);
            }
        };

        const auto read_chunk = [&]() {
            const u8 header = take8();

            for (int i = 0; i < 8; ++i) {
                if (in_position >= src.size() || out_position >= dst.size())
                    return;
                read_group(header & (1 << (7 - i)));
            }
        };

        while (in_position < src.size() && out_position < dst.size())
            read_chunk();

        // if (out_position < dst.size())
        //   return llvm::createStringError(
        //       std::errc::executable_format_error,
        //       "Truncated source file: the file could not be decompressed fully");

        // if (in_position != src.size())
        //   return llvm::createStringError(
        //       std::errc::no_buffer_space,
        //       "Invalid YAZ0 header: file is larger than reported");

        s_last_error.clear();
        return true;
    }

    bool decodeFirstChunk(std::vector<u8> &dst, const std::vector<u8> &src) {
        auto exp = getExpandedSize(src);
        if (!exp) {
            s_last_error = "Source is not a SZS compressed file!";
            return false;
        }

        int in_position  = 0x10;
        int out_position = 0;

        const auto take8 = [&]() {
            const u8 result = src[in_position];
            ++in_position;
            return result;
        };
        const auto take16 = [&]() {
            const u32 high = src[in_position];
            ++in_position;
            const u32 low = src[in_position];
            ++in_position;
            return (high << 8) | low;
        };
        const auto give8 = [&](u8 val) {
            dst.emplace_back(val);
            ++out_position;
        };

        const auto read_group = [&](bool raw) {
            if (raw) {
                give8(take8());
                return;
            }

            const u16 group   = take16();
            const int reverse = (group & 0xfff) + 1;
            const int g_size  = group >> 12;

            int size = g_size ? g_size + 2 : take8() + 18;

            // Invalid data could buffer overflow

            for (int i = 0; i < size; ++i) {
                give8(dst[out_position - reverse]);
            }
        };

        const auto read_chunk = [&]() {
            const u8 header = take8();

            for (int i = 0; i < 8; ++i) {
                if (in_position >= src.size())
                    return;
                read_group(header & (1 << (7 - i)));
            }
        };

        if (in_position < src.size())
            read_chunk();

        // if (out_position < dst.size())
        //   return llvm::createStringError(
        //       std::errc::executable_format_error,
        //       "Truncated source file: the file could not be decompressed fully");

        // if (in_position != src.size())
        //   return llvm::createStringError(
        //       std::errc::no_buffer_space,
        //       "Invalid YAZ0 header: file is larger than reported");

        s_last_error.clear();
        return true;
    }

    bool isDataYaz0Compressed(const std::vector<u8> &src) {
        if (src.size() < 8)
            return false;

        return src[0] == 'Y' && src[1] == 'a' && src[2] == 'z' && src[3] == '0';
    }

    std::string getLastError() { return s_last_error; }

    u32 getWorstEncodingSize(u32 src) { return 16 + roundUp(src, 8) / 8 * 9 - 1; }
    u32 getWorstEncodingSize(const std::vector<u8> &src) {
        return getWorstEncodingSize(static_cast<u32>(src.size()));
    }

    u32 getWorstEncodingSize(const std::string &src) {
        return getWorstEncodingSize(static_cast<u32>(src.size()));
    }

    std::vector<u8> encodeFast(const std::vector<u8> &src) {
        std::vector<u8> result(getWorstEncodingSize(src));

        result[0] = 'Y';
        result[1] = 'a';
        result[2] = 'z';
        result[3] = '0';

        result[4] = (src.size() & 0xff00'0000) >> 24;
        result[5] = (src.size() & 0x00ff'0000) >> 16;
        result[6] = (src.size() & 0x0000'ff00) >> 8;
        result[7] = (src.size() & 0x0000'00ff) >> 0;

        std::fill(result.begin() + 8, result.begin() + 16, 0);

        auto *dst = result.data() + 16;

        unsigned counter = 0;
        for (auto *it = src.data(); it < src.data() + src.size(); --counter) {
            if (!counter) {
                *dst++  = 0xff;
                counter = 8;
            }
            *dst++ = *it++;
        }
        while (counter--)
            *dst++ = 0;

        return result;
    }

    static void findMatch(const u8 *src, int srcPos, int maxSize, int *matchOffset, int *matchSize);

    int encodeBoyerMooreHorspool(const u8 *src, u8 *dst, int srcSize) {
        int srcPos;
        int groupHeaderPos;
        int dstPos;
        u8 groupHeaderBitRaw;

        dst[0]  = 'Y';
        dst[1]  = 'a';
        dst[2]  = 'z';
        dst[3]  = '0';
        dst[4]  = 0;
        dst[5]  = srcSize >> 16;
        dst[6]  = srcSize >> 8;
        dst[7]  = srcSize;
        dst[16] = 0;

        srcPos            = 0;
        groupHeaderBitRaw = 0x80;
        groupHeaderPos    = 16;
        dstPos            = 17;
        while (srcPos < srcSize) {
            int matchOffset;
            int firstMatchLen;
            findMatch(src, srcPos, srcSize, &matchOffset, &firstMatchLen);
            if (firstMatchLen > 2) {
                int secondMatchOffset;
                int secondMatchLen;
                findMatch(src, srcPos + 1, srcSize, &secondMatchOffset, &secondMatchLen);
                if (firstMatchLen + 1 < secondMatchLen) {
                    // Put a single byte
                    dst[groupHeaderPos] |= groupHeaderBitRaw;
                    groupHeaderBitRaw = groupHeaderBitRaw >> 1;
                    dst[dstPos++]     = src[srcPos++];
                    if (!groupHeaderBitRaw) {
                        groupHeaderBitRaw = 0x80;
                        groupHeaderPos    = dstPos;
                        dst[dstPos++]     = 0;
                    }
                    // Use the second match
                    firstMatchLen = secondMatchLen;
                    matchOffset   = secondMatchOffset;
                }
                matchOffset = srcPos - matchOffset - 1;
                if (firstMatchLen < 18) {
                    matchOffset |= ((firstMatchLen - 2) << 12);
                    dst[dstPos]     = matchOffset >> 8;
                    dst[dstPos + 1] = matchOffset;
                    dstPos += 2;
                } else {
                    dst[dstPos]     = matchOffset >> 8;
                    dst[dstPos + 1] = matchOffset;
                    dst[dstPos + 2] = firstMatchLen - 18;
                    dstPos += 3;
                }
                srcPos += firstMatchLen;
            } else {
                // Put a single byte
                dst[groupHeaderPos] |= groupHeaderBitRaw;
                dst[dstPos++] = src[srcPos++];
            }

            // Write next group header
            groupHeaderBitRaw >>= 1;
            if (!groupHeaderBitRaw) {
                groupHeaderBitRaw = 0x80;
                groupHeaderPos    = dstPos;
                dst[dstPos++]     = 0;
            }
        }

        return dstPos;
    }

    void findMatch(const u8 *src, int srcPos, int maxSize, int *matchOffset, int *matchSize) {
        // SZS backreference types:
        // (2 bytes) N >= 2:  NR RR    -> maxMatchSize=16+2,    windowOffset=4096+1
        // (3 bytes) N >= 18: 0R RR NN -> maxMatchSize=0xFF+18, windowOffset=4096+1
        // Yaz0 Window is 4096 bytes back
        int windowStart = (srcPos > 4096) ? (srcPos - 4096) : 0;
        int maxMatchLen = 255 + 18;  // Maximum Yaz0 match length

        // Clamp max length to end of buffer
        if (srcPos + maxMatchLen > maxSize) {
            maxMatchLen = maxSize - srcPos;
        }

        // We need at least 3 bytes for a match
        if (maxMatchLen < 3) {
            *matchOffset = 0;
            *matchSize   = 0;
            return;
        }

        const u8 *haystackStart = src + windowStart;
        const u8 *haystackEnd   = src + srcPos;
        const u8 *needle        = src + srcPos;

        int bestLen    = 0;
        int bestOffset = 0;

        // Optimization: Only search using the first byte.
        // memchr is usually SIMD optimized and extremely fast.
        const u8 *scan = haystackStart;
        while (scan < haystackEnd) {
            // Find the next occurrence of the first character
            scan = (const u8 *)memchr(scan, needle[0], haystackEnd - scan);

            // If no more occurrences, stop.
            if (!scan)
                break;

            // We found the first byte. Now check how long the match goes.
            // We only care if this potential match is longer than what we already found.
            // Optimization: Check the byte at 'bestLen' first to fail fast.
            if (scan[bestLen] == needle[bestLen]) {
                int currentLen = 1;
                // Verify the rest of the match
                while (currentLen < maxMatchLen && scan[currentLen] == needle[currentLen]) {
                    currentLen++;
                }

                // Did we beat the record?
                if (currentLen > bestLen) {
                    bestLen    = currentLen;
                    bestOffset = (int)(scan - src);

                    // If we found the maximum possible length, stop searching.
                    if (bestLen == maxMatchLen)
                        break;
                }
            }

            // Move forward to find the next occurrence
            scan++;
        }

        // If we didn't find a match >= 3, return 0
        if (bestLen < 3) {
            *matchOffset = 0;
            *matchSize   = 0;
        } else {
            *matchOffset = bestOffset;
            *matchSize   = bestLen;
        }
    }

    enum {
        YAZ0_MAGIC = 0x59617a30,
        YAZ1_MAGIC = 0x59617a31,
    };

}  // namespace rlibrii::szs