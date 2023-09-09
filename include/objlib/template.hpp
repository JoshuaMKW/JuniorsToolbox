#pragma once

#include "json.hpp"
#include "types.hpp"
#include <expected>
#include <string>
#include <unordered_map>
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

    struct TemplateEnum;
    struct TemplateMember;
    struct TemplateStruct;

    template <typename T, bool comment = false> struct TemplateTypeMap {
        static constexpr TemplateType value = TemplateType::UNKNOWN;
    };

    template <> struct TemplateTypeMap<bool, false> {
        static constexpr TemplateType value = TemplateType::BOOL;
    };

    template <> struct TemplateTypeMap<s8, false> {
        static constexpr TemplateType value = TemplateType::S8;
    };

    template <> struct TemplateTypeMap<u8, false> {
        static constexpr TemplateType value = TemplateType::U8;
    };

    template <> struct TemplateTypeMap<s16, false> {
        static constexpr TemplateType value = TemplateType::S16;
    };

    template <> struct TemplateTypeMap<u16, false> {
        static constexpr TemplateType value = TemplateType::U16;
    };

    template <> struct TemplateTypeMap<s32, false> {
        static constexpr TemplateType value = TemplateType::S32;
    };

    template <> struct TemplateTypeMap<u32, false> {
        static constexpr TemplateType value = TemplateType::U32;
    };

    template <> struct TemplateTypeMap<f32, false> {
        static constexpr TemplateType value = TemplateType::F32;
    };

    template <> struct TemplateTypeMap<f64, false> {
        static constexpr TemplateType value = TemplateType::F64;
    };

    template <> struct TemplateTypeMap<std::string, false> {
        static constexpr TemplateType value = TemplateType::STRING;
    };

    template <> struct TemplateTypeMap<TemplateEnum, false> {
        static constexpr TemplateType value = TemplateType::TRANSFORM;
    };

    template <> struct TemplateTypeMap<TemplateStruct, false> {
        static constexpr TemplateType value = TemplateType::STRUCT;
    };

    template <> struct TemplateTypeMap<std::string, true> {
        static constexpr TemplateType value = TemplateType::COMMENT;
    };

    class TemplateValue {
    public:
        TemplateValue() = delete;
        template <typename T> TemplateValue(T value) : m_value(value) {
            set<T, false>(value);
        }

        [[nodiscard]] TemplateType type() const { return m_type; }

        template <typename T, bool comment = false>
        [[nodiscard]] std::expected<T, std::string> get() const {
            if (m_type != TemplateTypeMap<T, comment>::value)
                return std::unexpected("Type record mismatch");
            try {
                return std::any_cast<T>(m_value);
            } catch (const std::bad_any_cast &e) {
                return std::unexpected(e.what());
            }
        }

        template <typename T, bool comment = false> bool set(const T &value) {
            if constexpr (std::is_same_v<std::remove_reference_t<T>, std::string> && comment) {
                m_type = TemplateType::COMMENT;
            } else {
                m_type = TemplateTypeMap<T>::value;
            }
            m_value = value;
            return true;
        }

    private:
        std::any m_value;
        TemplateType m_type = TemplateType::UNKNOWN;
    };

    struct TemplateEnum {
        using value_type = std::pair<std::string, TemplateValue>;

        TemplateType m_type;
        std::string m_name;
        std::vector<value_type> m_values;
        bool m_bit_mask = false;

        [[nodiscard]] constexpr bool isBitMasked() const { return m_bit_mask; }
    };

    struct TemplateMember {
        std::string m_name;
        std::vector<TemplateValue> m_values;

        [[nodiscard]] constexpr bool isArray() const { return m_values.size() > 1; }
    };

    struct TemplateStruct {
        std::string m_name;
        std::vector<TemplateMember> m_members;
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

        std::string_view name() const { return m_name; }
        std::string_view longName() const { return m_long_name; }

        std::optional<TemplateEnum> getEnum(const std::string &name) const {
            for (const auto &e : m_enums) {
                if (e.m_name == name)
                    return e;
            }
            return {};
        }
        std::optional<TemplateStruct> getStruct(const std::string &name) const {
            for (const auto &s : m_structs) {
                if (s.m_name == name)
                    return s;
            }
            return {};
        }
        std::optional<TemplateMember> getMember(const std::string &name) const {
            for (const auto &m : m_members) {
                if (m.m_name == name)
                    return m;
            }
            return {};
        }
        std::optional<TemplateWizard> getWizard(const std::string &name) const {
            for (const auto &w : m_wizards) {
                if (w.m_name == name)
                    return w;
            }
            return {};
        }

    private:
        std::string m_name;
        std::string m_long_name;
        std::vector<TemplateEnum> m_enums     = {};
        std::vector<TemplateStruct> m_structs = {};
        std::vector<TemplateMember> m_members = {};
        std::vector<TemplateWizard> m_wizards = {};
    };
}  // namespace Toolbox::Object
