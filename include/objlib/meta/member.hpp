#pragma once

#include "enum.hpp"
#include "error.hpp"
#include "objlib/qualname.hpp"
#include "struct.hpp"
#include "value.hpp"
#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Toolbox::Object {

    class MetaMember {
    public:
        using value_type = std::variant<std::shared_ptr<MetaStruct>, std::shared_ptr<MetaEnum>,
                                        std::shared_ptr<MetaValue>>;

        MetaMember() = delete;
        MetaMember(std::string_view name, const MetaValue &value) : m_name(name), m_values() {
            auto p = std::make_shared<MetaValue>(value);
            m_values.emplace_back(std::move(p));
        }
        MetaMember(std::string_view name, const MetaStruct &value) : m_name(name), m_values() {
            auto p = std::make_shared<MetaStruct>(value);
            m_values.emplace_back(std::move(p));
        }
        MetaMember(std::string_view name, const MetaEnum &value) : m_name(name), m_values() {
            auto p = std::make_shared<MetaEnum>(value);
            m_values.emplace_back(std::move(p));
        }
        MetaMember(std::string_view name, const std::vector<MetaValue> &values)
            : m_name(name), m_values() {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaValue>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaStruct> &values)
            : m_name(name), m_values() {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaStruct>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaEnum> &values)
            : m_name(name), m_values() {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaEnum>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(const MetaMember &) = default;
        MetaMember(MetaMember &&)      = default;
        ~MetaMember()                  = default;

        [[nodiscard]] std::string rawName() const { return m_name; }
        [[nodiscard]] std::string strippedName() const;
        [[nodiscard]] std::string formattedName(int index) const;
        [[nodiscard]] QualifiedName qualifiedName() const;
        [[nodiscard]] constexpr MetaStruct *parent() const { return m_parent; }

        template <typename T> std::expected<std::weak_ptr<T>, MetaError> value(int index) const {
            return std::unexpected("Invalid type");
        }
        template <>
        std::expected<std::weak_ptr<MetaStruct>, MetaError> value<MetaStruct>(int index) const;
        template <>
        std::expected<std::weak_ptr<MetaEnum>, MetaError> value<MetaEnum>(int index) const;
        template <>
        std::expected<std::weak_ptr<MetaValue>, MetaError> value<MetaValue>(int index) const;

        [[nodiscard]] bool isTypeBitMasked() const {
            return isTypeEnum() && value<MetaEnum>(0).value().lock()->isBitMasked();
        }
        [[nodiscard]] bool isArray() const { return m_values.size() > 1; }
        [[nodiscard]] bool isTypeStruct() const {
            return std::holds_alternative<std::shared_ptr<MetaStruct>>(m_values[0]);
        }
        [[nodiscard]] bool isTypeEnum() const {
            return std::holds_alternative<std::shared_ptr<MetaEnum>>(m_values[0]);
        }
        [[nodiscard]] bool isTypeBool() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::BOOL;
        }
        [[nodiscard]] bool isTypeS8() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::S8;
        }
        [[nodiscard]] bool isTypeU8() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::U8;
        }
        [[nodiscard]] bool isTypeS16() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::S16;
        }
        [[nodiscard]] bool isTypeU16() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::U16;
        }
        [[nodiscard]] bool isTypeS32() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::S32;
        }
        [[nodiscard]] bool isTypeU32() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::U32;
        }
        [[nodiscard]] bool isTypeF32() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::F32;
        }
        [[nodiscard]] bool isTypeF64() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::F64;
        }
        [[nodiscard]] bool isTypeString() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::STRING;
        }
        [[nodiscard]] bool isTypeVec3() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::VEC3;
        }
        [[nodiscard]] bool isTypeTransform() const {
            return isTypeValue() &&
                   value<MetaValue>(0).value().lock()->type() == MetaType::TRANSFORM;
        }
        [[nodiscard]] bool isTypeComment() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::COMMENT;
        }
        [[nodiscard]] bool isTypeUnknown() const {
            return isTypeValue() && value<MetaValue>(0).value().lock()->type() == MetaType::UNKNOWN;
        }

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        bool operator==(const MetaMember &other) const;

    protected:
        constexpr bool isTypeValue() const {
            return std::holds_alternative<std::shared_ptr<MetaValue>>(m_values[0]);
        }

    private:
        std::string m_name;
        std::vector<value_type> m_values;
        MetaStruct *m_parent = nullptr;
    };

}  // namespace Toolbox::Object