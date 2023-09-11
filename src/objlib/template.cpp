#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include <optional>

namespace Toolbox::Object {
    constexpr QualifiedName TemplateStruct::qualifiedName() const {
        TemplateStruct *parent          = m_parent;
        std::vector<std::string> scopes = {m_name};
        while (parent) {
            scopes.push_back(parent->m_name);
            parent = parent->m_parent;
        }
        std::reverse(scopes.begin(), scopes.end());
        return QualifiedName(scopes);
    }

    void TemplateStruct::dump(std::ostream &out, int indention, int indention_width,
                              bool naked) const {
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        if (!naked)
            out << self_indent << "struct " << m_name << " {\n";
        else
            out << "{\n";
        for (const auto &m : m_members) {
            m.dump(out, indention + 1, indention_width);
        }
        out << self_indent << "}";
        if (!naked)
            out << "\n";
    }

    bool TemplateStruct::operator==(const TemplateStruct &other) const = default;

    std::optional<TemplateEnum::value_type> TemplateEnum::find(std::string_view name) const {
        for (const auto &v : m_values) {
            if (v.first == name)
                return v;
        }
        return {};
    }

    template <> std::optional<TemplateEnum::value_type> TemplateEnum::vfind<s8>(s8 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<s8>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <> std::optional<TemplateEnum::value_type> TemplateEnum::vfind<u8>(u8 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<u8>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <> std::optional<TemplateEnum::value_type> TemplateEnum::vfind<s16>(s16 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<s16>();
            if (_v && *_v == value)
                return v;
        }
        return {};
    }

    template <> std::optional<TemplateEnum::value_type> TemplateEnum::vfind<u16>(u16 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<u16>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <> std::optional<TemplateEnum::value_type> TemplateEnum::vfind<s32>(s32 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<s32>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <> std::optional<TemplateEnum::value_type> TemplateEnum::vfind<u32>(u32 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<u32>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    void TemplateEnum::dump(std::ostream &out, int indention, int indention_width) const {
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        out << self_indent << "enum " << m_name << "<" << meta_type_name(m_type) << "> {\n";
        for (const auto &v : m_values) {
            out << value_indent << v.first << " = " << v.second.toString() << ",\n";
        }
        out << self_indent << "}\n";
    }

    constexpr bool TemplateEnum::operator==(const TemplateEnum &rhs) const = default;

    std::string TemplateMember::strippedName() const {
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

    std::string TemplateMember::formattedName(int index) const {
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

    QualifiedName TemplateMember::qualifiedName() const {
        TemplateStruct *parent          = m_parent;
        std::vector<std::string> scopes = {m_name};
        while (parent) {
            scopes.push_back(std::string(parent->name()));
            parent = parent->parent();
        }
        std::reverse(scopes.begin(), scopes.end());
        return QualifiedName(scopes);
    }

    template <>
    std::expected<TemplateStruct, std::string>
    TemplateMember::value<TemplateStruct>(int index) const {
        if (index >= m_values.size())
            return std::unexpected("Index out of bounds");
        if (!isTypeStruct())
            return std::unexpected("Invalid type");
        return std::get<TemplateStruct>(m_values[index]);
    }

    template <>
    std::expected<TemplateEnum, std::string> TemplateMember::value<TemplateEnum>(int index) const {
        if (index >= m_values.size())
            return std::unexpected("Index out of bounds");
        if (!isTypeEnum())
            return std::unexpected("Invalid type");
        return std::get<TemplateEnum>(m_values[index]);
    }

    template <>
    std::expected<MetaValue, std::string> TemplateMember::value<MetaValue>(int index) const {
        if (index >= m_values.size())
            return std::unexpected("Index out of bounds");
        if (!isTypeValue())
            return std::unexpected("Invalid type");
        return std::get<MetaValue>(m_values[index]);
    }

    bool TemplateMember::operator==(const TemplateMember &other) const = default;

    void TemplateMember::dump(std::ostream &out, int indention, int indention_width) const {
        auto self_indent  = std::string(indention * indention_width, ' ');
        auto value_indent = std::string((indention + 1) * indention_width, ' ');
        if (!isArray()) {
            if (isTypeStruct()) {
                auto _struct = std::get<TemplateStruct>(m_values[0]);
                out << self_indent << _struct.name() << " " << m_name << " ";
                _struct.dump(out, indention, indention_width, true);
                out << ";\n";
            } else if (isTypeEnum()) {
                out << self_indent << std::get<TemplateEnum>(m_values[0]).name() << " " << m_name
                    << ";\n";
            } else if (isTypeValue()) {
                auto _value = std::get<MetaValue>(m_values[0]);
                out << self_indent << meta_type_name(_value.type()) << " " << m_name << " = "
                    << _value.toString() << ";\n";
            }
            return;
        }

        if (isTypeStruct()) {
            out << self_indent << std::get<TemplateStruct>(m_values[0]).name() << " " << m_name
                << " [\n";
            for (const auto &v : m_values) {
                auto _struct       = std::get<TemplateStruct>(v);
                std::cout << value_indent;
                std::get<TemplateStruct>(v).dump(out, indention + 1, indention_width, true);
                std::cout << ",\n";
            }
            out << self_indent << "];\n";
        } else if (isTypeEnum()) {
            out << self_indent << std::get<TemplateEnum>(m_values[0]).name() << " " << m_name
                << " [\n";
            for (const auto &v : m_values) {
                auto _enum = std::get<TemplateEnum>(v);
                std::cout << value_indent << _enum.name() << " " << meta_type_name(_enum.type())
                          << ";\n";
            }
            out << self_indent << "];\n";
        } else if (isTypeValue()) {
            out << self_indent << m_name << " [\n";
            for (const auto &v : m_values) {
                auto _value = std::get<MetaValue>(v);
                out << value_indent << meta_type_name(_value.type()) << m_name << " = "
                    << std::get<MetaValue>(v).toString() << ";\n";
            }
            out << self_indent << "];\n";
        }
    }

    std::optional<TemplateEnum> Template::getEnum(const std::string &name) const {
        for (const auto &e : m_enums) {
            if (e.name() == name)
                return e;
        }
        return {};
    }

    std::optional<TemplateStruct> Template::getStruct(const std::string &name) const {
        for (const auto &s : m_structs) {
            if (s.name() == name)
                return s;
        }
        return {};
    }

    std::optional<TemplateMember> Template::getMember(const std::string &name) const {
        for (const auto &m : m_members) {
            if (m.formattedName(0) == name)
                return m;
        }
        return {};
    }

    std::optional<TemplateWizard> Template::getWizard(const std::string &name) const {
        for (const auto &w : m_wizards) {
            if (w.m_name == name)
                return w;
        }
        return {};
    }

}  // namespace Toolbox::Object