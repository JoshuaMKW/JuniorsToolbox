#pragma once

#include "types.hpp"
#include <expected>
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
        static constexpr MetaType value        = MetaType::UNKNOWN;
        static constexpr std::string_view name = "unknown";
    };

    template <> struct map_to_type_enum<bool, false> {
        static constexpr MetaType value        = MetaType::BOOL;
        static constexpr std::string_view name = "bool";
    };

    template <> struct map_to_type_enum<s8, false> {
        static constexpr MetaType value        = MetaType::S8;
        static constexpr std::string_view name = "s8";
    };

    template <> struct map_to_type_enum<u8, false> {
        static constexpr MetaType value        = MetaType::U8;
        static constexpr std::string_view name = "u8";
    };

    template <> struct map_to_type_enum<s16, false> {
        static constexpr MetaType value        = MetaType::S16;
        static constexpr std::string_view name = "s16";
    };

    template <> struct map_to_type_enum<u16, false> {
        static constexpr MetaType value        = MetaType::U16;
        static constexpr std::string_view name = "u16";
    };

    template <> struct map_to_type_enum<s32, false> {
        static constexpr MetaType value        = MetaType::S32;
        static constexpr std::string_view name = "s32";
    };

    template <> struct map_to_type_enum<u32, false> {
        static constexpr MetaType value        = MetaType::U32;
        static constexpr std::string_view name = "u32";
    };

    template <> struct map_to_type_enum<f32, false> {
        static constexpr MetaType value        = MetaType::F32;
        static constexpr std::string_view name = "f32";
    };

    template <> struct map_to_type_enum<f64, false> {
        static constexpr MetaType value        = MetaType::F64;
        static constexpr std::string_view name = "f64";
    };

    template <> struct map_to_type_enum<std::string, false> {
        static constexpr MetaType value        = MetaType::STRING;
        static constexpr std::string_view name = "string";
    };

    template <> struct map_to_type_enum<std::string, true> {
        static constexpr MetaType value        = MetaType::COMMENT;
        static constexpr std::string_view name = "comment";
    };

    template <typename T, bool comment = false>
    static constexpr MetaType template_type_v = map_to_type_enum<T, comment>::value;

    template <typename T, bool comment = false>
    static constexpr std::string_view template_type_name_v = map_to_type_enum<T, comment>::name;

    constexpr std::string_view meta_type_name(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return template_type_name_v<bool>;
        case MetaType::S8:
            return template_type_name_v<s8>;
        case MetaType::U8:
            return template_type_name_v<u8>;
        case MetaType::S16:
            return template_type_name_v<s16>;
        case MetaType::U16:
            return template_type_name_v<u16>;
        case MetaType::S32:
            return template_type_name_v<s32>;
        case MetaType::U32:
            return template_type_name_v<u32>;
        case MetaType::F32:
            return template_type_name_v<f32>;
        case MetaType::F64:
            return template_type_name_v<f64>;
        case MetaType::STRING:
            return template_type_name_v<std::string>;
        case MetaType::COMMENT:
            return template_type_name_v<std::string, true>;
        case MetaType::UNKNOWN:
        default:
            return template_type_name_v<void>;
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