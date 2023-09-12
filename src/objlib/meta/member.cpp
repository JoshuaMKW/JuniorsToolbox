#include "objlib/meta/member.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include <algorithm>
#include <optional>

namespace Toolbox::Object {

    std::string MetaMember::strippedName() const {
        std::string result = m_name;
        {
            size_t pos = 0;
            while ((pos = result.find("{c}")) != std::string::npos) {
                result.replace(pos, 3, "");
            }
        }

        {
            size_t pos = 0;
            while ((pos = result.find("{C}")) != std::string::npos) {
                result.replace(pos, 3, "");
            }
        }

        {
            size_t pos = 0;
            while ((pos = result.find("{i}")) != std::string::npos) {
                result.replace(pos, 3, "");
            }
        }

        return result;
    }

    std::string MetaMember::formattedName(int index) const {
        std::string chars = "";

        // Append a-z depending on the index
        int charIndex = index;
        for (int i = 0; i < 4; ++i) {
            chars += static_cast<char>(('a' + charIndex) % 26);
            if (charIndex < 26)
                break;
            charIndex /= 26;
            charIndex -= 1;
        }

        std::reverse(chars.begin(), chars.end());

        std::string uchars = chars;
        std::transform(uchars.begin(), uchars.end(), uchars.begin(), ::toupper);

        // Find and replace all {c} with the chars
        std::string result = m_name;
        {
            size_t pos = 0;
            while ((pos = result.find("{c}")) != std::string::npos) {
                result.replace(pos, 3, chars);
            }
        }

        // Find and replace all {C} with the uppercase chars
        {
            size_t pos = 0;
            while ((pos = result.find("{C}")) != std::string::npos) {
                result.replace(pos, 3, uchars);
            }
        }

        // Find and replace all {i} with the index
        {
            size_t pos    = 0;
            auto indexstr = std::to_string(index);
            while ((pos = result.find("{i}")) != std::string::npos) {
                result.replace(pos, 3, indexstr);
            }
        }

        return result;
    }

    QualifiedName MetaMember::qualifiedName() const {
        MetaStruct *parent              = m_parent;
        std::vector<std::string> scopes = {m_name};
        while (parent) {
            scopes.push_back(std::string(parent->name()));
            parent = parent->parent();
        }
        std::reverse(scopes.begin(), scopes.end());
        return QualifiedName(scopes);
    }

    template <>
    std::expected<std::weak_ptr<MetaStruct>, MetaError>
    MetaMember::value<MetaStruct>(int index) const {
        if (index >= m_values.size() || index < 0) {
            return make_meta_error<std::weak_ptr<MetaStruct>>(m_name, index, m_values.size());
        }
        if (!isTypeValue()) {
            return make_meta_error<std::weak_ptr<MetaStruct>>(
                m_name, "MetaStruct", isTypeValue() ? "MetaValue" : "MetaEnum");
        }
        return std::get<std::shared_ptr<MetaStruct>>(m_values[index]);
    }

    template <>
    std::expected<std::weak_ptr<MetaEnum>, MetaError> MetaMember::value<MetaEnum>(int index) const {
        if (index >= m_values.size() || index < 0) {
            return make_meta_error<std::weak_ptr<MetaEnum>>(m_name, index, m_values.size());
        }
        if (!isTypeValue()) {
            return make_meta_error<std::weak_ptr<MetaEnum>>(
                m_name, "MetaEnum", isTypeValue() ? "MetaValue" : "MetaStruct");
        }
        return std::get<std::shared_ptr<MetaEnum>>(m_values[index]);
    }

    template <>
    std::expected<std::weak_ptr<MetaValue>, MetaError>
    MetaMember::value<MetaValue>(int index) const {
        if (index >= m_values.size() || index < 0) {
            return make_meta_error<std::weak_ptr<MetaValue>>(m_name, index, m_values.size());
        }
        if (!isTypeValue()) {
            return make_meta_error<std::weak_ptr<MetaValue>>(
                m_name, "MetaValue", isTypeStruct() ? "MetaStruct" : "MetaEnum");
        }
        return std::get<std::shared_ptr<MetaValue>>(m_values[index]);
    }

    bool MetaMember::operator==(const MetaMember &other) const = default;

    void MetaMember::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        indention_width   = std::min(indention_width, size_t(8));
        auto self_indent  = std::string(indention * indention_width, ' ');
        auto value_indent = std::string((indention + 1) * indention_width, ' ');
        if (!isArray()) {
            if (isTypeStruct()) {
                auto _struct = std::get<std::shared_ptr<MetaStruct>>(m_values[0]);
                out << self_indent << _struct->name() << " " << m_name << " ";
                _struct->dump(out, indention, indention_width, true);
                out << ";\n";
            } else if (isTypeEnum()) {
                auto _enum = std::get<std::shared_ptr<MetaEnum>>(m_values[0]);
                out << self_indent << _enum->name() << " " << m_name << " = "
                    << _enum->value().toString() << ";\n";
            } else if (isTypeValue()) {
                auto _value = std::get<std::shared_ptr<MetaValue>>(m_values[0]);
                out << self_indent << meta_type_name(_value->type()) << " " << m_name << " = "
                    << _value->toString() << ";\n";
            }
            return;
        }

        if (isTypeStruct()) {
            out << self_indent << std::get<std::shared_ptr<MetaStruct>>(m_values[0])->name() << " "
                << m_name
                << " = [\n";
            for (const auto &v : m_values) {
                auto _struct = std::get<std::shared_ptr<MetaStruct>>(v);
                std::cout << value_indent;
                std::get<std::shared_ptr<MetaStruct>>(v)->dump(out, indention + 1, indention_width,
                                                              true);
                std::cout << ",\n";
            }
            out << self_indent << "];\n";
        } else if (isTypeEnum()) {
            out << self_indent << std::get<std::shared_ptr<MetaEnum>>(m_values[0])->name() << " "
                << m_name
                << " = [\n";
            for (const auto &v : m_values) {
                auto _enum = std::get<std::shared_ptr<MetaEnum>>(v);
                out << _enum->value().toString() << ",\n";
            }
            out << self_indent << "];\n";
        } else if (isTypeValue()) {
            out << self_indent
                << meta_type_name(std::get<std::shared_ptr<MetaEnum>>(m_values[0])->type()) << m_name
                << " = [\n";
            for (const auto &v : m_values) {
                auto _value = std::get<std::shared_ptr<MetaValue>>(v);
                out << value_indent << meta_type_name(_value->type()) << m_name << " = "
                    << std::get<std::shared_ptr<MetaValue>>(v)->toString() << ",\n";
            }
            out << self_indent << "];\n";
        }
    }

}  // namespace Toolbox::Object