#pragma once

#include "types.hpp"
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <variant>

namespace Toolbox::Object {

    enum class MetaType {
        BOOL,
        S8,
        U8,
        S16,
        U16,
        S32,
        U32,
        F32,
        F64,
        STRING,
        VEC3,
        TRANSFORM,
        COMMENT,
        UNKNOWN,
    };

    template <typename T, bool comment = false> struct map_to_type_enum {
        static constexpr MetaType value = MetaType::UNKNOWN;
    };

    template <> struct map_to_type_enum<bool, false> {
        static constexpr MetaType value = MetaType::BOOL;
    };

    template <> struct map_to_type_enum<s8, false> {
        static constexpr MetaType value = MetaType::S8;
    };

    template <> struct map_to_type_enum<u8, false> {
        static constexpr MetaType value = MetaType::U8;
    };

    template <> struct map_to_type_enum<s16, false> {
        static constexpr MetaType value = MetaType::S16;
    };

    template <> struct map_to_type_enum<u16, false> {
        static constexpr MetaType value = MetaType::U16;
    };

    template <> struct map_to_type_enum<s32, false> {
        static constexpr MetaType value = MetaType::S32;
    };

    template <> struct map_to_type_enum<u32, false> {
        static constexpr MetaType value = MetaType::U32;
    };

    template <> struct map_to_type_enum<f32, false> {
        static constexpr MetaType value = MetaType::F32;
    };

    template <> struct map_to_type_enum<f64, false> {
        static constexpr MetaType value = MetaType::F64;
    };

    template <> struct map_to_type_enum<std::string, false> {
        static constexpr MetaType value = MetaType::STRING;
    };

    template <> struct map_to_type_enum<std::string, true> {
        static constexpr MetaType value = MetaType::COMMENT;
    };

    template <typename T, bool comment = false>
    static constexpr MetaType template_type_v = map_to_type_enum<T, comment>::value;

    template <MetaType T> struct meta_type_info {
        static constexpr std::string_view name = "unknown";
        static constexpr size_t size           = 0;
        static constexpr size_t alignment      = 0;
    };

    template <> struct meta_type_info<MetaType::BOOL> {
        static constexpr std::string_view name = "bool";
        static constexpr size_t size           = sizeof(bool);
        static constexpr size_t alignment      = alignof(bool);
    };

    template <> struct meta_type_info<MetaType::S8> {
        static constexpr std::string_view name = "s8";
        static constexpr size_t size           = sizeof(s8);
        static constexpr size_t alignment      = alignof(s8);
    };

    template <> struct meta_type_info<MetaType::U8> {
        static constexpr std::string_view name = "u8";
        static constexpr size_t size           = sizeof(u8);
        static constexpr size_t alignment      = alignof(u8);
    };

    template <> struct meta_type_info<MetaType::S16> {
        static constexpr std::string_view name = "s16";
        static constexpr size_t size           = sizeof(s16);
        static constexpr size_t alignment      = alignof(s16);
    };

    template <> struct meta_type_info<MetaType::U16> {
        static constexpr std::string_view name = "u16";
        static constexpr size_t size           = sizeof(u16);
        static constexpr size_t alignment      = alignof(u16);
    };

    template <> struct meta_type_info<MetaType::S32> {
        static constexpr std::string_view name = "s32";
        static constexpr size_t size           = sizeof(s32);
        static constexpr size_t alignment      = alignof(s32);
    };

    template <> struct meta_type_info<MetaType::U32> {
        static constexpr std::string_view name = "u32";
        static constexpr size_t size           = sizeof(u32);
        static constexpr size_t alignment      = alignof(u32);
    };

    template <> struct meta_type_info<MetaType::F32> {
        static constexpr std::string_view name = "f32";
        static constexpr size_t size           = sizeof(f32);
        static constexpr size_t alignment      = alignof(f32);
    };

    template <> struct meta_type_info<MetaType::F64> {
        static constexpr std::string_view name = "f64";
        static constexpr size_t size           = sizeof(f64);
        static constexpr size_t alignment      = alignof(f64);
    };

    template <> struct meta_type_info<MetaType::STRING> {
        static constexpr std::string_view name = "string";
        static constexpr size_t size           = 2;
        static constexpr size_t alignment      = 4;
    };

    template <> struct meta_type_info<MetaType::COMMENT> {
        static constexpr std::string_view name = "comment";
        static constexpr size_t size           = 0;
        static constexpr size_t alignment      = 0;
    };

