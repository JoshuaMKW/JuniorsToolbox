#include "objlib/meta/enum.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include <optional>

namespace Toolbox::Object {

    std::optional<MetaEnum> Template::getEnum(const std::string &name) const {
        for (const auto &e : m_enums) {
            if (e.name() == name)
                return e;
        }
        return {};
    }

    std::optional<MetaStruct> Template::getStruct(const std::string &name) const {
        for (const auto &s : m_structs) {
            if (s.name() == name)
                return s;
        }
        return {};
    }

    std::optional<MetaMember> Template::getMember(const std::string &name) const {
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