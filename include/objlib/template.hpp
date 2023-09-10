#pragma once

#include "json.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using json = nlohmann::json;

namespace Toolbox::Object {

    enum class TemplateType {
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
        ENUM,
        VEC3,
        TRANSFORM,
        STRUCT,
        COMMENT,
        UNKNOWN,
    };

    class TemplateEnum;
    class TemplateValue;
    class TemplateMember;
    class TemplateStruct;

    template <typename T, bool comment = false> struct map_to_type_enum {
        static constexpr TemplateType value    = TemplateType::UNKNOWN;
        static constexpr std::string_view name = "unknown";
    };

    template <> struct map_to_type_enum<bool, false> {
        static constexpr TemplateType value    = TemplateType::BOOL;
        static constexpr std::string_view name = "bool";
    };

    template <> struct map_to_type_enum<s8, false> {
        static constexpr TemplateType value    = TemplateType::S8;
        static constexpr std::string_view name = "s8";
    };

    template <> struct map_to_type_enum<u8, false> {
        static constexpr TemplateType value    = TemplateType::U8;
        static constexpr std::string_view name = "u8";
    };

    template <> struct map_to_type_enum<s16, false> {
        static constexpr TemplateType value    = TemplateType::S16;
        static constexpr std::string_view name = "s16";
    };

    template <> struct map_to_type_enum<u16, false> {
        static constexpr TemplateType value    = TemplateType::U16;
        static constexpr std::string_view name = "u16";
    };

    template <> struct map_to_type_enum<s32, false> {
        static constexpr TemplateType value    = TemplateType::S32;
        static constexpr std::string_view name = "s32";
    };

    template <> struct map_to_type_enum<u32, false> {
        static constexpr TemplateType value    = TemplateType::U32;
        static constexpr std::string_view name = "u32";
    };

    template <> struct map_to_type_enum<f32, false> {
        static constexpr TemplateType value    = TemplateType::F32;
        static constexpr std::string_view name = "f32";
    };

    template <> struct map_to_type_enum<f64, false> {
        static constexpr TemplateType value    = TemplateType::F64;
        static constexpr std::string_view name = "f64";
    };

    template <> struct map_to_type_enum<std::string, false> {
        static constexpr TemplateType value    = TemplateType::STRING;
        static constexpr std::string_view name = "string";
    };

    template <> struct map_to_type_enum<Transform, false> {
        static constexpr TemplateType value    = TemplateType::TRANSFORM;
        static constexpr std::string_view name = "transform";
    };

    template <> struct map_to_type_enum<TemplateEnum, false> {
        static constexpr TemplateType value    = TemplateType::ENUM;
        static constexpr std::string_view name = "enum";
    };

    template <> struct map_to_type_enum<TemplateStruct, false> {
        static constexpr TemplateType value    = TemplateType::STRUCT;
        static constexpr std::string_view name = "struct";
    };

    template <> struct map_to_type_enum<std::string, true> {
        static constexpr TemplateType value    = TemplateType::COMMENT;
        static constexpr std::string_view name = "comment";
    };

    template <typename T, bool comment = false>
    static constexpr TemplateType template_type_v = map_to_type_enum<T, comment>::value;

    template <typename T, bool comment = false>
    static constexpr std::string_view template_type_name_v = map_to_type_enum<T, comment>::name;

    constexpr std::string_view template_type_name(TemplateType type) {
        switch (type) {
        case TemplateType::BOOL:
            return template_type_name_v<bool>;
        case TemplateType::S8:
            return template_type_name_v<s8>;
        case TemplateType::U8:
            return template_type_name_v<u8>;
        case TemplateType::S16:
            return template_type_name_v<s16>;
        case TemplateType::U16:
            return template_type_name_v<u16>;
        case TemplateType::S32:
            return template_type_name_v<s32>;
        case TemplateType::U32:
            return template_type_name_v<u32>;
        case TemplateType::F32:
            return template_type_name_v<f32>;
        case TemplateType::F64:
            return template_type_name_v<f64>;
        case TemplateType::STRING:
            return template_type_name_v<std::string>;
        case TemplateType::ENUM:
            return template_type_name_v<TemplateEnum>;
        case TemplateType::VEC3:
            return "Vec3";
        case TemplateType::TRANSFORM:
            return template_type_name_v<Transform>;
        case TemplateType::STRUCT:
            return template_type_name_v<TemplateStruct>;
        case TemplateType::COMMENT:
            return template_type_name_v<std::string, true>;
        case TemplateType::UNKNOWN:
        default:
            return template_type_name_v<void>;
        }
    }

