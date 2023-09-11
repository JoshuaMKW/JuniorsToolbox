#pragma once

#include "json.hpp"
#include "metavalue.hpp"
#include "qualname.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using json = nlohmann::json;

namespace Toolbox::Object {
    class TemplateEnum;
    class TemplateMember;
    class TemplateStruct;

    class TemplateStruct {
    public:
        constexpr TemplateStruct() = delete;
        constexpr TemplateStruct(std::string_view name) : m_name(name) {}
        constexpr TemplateStruct(std::string_view name, std::vector<TemplateMember> members)
            : m_name(name), m_members(std::move(members)) {}
        constexpr TemplateStruct(const TemplateStruct &other) = default;
        constexpr TemplateStruct(TemplateStruct &&other)      = default;
        constexpr ~TemplateStruct()                           = default;

        [[nodiscard]] constexpr std::string_view name() const { return m_name; }
        [[nodiscard]] constexpr std::vector<TemplateMember> members() const { return m_members; }
        [[nodiscard]] constexpr TemplateStruct *parent() const { return m_parent; }

        [[nodiscard]] constexpr QualifiedName qualifiedName() const;

        void dump(std::ostream &out, int indention, int indention_width, bool naked = false) const;
        void dump(std::ostream &out, int indention, bool naked = false) const { dump(out, indention, 2, naked); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        bool operator==(const TemplateStruct &other) const;

    private:
        std::string m_name;
        std::vector<TemplateMember> m_members = {};
        TemplateStruct *m_parent              = nullptr;
    };

    class TemplateEnum {
    public:
        using value_type = std::pair<std::string, MetaValue>;

        using iterator               = std::vector<value_type>::iterator;
        using const_iterator         = std::vector<value_type>::const_iterator;
        using reverse_iterator       = std::vector<value_type>::reverse_iterator;
        using const_reverse_iterator = std::vector<value_type>::const_reverse_iterator;

        constexpr TemplateEnum() = delete;
        constexpr TemplateEnum(std::string_view name, std::vector<value_type> values,
                               bool bit_mask = false)
            : m_type(MetaType::S32), m_name(name), m_values(std::move(values)),
              m_bit_mask(bit_mask) {}
        constexpr TemplateEnum(std::string_view name, MetaType type, std::vector<value_type> values,
                               bool bit_mask = false)
            : TemplateEnum(name, values, bit_mask) {
            switch (type) {
            case MetaType::S8:
            case MetaType::U8:
            case MetaType::S16:
            case MetaType::U16:
            case MetaType::S32:
            case MetaType::U32:
                m_type = type;
                break;
            default:
                break;
            }
        }
        constexpr TemplateEnum(const TemplateEnum &other) = default;
        constexpr TemplateEnum(TemplateEnum &&other)      = default;
        constexpr ~TemplateEnum()                         = default;

        [[nodiscard]] constexpr MetaType type() const { return m_type; }
        [[nodiscard]] constexpr std::string_view name() const { return m_name; }
        [[nodiscard]] constexpr std::vector<value_type> values() const { return m_values; }

        [[nodiscard]] constexpr bool isBitMasked() const { return m_bit_mask; }

        [[nodiscard]] constexpr iterator begin() { return m_values.begin(); }
        [[nodiscard]] constexpr const_iterator begin() const { return m_values.begin(); }
        [[nodiscard]] constexpr const_iterator cbegin() const { return m_values.cbegin(); }

        [[nodiscard]] constexpr iterator end() { return m_values.end(); }
        [[nodiscard]] constexpr const_iterator end() const { return m_values.end(); }
        [[nodiscard]] constexpr const_iterator cend() const { return m_values.cend(); }

