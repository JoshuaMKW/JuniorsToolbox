#pragma once

#include "clone.hpp"
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

    inline std::string makeNameArrayIndex(std::string_view name, size_t index) {
        return std::format("{}[{}]", name, index);
    }

    inline void makeNameArrayIndex(QualifiedName &name, size_t scopeidx, size_t index) {
        if (scopeidx >= name.depth())
            return;
        auto name_str  = std::string(name[scopeidx]);
        name[scopeidx] = makeNameArrayIndex(std::string_view(name_str), index);
    }

    inline std::expected<size_t, MetaScopeError> getArrayIndex(const QualifiedName &name,
                                                               size_t scopeidx) {
        auto name_str = name[scopeidx];

        auto pos = name_str.find('[');
        if (pos == std::string_view::npos)
            return std::string_view::npos;

        auto end = name_str.find(']', pos);
        if (end == std::string_view::npos) {
            return make_meta_error<size_t>(name.toString(),
                                           name.getAbsIndexOf(scopeidx, static_cast<int>(pos)),
                                           "Array specifier missing end token `]'");
        }

        auto index = name_str.substr(pos + 1, end - pos - 1);
        return std::stoi(std::string(index), nullptr, 0);
    }

    inline std::expected<size_t, MetaScopeError> getArrayIndex(std::string_view name) {
        return getArrayIndex(name, 0);
    }

    class MetaMember : public IClonable {
    public:
        using value_type = std::variant<std::shared_ptr<MetaStruct>, std::shared_ptr<MetaEnum>,
                                        std::shared_ptr<MetaValue>>;
        using size_type  = std::variant<size_t, std::shared_ptr<MetaValue>>;

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
            : m_name(name), m_values(), m_arraysize(values.size()) {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaValue>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaStruct> &values)
            : m_name(name), m_values(), m_arraysize(values.size()) {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaStruct>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaEnum> &values)
            : m_name(name), m_values(), m_arraysize(values.size()) {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaEnum>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaValue> &values,
                   std::shared_ptr<MetaValue> arraysize)
            : m_name(name), m_values(), m_arraysize(arraysize) {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaValue>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaStruct> &values,
                   std::shared_ptr<MetaValue> arraysize)
            : m_name(name), m_values(), m_arraysize(arraysize) {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaStruct>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaEnum> &values,
                   std::shared_ptr<MetaValue> arraysize)
            : m_name(name), m_values(), m_arraysize(arraysize) {
            for (const auto &value : values) {
                auto p = std::make_shared<MetaEnum>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(const MetaMember &) = default;
        MetaMember(MetaMember &&)      = default;
        ~MetaMember()                  = default;

    protected:
        MetaMember() = default;

    public:
        [[nodiscard]] constexpr std::string name() const { return m_name; }
        [[nodiscard]] constexpr MetaStruct *parent() const { return m_parent; }

        [[nodiscard]] QualifiedName qualifiedName() const;

        template <typename T>
        std::expected<std::shared_ptr<T>, MetaError> value(size_t index) const {
            return std::unexpected("Invalid type");
        }
        template <>
        std::expected<std::shared_ptr<MetaStruct>, MetaError> value<MetaStruct>(size_t index) const;
        template <>
        std::expected<std::shared_ptr<MetaEnum>, MetaError> value<MetaEnum>(size_t index) const;
        template <>
        std::expected<std::shared_ptr<MetaValue>, MetaError> value<MetaValue>(size_t index) const;

        [[nodiscard]] size_t arraysize() const {
            if (std::holds_alternative<std::shared_ptr<MetaValue>>(m_arraysize)) {
                auto vptr = std::get<std::shared_ptr<MetaValue>>(m_arraysize);
                auto size = vptr->get<size_t>();
                if (!size)
                    return 0;
                return *size;
            }
            return std::get<size_t>(m_arraysize);
        }
        [[nodiscard]] bool isEmpty() const { return m_values.empty(); }
        [[nodiscard]] bool isArray() const { return m_values.size() > 1; }
        [[nodiscard]] bool isTypeBitMasked() const {
            return !isEmpty() && isTypeEnum() && value<MetaEnum>(0).value()->isBitMasked();
        }
        [[nodiscard]] bool isTypeStruct() const {
            return !isEmpty() && std::holds_alternative<std::shared_ptr<MetaStruct>>(m_values[0]);
        }
        [[nodiscard]] bool isTypeEnum() const {
            return !isEmpty() && std::holds_alternative<std::shared_ptr<MetaEnum>>(m_values[0]);
        }
        [[nodiscard]] bool isTypeBool() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::BOOL;
        }
        [[nodiscard]] bool isTypeS8() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::S8;
        }
        [[nodiscard]] bool isTypeU8() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::U8;
        }
        [[nodiscard]] bool isTypeS16() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::S16;
        }
        [[nodiscard]] bool isTypeU16() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::U16;
        }
        [[nodiscard]] bool isTypeS32() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::S32;
        }
        [[nodiscard]] bool isTypeU32() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::U32;
        }
        [[nodiscard]] bool isTypeF32() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::F32;
        }
        [[nodiscard]] bool isTypeF64() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::F64;
        }
        [[nodiscard]] bool isTypeString() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::STRING;
        }
        [[nodiscard]] bool isTypeVec3() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::VEC3;
        }
        [[nodiscard]] bool isTypeTransform() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::TRANSFORM;
        }
        [[nodiscard]] bool isTypeRGB() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::RGB;
        }
        [[nodiscard]] bool isTypeRGBA() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::RGBA;
        }
        [[nodiscard]] bool isTypeComment() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::COMMENT;
        }
        [[nodiscard]] bool isTypeUnknown() const {
            return !isEmpty() && isTypeValue() &&
                   value<MetaValue>(0).value()->type() == MetaType::UNKNOWN;
        }

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        bool operator==(const MetaMember &other) const;

        std::unique_ptr<IClonable> clone(bool deep) const override;

    protected:
        constexpr bool isTypeValue() const {
            return std::holds_alternative<std::shared_ptr<MetaValue>>(m_values[0]);
        }

        void syncArray() {
            size_t asize = arraysize();
            if (m_values.size() != asize)
                m_values.resize(asize);
        }

        bool validateIndex(size_t index) const {
            return index < m_values.size();
        }

    private:
        std::string m_name;
        std::vector<value_type> m_values;
        size_type m_arraysize;
        MetaStruct *m_parent = nullptr;
    };

}  // namespace Toolbox::Object