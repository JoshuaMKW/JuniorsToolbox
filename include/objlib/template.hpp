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
        Template(std::string_view type, std::string_view name) : m_type(type), m_name(name) {}
        Template(std::string_view type, Deserializer &in) : m_type(type) { deserialize(in); }
        Template(const Template &) = default;
        Template(Template &&)      = default;
        ~Template()                = default;

        [[nodiscard]] std::string_view type() const { return m_type; }
        [[nodiscard]] std::string_view longName() const { return m_name; }

        std::expected<std::vector<MetaMember>, SerialError> getMembers(std::string_view wizard) const;

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

    private:
        void cacheEnums(json &enums);
        void cacheStructs(json &structs);

        std::optional<MetaMember>
        loadMemberEnum(std::string_view name, size_t array_size);

        std::optional<MetaStruct>
        loadMemberStruct(std::string_view name, size_t array_size);

        std::optional<MetaMember> loadMemberPrimitive(std::string_view name, std::string_view type,
                                                      size_t array_size);

        void loadMembers(json &members, json &structs, json &enums);
        void loadWizards(json &wizards);

    private:
        std::string m_type;
        std::string m_name;

        std::vector<MetaMember> m_members     = {};
        std::vector<TemplateWizard> m_wizards = {};

        
        std::vector<MetaStruct> m_struct_cache = {};
        std::vector<MetaEnum> m_enum_cache    = {};
    };

}  // namespace Toolbox::Object