    class TemplateStruct {
    public:
        constexpr TemplateStruct() = delete;
        constexpr TemplateStruct(std::string_view name) : m_name(name) {}
        constexpr TemplateStruct(std::string_view name, std::vector<TemplateMember> members)
            : m_name(name), m_members(std::move(members)) {}
        constexpr ~TemplateStruct() = default;

        [[nodiscard]] constexpr std::string_view name() const { return m_name; }
        [[nodiscard]] constexpr std::vector<TemplateMember> members() const { return m_members; }

        bool operator==(const TemplateStruct &other) const;

    private:
        std::string m_name;
        std::vector<TemplateMember> m_members = {};
    };

    class TemplateEnum {
    public:
        using value_type = std::pair<std::string, TemplateValue>;

        using iterator               = std::vector<value_type>::iterator;
        using const_iterator         = std::vector<value_type>::const_iterator;
        using reverse_iterator       = std::vector<value_type>::reverse_iterator;
        using const_reverse_iterator = std::vector<value_type>::const_reverse_iterator;

        constexpr TemplateEnum() = delete;
        constexpr TemplateEnum(std::string_view name, std::vector<value_type> values,
                               bool bit_mask = false)
            : m_type(TemplateType::S32), m_name(name), m_values(std::move(values)),
              m_bit_mask(bit_mask) {}
        constexpr TemplateEnum(std::string_view name, TemplateType type,
                               std::vector<value_type> values, bool bit_mask = false)
            : TemplateEnum(name, values, bit_mask) {
            switch (type) {
            case TemplateType::S8:
            case TemplateType::U8:
            case TemplateType::S16:
            case TemplateType::U16:
            case TemplateType::S32:
            case TemplateType::U32:
                m_type = type;
                break;
            default:
                break;
            }
        }
        constexpr ~TemplateEnum() = default;

        [[nodiscard]] constexpr bool isBitMasked() const { return m_bit_mask; }

        [[nodiscard]] constexpr TemplateType type() const { return m_type; }
        [[nodiscard]] constexpr std::string_view name() const { return m_name; }
        [[nodiscard]] constexpr std::vector<value_type> values() const { return m_values; }

        [[nodiscard]] constexpr iterator begin();
        [[nodiscard]] constexpr const_iterator begin() const;
        [[nodiscard]] constexpr const_iterator cbegin() const;

        [[nodiscard]] constexpr iterator end();
        [[nodiscard]] constexpr const_iterator end() const;
        [[nodiscard]] constexpr const_iterator cend() const;

        [[nodiscard]] constexpr reverse_iterator rbegin();
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const;
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const;

        [[nodiscard]] constexpr reverse_iterator rend();
        [[nodiscard]] constexpr const_reverse_iterator rend() const;
        [[nodiscard]] constexpr const_reverse_iterator crend() const;

        [[nodiscard]] std::optional<value_type> find(std::string_view name) const;

        template <typename T>
        [[nodiscard]] std::optional<value_type> vfind(T value) const {
            return {};
        }
        template <> [[nodiscard]] std::optional<value_type> vfind<s8>(s8 value) const;
        template <> [[nodiscard]] std::optional<value_type> vfind<u8>(u8 value) const;
        template <> [[nodiscard]] std::optional<value_type> vfind<s16>(s16 value) const;
        template <> [[nodiscard]] std::optional<value_type> vfind<u16>(u16 value) const;
        template <> [[nodiscard]] std::optional<value_type> vfind<s32>(s32 value) const;
        template <> [[nodiscard]] std::optional<value_type> vfind<u32>(u32 value) const;

        void dump(std::ostream &out, int indention = 0, int indention_width = 2) const;

        constexpr bool operator==(const TemplateEnum &rhs) const;

    private:
        TemplateType m_type;
        std::string m_name;
        std::vector<value_type> m_values;
        bool m_bit_mask = false;
    };

    class TemplateValue {
    public:
        using value_type = std::variant<bool, s8, u8, s16, u16, s32, u32, s64, u64, f32, f64,
                                        std::string, Transform, TemplateEnum, TemplateStruct>;

        TemplateValue() = delete;
        template <typename T> TemplateValue(T value) : m_value(value) { set<T, false>(value); }