        [[nodiscard]] constexpr reverse_iterator rbegin() { return m_values.rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const { return m_values.rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const {
            return m_values.crbegin();
        }

        [[nodiscard]] constexpr reverse_iterator rend() { return m_values.rend(); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const { return m_values.rend(); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const { return m_values.crend(); }

        [[nodiscard]] std::optional<value_type> find(std::string_view name) const;

        template <typename T> [[nodiscard]] std::optional<value_type> vfind(T value) const {
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
        MetaType m_type;
        std::string m_name;
        std::vector<value_type> m_values;
        bool m_bit_mask = false;
    };

    class TemplateMember {
    public:
        using value_type = std::variant<TemplateStruct, TemplateEnum, MetaValue>;

        TemplateMember() = delete;
        TemplateMember(std::string_view name, const MetaValue &value) : m_name(name), m_values() {
             m_values.emplace_back(value_type(value));
        }
        TemplateMember(std::string_view name, const TemplateStruct &value)
            : m_name(name), m_values() {
             m_values.emplace_back(value_type(value));
        }
        TemplateMember(std::string_view name, const TemplateEnum &value)
            : m_name(name), m_values() {
             m_values.emplace_back(value_type(value));
        }
        TemplateMember(std::string_view name, const std::vector<MetaValue> &values)
            : m_name(name), m_values() {
            for (const auto &value : values) {
                m_values.emplace_back(value_type(value));
            }
        }
        TemplateMember(std::string_view name, const std::vector<TemplateStruct> &values)
            : m_name(name), m_values() {
            for (const auto &value : values) {
                m_values.emplace_back(value_type(value));
            }
        }
        TemplateMember(std::string_view name, const std::vector<TemplateEnum> &values)
            : m_name(name), m_values() {
            for (const auto &value : values) {
                m_values.emplace_back(value_type(value));
            }
        }
        TemplateMember(const TemplateMember &) = default;
        TemplateMember(TemplateMember &&)      = default;
        ~TemplateMember()                      = default;

        [[nodiscard]] std::string rawName() const { return m_name; }
        [[nodiscard]] std::string strippedName() const;
        [[nodiscard]] std::string formattedName(int index) const;
        [[nodiscard]] QualifiedName qualifiedName() const;
        [[nodiscard]] constexpr TemplateStruct *parent() const { return m_parent; }

        template <typename T> std::expected<T, std::string> value(int index) const {
            return std::unexpected("Invalid type");
        }
        template <>
        std::expected<TemplateStruct, std::string> value<TemplateStruct>(int index) const;
        template <> std::expected<TemplateEnum, std::string> value<TemplateEnum>(int index) const;
        template <> std::expected<MetaValue, std::string> value<MetaValue>(int index) const;

        [[nodiscard]] bool isTypeBitMasked() const {
            return isTypeEnum() && value<TemplateEnum>(0).value().isBitMasked();
        }
        [[nodiscard]] bool isArray() const { return m_values.size() > 1; }
        [[nodiscard]] bool isTypeStruct() const {
            return std::holds_alternative<TemplateStruct>(m_values[0]);
        }
        [[nodiscard]] bool isTypeEnum() const {
            return std::holds_alternative<TemplateEnum>(m_values[0]);
        }
        [[nodiscard]] bool isTypeBool() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::BOOL;
        }
        [[nodiscard]] bool isTypeS8() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::S8;
        }
        [[nodiscard]] bool isTypeU8() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::U8;
        }
        [[nodiscard]] bool isTypeS16() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::S16;
        }
        [[nodiscard]] bool isTypeU16() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::U16;
        }
        [[nodiscard]] bool isTypeS32() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::S32;
        }
        [[nodiscard]] bool isTypeU32() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::U32;
        }
        [[nodiscard]] bool isTypeF32() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::F32;
        }
        [[nodiscard]] bool isTypeF64() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::F64;
        }
        [[nodiscard]] bool isTypeString() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::STRING;
        }
        [[nodiscard]] bool isTypeVec3() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::VEC3;
        }
        [[nodiscard]] bool isTypeTransform() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::TRANSFORM;
        }
        [[nodiscard]] bool isTypeComment() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::COMMENT;
        }
        [[nodiscard]] bool isTypeUnknown() const {
            return isTypeValue() && value<MetaValue>(0).value().type() == MetaType::UNKNOWN;
        }

        void dump(std::ostream &out, int indention = 0, int indention_width = 4) const;

        bool operator==(const TemplateMember &other) const;

    protected:
        constexpr bool isTypeValue() const {
            return std::holds_alternative<MetaValue>(m_values[0]);
        }

    private:
        std::string m_name;
        std::vector<value_type> m_values;
        TemplateStruct *m_parent = nullptr;
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
