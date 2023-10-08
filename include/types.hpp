#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;

// Credits to riidefi once again for this snippet

template <typename T> struct endian_swapped_t {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

    constexpr endian_swapped_t() : mData(T{}) {}
    constexpr endian_swapped_t(T v) : mData(getFlipped(v)) {}

    constexpr operator T() const { return getFlipped(mData); }
    constexpr T operator*() const { return getFlipped(mData); }

private:
    static constexpr T getFlipped(T v) {
        if constexpr (std::is_same_v<T, f32>) {
            u32 t2 = std::byteswap(std::bit_cast<u32>(v));
            return std::bit_cast<f32>(t2);
        } else if constexpr (std::is_same_v<T, f64>) {
            u64 t2 = std::byteswap(std::bit_cast<u64>(v));
            return std::bit_cast<f64>(t2);
        } else {
            return std::byteswap(v);
        }
    }

    T mData;
};

using bs16 = endian_swapped_t<s16>;
using bu16 = endian_swapped_t<u16>;
using bs32 = endian_swapped_t<s32>;
using bu32 = endian_swapped_t<u32>;
using bs64 = endian_swapped_t<s64>;
using bu64 = endian_swapped_t<u64>;
using bf32 = endian_swapped_t<f32>;
using bf64 = endian_swapped_t<f64>;

static_assert(sizeof(endian_swapped_t<s16>) == sizeof(s16));
static_assert(sizeof(endian_swapped_t<u16>) == sizeof(u16));
static_assert(sizeof(endian_swapped_t<s32>) == sizeof(s32));
static_assert(sizeof(endian_swapped_t<u32>) == sizeof(u32));
static_assert(sizeof(endian_swapped_t<s64>) == sizeof(s64));
static_assert(sizeof(endian_swapped_t<u64>) == sizeof(u64));
static_assert(sizeof(endian_swapped_t<f32>) == sizeof(f32));
static_assert(sizeof(endian_swapped_t<f64>) == sizeof(f64));