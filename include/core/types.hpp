#pragma once

#include <bit>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace Toolbox {

    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using s8  = int8_t;
    using s16 = int16_t;
    using s32 = int32_t;
    using s64 = int64_t;
    using f32 = float;
    using f64 = double;

    // Credits to riidefi once again for this snippet

    template <typename T> struct endian_swapped_t {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

        constexpr endian_swapped_t() : mData(T{}) {}
        constexpr endian_swapped_t(T v) : mData(getFlipped(v)) {}

        constexpr operator T() const { return mData; }
        constexpr T operator*() const { return mData; }

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

    template <typename _Enum>
    constexpr auto raw_enum(_Enum e) -> decltype(auto) {
        return static_cast<std::underlying_type_t<_Enum>>(e);
    }

}  // namespace Toolbox

#define TOOLBOX_BITWISE_ENUM(EnumType)                                                             \
    inline EnumType operator|(EnumType lhs, EnumType rhs) {                                        \
        using T = std::underlying_type_t<EnumType>;                                                \
        return static_cast<EnumType>(static_cast<T>(lhs) | static_cast<T>(rhs));                   \
    }                                                                                              \
    inline EnumType operator&(EnumType lhs, EnumType rhs) {                                        \
        using T = std::underlying_type_t<EnumType>;                                                \
        return static_cast<EnumType>(static_cast<T>(lhs) & static_cast<T>(rhs));                   \
    }                                                                                              \
    inline EnumType operator^(EnumType lhs, EnumType rhs) {                                        \
        using T = std::underlying_type_t<EnumType>;                                                \
        return static_cast<EnumType>(static_cast<T>(lhs) ^ static_cast<T>(rhs));                   \
    }                                                                                              \
    inline EnumType operator~(EnumType rhs) {                                                      \
        using T = std::underlying_type_t<EnumType>;                                                \
        return static_cast<EnumType>(~static_cast<T>(rhs));                                        \
    }                                                                                              \
    inline EnumType &operator|=(EnumType &lhs, EnumType rhs) {                                     \
        using T = std::underlying_type_t<EnumType>;                                                \
        lhs     = static_cast<EnumType>(static_cast<T>(lhs) | static_cast<T>(rhs));                \
        return lhs;                                                                                \
    }                                                                                              \
    inline EnumType &operator&=(EnumType &lhs, EnumType rhs) {                                     \
        using T = std::underlying_type_t<EnumType>;                                                \
        lhs     = static_cast<EnumType>(static_cast<T>(lhs) & static_cast<T>(rhs));                \
        return lhs;                                                                                \
    }                                                                                              \
    inline EnumType &operator^=(EnumType &lhs, EnumType rhs) {                                     \
        using T = std::underlying_type_t<EnumType>;                                                \
        lhs     = static_cast<EnumType>(static_cast<T>(lhs) ^ static_cast<T>(rhs));                \
        return lhs;                                                                                \
    }