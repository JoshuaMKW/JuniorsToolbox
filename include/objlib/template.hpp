#pragma once

#include "json.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include "qualname.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "meta/member.hpp"

using json = nlohmann::json;

namespace Toolbox::Object {

    struct TemplateWizard {
        std::string m_name;
        std::vector<MetaMember> m_init_members;
    };

    class Template {
    public:
        Template() = delete;
        Template(std::string_view name, std::string_view long_name)
            : m_name(name), m_long_name(long_name) {}
        ~Template() = default;

        [[nodiscard]] std::string_view name() const { return m_name; }
        [[nodiscard]] std::string_view longName() const { return m_long_name; }

        [[nodiscard]] std::optional<MetaEnum> getEnum(const std::string &name) const;
        [[nodiscard]] std::optional<MetaStruct> getStruct(const std::string &name) const;
        [[nodiscard]] std::optional<MetaMember> getMember(const std::string &name) const;
        [[nodiscard]] std::optional<TemplateWizard> getWizard(const std::string &name) const;

    private:
        std::string m_name;
        std::string m_long_name;
        std::vector<MetaEnum> m_enums     = {};
        std::vector<MetaStruct> m_structs = {};
        std::vector<MetaMember> m_members = {};
        std::vector<TemplateWizard> m_wizards = {};
    };

}  // namespace Toolbox::Object