        [[nodiscard]] constexpr TemplateType type() const { return m_type; }

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
                m_type = TemplateType::COMMENT;
            } else {
                m_type = map_to_type_enum<T>::value;
            }
            m_value = value;
            return true;
        }

        [[nodiscard]] std::string toString() const;

        bool operator==(const TemplateValue &rhs) const;

    private:
        value_type m_value;
        TemplateType m_type = TemplateType::UNKNOWN;
    };

    class TemplateMember {
    public:
        TemplateMember() = delete;
        TemplateMember(std::string_view name, const TemplateValue &value)
            : m_name(name), m_values({value}) {}
        TemplateMember(std::string_view name, std::vector<TemplateValue> values)
            : m_name(name), m_values(std::move(values)) {}
        ~TemplateMember() = default;

        [[nodiscard]] std::string rawName() const { return m_name; }
        [[nodiscard]] std::string strippedName() const;
        [[nodiscard]] std::string formattedName(int index) const;

        [[nodiscard]] constexpr bool isArray() const { return m_values.size() > 1; }
        [[nodiscard]] constexpr bool isTypeEnum() const {
            return m_values[0].type() == TemplateType::ENUM;
        }
        [[nodiscard]] constexpr bool isTypeStruct() const {
            return m_values[0].type() == TemplateType::STRUCT;
        }
        [[nodiscard]] constexpr bool isTypeComment() const {
            return m_values[0].type() == TemplateType::COMMENT;
        }

        [[nodiscard]] constexpr bool isTypeBitMasked() const {
            return isTypeEnum() && m_values[0].get<TemplateEnum>().value().isBitMasked();
        }

        [[nodiscard]] constexpr bool isTypeBool() const {
            return m_values[0].type() == TemplateType::BOOL;
        }
        [[nodiscard]] constexpr bool isTypeS8() const {
            return m_values[0].type() == TemplateType::S8;
        }
        [[nodiscard]] constexpr bool isTypeU8() const {
            return m_values[0].type() == TemplateType::U8;
        }
        [[nodiscard]] constexpr bool isTypeS16() const {
            return m_values[0].type() == TemplateType::S16;
        }
        [[nodiscard]] constexpr bool isTypeU16() const {
            return m_values[0].type() == TemplateType::U16;
        }
        [[nodiscard]] constexpr bool isTypeS32() const {
            return m_values[0].type() == TemplateType::S32;
        }
        [[nodiscard]] constexpr bool isTypeU32() const {
            return m_values[0].type() == TemplateType::U32;
        }
        [[nodiscard]] constexpr bool isTypeF32() const {
            return m_values[0].type() == TemplateType::F32;
        }
        [[nodiscard]] constexpr bool isTypeF64() const {
            return m_values[0].type() == TemplateType::F64;
        }
        [[nodiscard]] constexpr bool isTypeString() const {
            return m_values[0].type() == TemplateType::STRING;
        }
        [[nodiscard]] constexpr bool isTypeVec3() const {
            return m_values[0].type() == TemplateType::VEC3;
        }
        [[nodiscard]] constexpr bool isTypeTransform() const {
            return m_values[0].type() == TemplateType::TRANSFORM;
        }

        [[nodiscard]] constexpr bool isTypeUnknown() const {
            return m_values[0].type() == TemplateType::UNKNOWN;
        }

        bool operator==(const TemplateMember &other) const;

    private:
        std::string m_name;
        std::vector<TemplateValue> m_values;
    };

    struct TemplateWizard {
        std::string m_name;
        std::vector<TemplateMember> m_init_members;
    };

    class Template {
    public:
        Template() = delete;
        Template(std::string_view name, std::string_view long_name)
            : m_name(name), m_long_name(long_name) {}
        ~Template() = default;

        [[nodiscard]] std::string_view name() const { return m_name; }
        [[nodiscard]] std::string_view longName() const { return m_long_name; }

        [[nodiscard]] std::optional<TemplateEnum> getEnum(const std::string &name) const;
        [[nodiscard]] std::optional<TemplateStruct> getStruct(const std::string &name) const;
        [[nodiscard]] std::optional<TemplateMember> getMember(const std::string &name) const;
        [[nodiscard]] std::optional<TemplateWizard> getWizard(const std::string &name) const;

    private:
        std::string m_name;
        std::string m_long_name;
        std::vector<TemplateEnum> m_enums     = {};
        std::vector<TemplateStruct> m_structs = {};
        std::vector<TemplateMember> m_members = {};
        std::vector<TemplateWizard> m_wizards = {};
    };
}  // namespace Toolbox::Object
