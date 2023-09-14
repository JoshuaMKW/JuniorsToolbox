#pragma once

#include "json.hpp"
#include "meta/member.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include "qualname.hpp"
#include "serial.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using json = nlohmann::json;

namespace Toolbox::Object {

    struct TemplateWizard {
        std::string m_name;
        std::vector<MetaMember> m_init_members;
    };

    class Template : public ISerializable {
    public:
        Template() = delete;
        Template(std::string_view type, std::string_view name)
            : m_type(type), m_name(name) {}
        Template(std::string_view type, Deserializer &in) : m_type(type) { deserialize(in); } 
        Template(const Template &) = default;
        Template(Template &&)      = default;
        ~Template()                = default;

        [[nodiscard]] std::string_view name() const { return m_type; }
        [[nodiscard]] std::string_view longName() const { return m_name; }

        [[nodiscard]] std::optional<MetaEnum> getEnum(const std::string &name) const;
        [[nodiscard]] std::optional<MetaStruct> getStruct(const std::string &name) const;
        [[nodiscard]] std::optional<MetaMember> getMember(const std::string &name) const;
        [[nodiscard]] std::optional<TemplateWizard> getWizard(const std::string &name) const;

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

    private:
        std::string m_type;
        std::string m_name;
        std::vector<MetaEnum> m_enums         = {};
        std::vector<MetaStruct> m_structs     = {};
        std::vector<MetaMember> m_members     = {};
        std::vector<TemplateWizard> m_wizards = {};
    };

}  // namespace Toolbox::Object