    constexpr std::string_view meta_type_name(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return meta_type_info<MetaType::BOOL>::name;
        case MetaType::S8:
            return meta_type_info<MetaType::S8>::name;
        case MetaType::U8:
            return meta_type_info<MetaType::U8>::name;
        case MetaType::S16:
            return meta_type_info<MetaType::S16>::name;
        case MetaType::U16:
            return meta_type_info<MetaType::U16>::name;
        case MetaType::S32:
            return meta_type_info<MetaType::S32>::name;
        case MetaType::U32:
            return meta_type_info<MetaType::U32>::name;
        case MetaType::F32:
            return meta_type_info<MetaType::F32>::name;
        case MetaType::F64:
            return meta_type_info<MetaType::F64>::name;
        case MetaType::STRING:
            return meta_type_info<MetaType::STRING>::name;
        case MetaType::COMMENT:
            return meta_type_info<MetaType::COMMENT>::name;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::name;
        }
    }

    constexpr size_t meta_type_size(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return meta_type_info<MetaType::BOOL>::size;
        case MetaType::S8:
            return meta_type_info<MetaType::S8>::size;
        case MetaType::U8:
            return meta_type_info<MetaType::U8>::size;
        case MetaType::S16:
            return meta_type_info<MetaType::S16>::size;
        case MetaType::U16:
            return meta_type_info<MetaType::U16>::size;
        case MetaType::S32:
            return meta_type_info<MetaType::S32>::size;
        case MetaType::U32:
            return meta_type_info<MetaType::U32>::size;
        case MetaType::F32:
            return meta_type_info<MetaType::F32>::size;
        case MetaType::F64:
            return meta_type_info<MetaType::F64>::size;
        case MetaType::STRING:
            return meta_type_info<MetaType::STRING>::size;
        case MetaType::COMMENT:
            return meta_type_info<MetaType::COMMENT>::size;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::size;
        }
    }

    constexpr size_t meta_type_alignment(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return meta_type_info<MetaType::BOOL>::alignment;
        case MetaType::S8:
            return meta_type_info<MetaType::S8>::alignment;
        case MetaType::U8:
            return meta_type_info<MetaType::U8>::alignment;
        case MetaType::S16:
            return meta_type_info<MetaType::S16>::alignment;
        case MetaType::U16:
            return meta_type_info<MetaType::U16>::alignment;
        case MetaType::S32:
            return meta_type_info<MetaType::S32>::alignment;
        case MetaType::U32:
            return meta_type_info<MetaType::U32>::alignment;
        case MetaType::F32:
            return meta_type_info<MetaType::F32>::alignment;
        case MetaType::F64:
            return meta_type_info<MetaType::F64>::alignment;
        case MetaType::STRING:
            return meta_type_info<MetaType::STRING>::alignment;
        case MetaType::COMMENT:
            return meta_type_info<MetaType::COMMENT>::alignment;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::alignment;
        }
    }

    class MetaValue {
    public:
        using value_type =
            std::variant<bool, s8, u8, s16, u16, s32, u32, s64, u64, f32, f64, std::string>;

        constexpr MetaValue() = delete;
        template <typename T> explicit constexpr MetaValue(T value) : m_value(value) {
            set<T, false>(value);
        }
        constexpr MetaValue(const MetaValue &other) = default;
        constexpr MetaValue(MetaValue &&other)      = default;

        constexpr MetaValue &operator=(const MetaValue &other) = default;
        constexpr MetaValue &operator=(MetaValue &&other)      = default;
        template <typename T> constexpr MetaValue &operator=(T &&value) {
            set<T, false>(value);
            return *this;
        }
        template <typename T> constexpr MetaValue &operator=(const T &value) {
            set<T, false>(value);
            return *this;
        }

        [[nodiscard]] constexpr bool is_comment() const { return m_type == MetaType::COMMENT; }

        [[nodiscard]] constexpr MetaType type() const { return m_type; }

        template <typename T, bool comment = false>
        [[nodiscard]] constexpr std::expected<T, std::string> get() const {
            if (m_type != map_to_type_enum<T, comment>::value)
                return std::unexpected("Type record mismatch");
            try {
                return std::get<T>(m_value);
            } catch (const std::bad_variant_access &e) {
                return std::unexpected(e.what());
            }
        }

        template <typename T, bool comment = false> constexpr bool set(const T &value) {
            if constexpr (std::is_same_v<std::remove_reference_t<T>, std::string> && comment) {
                m_type = MetaType::COMMENT;
            } else {
                m_type = map_to_type_enum<T>::value;
            }
            m_value = value;
            return true;
        }

        [[nodiscard]] std::string toString() const;

        constexpr bool operator==(const MetaValue &rhs) const = default;

    private:
        value_type m_value;
        MetaType m_type = MetaType::UNKNOWN;
    };

}  // namespace Toolbox::Object