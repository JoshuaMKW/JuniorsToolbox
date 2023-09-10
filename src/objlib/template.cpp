#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include <optional>

namespace Toolbox::Object {

    constexpr TemplateEnum::iterator TemplateEnum::begin() { return m_values.begin(); }
    constexpr TemplateEnum::const_iterator TemplateEnum::begin() const { return m_values.begin(); }
    constexpr TemplateEnum::const_iterator TemplateEnum::cbegin() const {
        return m_values.cbegin();
    }

    constexpr TemplateEnum::iterator TemplateEnum::end() { return m_values.end(); }
    constexpr TemplateEnum::const_iterator TemplateEnum::end() const { return m_values.end(); }
    constexpr TemplateEnum::const_iterator TemplateEnum::cend() const { return m_values.cend(); }

    constexpr TemplateEnum::reverse_iterator TemplateEnum::rbegin() { return m_values.rbegin(); }
    constexpr TemplateEnum::const_reverse_iterator TemplateEnum::rbegin() const {
        return m_values.rbegin();
    }
    constexpr TemplateEnum::const_reverse_iterator TemplateEnum::crbegin() const {
        return m_values.crbegin();
    }

    constexpr TemplateEnum::reverse_iterator TemplateEnum::rend() { return m_values.rend(); }
    constexpr TemplateEnum::const_reverse_iterator TemplateEnum::rend() const {
        return m_values.rend();
    }
    constexpr TemplateEnum::const_reverse_iterator TemplateEnum::crend() const {
        return m_values.crend();
    }

    std::optional<TemplateEnum::value_type>
    TemplateEnum::find(std::string_view name) const {
        for (const auto &v : m_values) {
            if (v.first == name)
                return v;
        }
        return {};
    }

    template <>
    std::optional<TemplateEnum::value_type> TemplateEnum::vfind<s8>(s8 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<s8>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <>
    std::optional<TemplateEnum::value_type> TemplateEnum::vfind<u8>(u8 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<u8>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <>
    std::optional<TemplateEnum::value_type> TemplateEnum::vfind<s16>(s16 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<s16>();
            if (_v && *_v == value)
                return v;
        }
        return {};
    }

    template <>
    std::optional<TemplateEnum::value_type> TemplateEnum::vfind<u16>(u16 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<u16>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <>
    std::optional<TemplateEnum::value_type> TemplateEnum::vfind<s32>(s32 value) const {
        for (const auto &v : m_values) {
            auto _v = v.second.get<s32>();
            if (_v.has_value() && *_v == value)
                return v;
        }
        return {};
    }

    template <>
    std::optional<TemplateEnum::value_type> TemplateEnum::vfind<u32>(u32 value) const {
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
        out << self_indent << "enum " << m_name << "<" << template_type_name(m_type) << "> {\n";
        for (const auto &v : m_values) {
            out << value_indent << v.first << " = ";
            switch (m_type) {
            case TemplateType::S8:
                out << static_cast<s32>(v.second.get<s8>().value());
                break;
            case TemplateType::U8:
                out << static_cast<u32>(v.second.get<u8>().value());
                break;
            case TemplateType::S16:
                out << static_cast<s32>(v.second.get<s16>().value());
                break;
            case TemplateType::U16:
                out << static_cast<u32>(v.second.get<u16>().value());
                break;
            case TemplateType::S32:
                out << v.second.get<s32>().value();
                break;
            case TemplateType::U32:
                out << v.second.get<u32>().value();
                break;
            default:
                out << "null";
                break;
            }
            out << ",\n";
        }
        out << self_indent << "}\n";
    }

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