#pragma once

#include "fsystem.hpp"
#include "jsonlib.hpp"
#include "meta/member.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include "qualname.hpp"
#include "serial.hpp"
#include "transform.hpp"
#include "core/types.hpp"
#include <expected>
#include <format>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Toolbox::Object {

    struct TemplateRenderInfo {
        std::optional<std::string> m_file_model;
        std::optional<std::string> m_file_materials;
        std::vector<std::string> m_file_animations;
    };

    struct TemplateWizard {
        std::string m_name = "default_init";
        std::vector<MetaMember> m_init_members;
        TemplateRenderInfo m_render_info;

        TemplateWizard &operator=(const TemplateWizard &other) {
            m_name = other.m_name;
            m_init_members.clear();
            for (auto &m : other.m_init_members) {
                m_init_members.push_back(m);
            }
            m_render_info = other.m_render_info;
            return *this;
        }
    };

    class Template : public ISerializable {
    public:
        friend class TemplateFactory;

        using json_t = nlohmann::ordered_json;

        Template()                 = default;
        Template(std::string_view type);

        Template(const Template &) = default;
        Template(Template &&)      = default;
        ~Template()                = default;

    protected:
        Template(std::string_view type, Deserializer &in) : m_type(type) { deserialize(in); }

    public:
        [[nodiscard]] std::string_view type() const { return m_type; }

        [[nodiscard]] std::vector<TemplateWizard> wizards() const { return m_wizards; }

        [[nodiscard]] std::optional<TemplateWizard> getWizard() const { return m_wizards[0]; }
        [[nodiscard]] std::optional<TemplateWizard> getWizard(std::string_view name) const {
            for (const auto &wizard : m_wizards) {
                if (wizard.m_name == name) {
                    return wizard;
                }
            }
            return {};
        }

        Template &operator=(const Template &other) {
            m_type    = other.m_type;
            m_wizards = other.m_wizards;

            m_struct_cache.clear();
            for (auto &s : other.m_struct_cache) {
                m_struct_cache.push_back(s);
            }

            m_enum_cache.clear();
            for (auto &e : other.m_enum_cache) {
                m_enum_cache.push_back(e);
            }

            return *this;
        }

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::expected<void, SerialError> loadFromBlob(Deserializer &in);
        std::expected<void, SerialError> loadFromJSON(Deserializer &in);

    protected:
        void cacheEnums(json_t &enums);
        void cacheStructs(json_t &structs);

        std::optional<MetaMember> loadMemberEnum(std::string_view name, std::string_view type,
                                                 MetaMember::size_type array_size);

        std::optional<MetaMember> loadMemberStruct(std::string_view name, std::string_view type,
                                                   MetaMember::size_type array_size);

        std::optional<MetaMember> loadMemberPrimitive(std::string_view name, std::string_view type,
                                                      MetaMember::size_type array_size);

        void loadMembers(json_t &members, std::vector<MetaMember> &out);
        void loadWizards(json_t &wizards, json_t &render_infos);

    private:
        std::string m_type;

        std::vector<TemplateWizard> m_wizards = {};

        std::vector<MetaStruct> m_struct_cache = {};
        std::vector<MetaEnum> m_enum_cache     = {};
    };

    class TemplateFactory {
    public:
        using create_ret_t = ScopePtr<Template>;
        using create_err_t = std::variant<FSError, JSONError>;
        using create_t     = std::expected<create_ret_t, create_err_t>;

        // Cached create method
        static std::expected<void, FSError> initialize();
        static create_t create(std::string_view type);
        static std::vector<create_ret_t> createAll();

        static std::expected<void, SerialError> loadFromCacheBlob(Deserializer &in);
        static std::expected<void, SerialError> saveToCacheBlob(Serializer &out);
    };

}  // namespace Toolbox::Object
