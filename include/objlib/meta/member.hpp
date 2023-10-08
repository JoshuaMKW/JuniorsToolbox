#pragma once

#include "clone.hpp"
#include "enum.hpp"
#include "errors.hpp"
#include "objlib/qualname.hpp"
#include "serial.hpp"
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

    class MetaMember : public ISerializable, public IClonable {
    public:
        struct ReferenceInfo {
            std::shared_ptr<MetaValue> m_ref;
            std::string m_name;

            bool operator==(const ReferenceInfo &other) const {
                return m_ref == other.m_ref && m_name == other.m_name;
            }
        };

        using value_type = std::variant<std::shared_ptr<MetaStruct>, std::shared_ptr<MetaEnum>,
                                        std::shared_ptr<MetaValue>>;
        using const_value_type =
            std::variant<std::shared_ptr<const MetaStruct>, std::shared_ptr<const MetaEnum>,
                         std::shared_ptr<const MetaValue>>;
        using size_type = std::variant<u32, ReferenceInfo>;

        MetaMember(std::string_view name, const ReferenceInfo &arraysize, value_type default_value)
            : m_name(name), m_values(), m_arraysize(arraysize), m_default(default_value) {}
        MetaMember(std::string_view name, const MetaValue &value)
            : m_name(name), m_values(), m_arraysize(static_cast<u32>(1)) {
            auto p = std::make_shared<MetaValue>(value);
            m_values.emplace_back(std::move(p));
            m_default = std::make_shared<MetaValue>(value);
        }
        MetaMember(std::string_view name, const MetaStruct &value)
            : m_name(name), m_values(), m_arraysize(static_cast<u32>(1)) {
            auto p = std::make_shared<MetaStruct>(value);
            m_values.emplace_back(std::move(p));
            m_default = std::make_shared<MetaStruct>(value);
        }
        MetaMember(std::string_view name, const MetaEnum &value)
            : m_name(name), m_values(), m_arraysize(static_cast<u32>(1)) {
            auto p = std::make_shared<MetaEnum>(value);
            m_values.emplace_back(std::move(p));
            m_default = std::make_shared<MetaEnum>(value);
        }
        MetaMember(std::string_view name, const std::vector<MetaValue> &values,
                   value_type default_value)
            : m_name(name), m_values(), m_arraysize(static_cast<u32>(values.size())),
              m_default(default_value) {
            if (values.empty())
                return;
            for (const auto &value : values) {
                auto p = std::make_shared<MetaValue>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaStruct> &values,
                   value_type default_value)
            : m_name(name), m_values(), m_arraysize(static_cast<u32>(values.size())),
              m_default(default_value) {
            if (values.empty())
                return;
            for (const auto &value : values) {
                auto p = std::make_shared<MetaStruct>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaEnum> &values,
                   value_type default_value)
            : m_name(name), m_values(), m_arraysize(static_cast<u32>(values.size())),
              m_default(default_value) {
            if (values.empty())
                return;
            for (const auto &value : values) {
                auto p = std::make_shared<MetaEnum>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaValue> &values,
                   const ReferenceInfo &arraysize, value_type default_value)
            : m_name(name), m_values(), m_arraysize(arraysize), m_default(default_value) {
            if (values.empty())
                return;
            for (const auto &value : values) {
                auto p = std::make_shared<MetaValue>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaStruct> &values,
                   const ReferenceInfo &arraysize, value_type default_value)
            : m_name(name), m_values(), m_arraysize(arraysize), m_default(default_value) {
            if (values.empty())
                return;
            for (const auto &value : values) {
                auto p = std::make_shared<MetaStruct>(value);
                m_values.emplace_back(std::move(p));
            }
        }
        MetaMember(std::string_view name, const std::vector<MetaEnum> &values,
                   const ReferenceInfo &arraysize, value_type default_value)
            : m_name(name), m_values(), m_arraysize(arraysize), m_default(default_value) {
            if (values.empty())
                return;
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
        [[nodiscard]] std::expected<std::shared_ptr<T>, MetaError> value(size_t index) const {
            return std::unexpected("Invalid type");
        }

        [[nodiscard]] value_type defaultValue() const { return m_default; }

        [[nodiscard]] u32 arraysize() const {
            if (std::holds_alternative<ReferenceInfo>(m_arraysize)) {
                auto vptr = std::get<ReferenceInfo>(m_arraysize);
                auto size = vptr.m_ref->get<u32>();
                if (!size)
                    return 0;
                return *size;
            }
            return std::get<u32>(m_arraysize);
        }
        [[nodiscard]] bool isEmpty() const { return m_values.empty(); }
        [[nodiscard]] bool isArray() const { return m_values.size() > 1; }
        [[nodiscard]] bool isTypeBitMasked() const {
            return isTypeEnum() &&
                   std::get<std::shared_ptr<MetaEnum>>(m_default)->isBitMasked();
        }
        [[nodiscard]] bool isTypeStruct() const {
            return std::holds_alternative<std::shared_ptr<MetaStruct>>(m_default);
        }
        [[nodiscard]] bool isTypeEnum() const {
            return std::holds_alternative<std::shared_ptr<MetaEnum>>(m_default);
        }
        [[nodiscard]] bool isTypeBool() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::BOOL;
        }
        [[nodiscard]] bool isTypeS8() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::S8;
        }
        [[nodiscard]] bool isTypeU8() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::U8;
        }
        [[nodiscard]] bool isTypeS16() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::S16;
        }
        [[nodiscard]] bool isTypeU16() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::U16;
        }
        [[nodiscard]] bool isTypeS32() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::S32;
        }
        [[nodiscard]] bool isTypeU32() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::U32;
        }
        [[nodiscard]] bool isTypeF32() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::F32;
        }
        [[nodiscard]] bool isTypeF64() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::F64;
        }
        [[nodiscard]] bool isTypeString() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::STRING;
        }
        [[nodiscard]] bool isTypeVec3() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::VEC3;
        }
        [[nodiscard]] bool isTypeTransform() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::TRANSFORM;
        }
        [[nodiscard]] bool isTypeRGB() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::RGB;
        }
        [[nodiscard]] bool isTypeRGBA() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::RGBA;
        }
        [[nodiscard]] bool isTypeComment() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::COMMENT;
        }
        [[nodiscard]] bool isTypeUnknown() const {
            return isTypeValue() &&
                   std::get<std::shared_ptr<MetaValue>>(m_default)->type() == MetaType::UNKNOWN;
        }

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        bool operator==(const MetaMember &other) const;

        void updateReferenceToList(const std::vector<std::shared_ptr<MetaMember>> &list);

        void syncArray() {
            size_t asize = arraysize();
            if (m_values.size() == asize)
                return;

            if (m_values.size() > asize) {
                m_values.resize(asize);
                return;
            }

            for (size_t i = m_values.size(); i < asize; ++i) {
                if (std::holds_alternative<std::shared_ptr<MetaValue>>(m_default)) {
                    m_values.emplace_back(std::make_shared<MetaValue>(
                        *std::get<std::shared_ptr<MetaValue>>(m_default)));
                } else if (std::holds_alternative<std::shared_ptr<MetaEnum>>(m_default)) {
                    m_values.emplace_back(std::make_shared<MetaEnum>(
                        *std::get<std::shared_ptr<MetaEnum>>(m_default)));
                } else {
                    m_values.emplace_back(make_deep_clone<MetaStruct>(
                        std::get<std::shared_ptr<MetaStruct>>(m_default)));
                }
            }
        }

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override;

    protected:
        bool isTypeValue() const {
            return !isEmpty() && std::holds_alternative<std::shared_ptr<MetaValue>>(m_values[0]);
        }

        bool validateIndex(size_t index) const { return static_cast<u32>(index) < m_values.size(); }

    private:
        std::string m_name;
        std::vector<value_type> m_values;
        size_type m_arraysize;
        MetaMember::value_type m_default;
        MetaStruct *m_parent = nullptr;
    };

    template <>
    [[nodiscard]] inline std::expected<std::shared_ptr<MetaStruct>,
                                MetaError>
    MetaMember::value<MetaStruct>(size_t index) const {
        if (!validateIndex(index)) {
            return make_meta_error<std::shared_ptr<MetaStruct>>(m_name, index, m_values.size());
        }
        if (!isTypeStruct()) {
            return make_meta_error<std::shared_ptr<MetaStruct>>(
                m_name, "MetaStruct", isTypeValue() ? "MetaValue" : "MetaEnum");
        }
        return std::get<std::shared_ptr<MetaStruct>>(m_values[index]);
    }
    template <>
    [[nodiscard]] inline std::expected<std::shared_ptr<MetaEnum>,
                                MetaError>
    MetaMember::value<MetaEnum>(size_t index) const {
        if (!validateIndex(index)) {
            return make_meta_error<std::shared_ptr<MetaEnum>>(m_name, index, m_values.size());
        }
        if (!isTypeEnum()) {
            return make_meta_error<std::shared_ptr<MetaEnum>>(
                m_name, "MetaEnum", isTypeValue() ? "MetaValue" : "MetaStruct");
        }
        return std::get<std::shared_ptr<MetaEnum>>(m_values[index]);
    }
    template <>
    [[nodiscard]] inline std::expected<std::shared_ptr<MetaValue>,
                                MetaError>
    MetaMember::value<MetaValue>(size_t index) const {
        if (!validateIndex(index)) {
            return make_meta_error<std::shared_ptr<MetaValue>>(m_name, index, m_values.size());
        }
        if (!isTypeValue()) {
            return make_meta_error<std::shared_ptr<MetaValue>>(
                m_name, "MetaValue", isTypeStruct() ? "MetaStruct" : "MetaEnum");
        }
        return std::get<std::shared_ptr<MetaValue>>(m_values[index]);
    }

}  // namespace Toolbox::Object