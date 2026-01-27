#pragma once

#include <expected>
#include <format>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/types.hpp"
#include "fsystem.hpp"
#include "jsonlib.hpp"
#include "meta/member.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include "qualname.hpp"
#include "serial.hpp"
#include "transform.hpp"

namespace Toolbox::Object {

    struct TemplateRenderInfo {
        std::optional<std::string> m_file_model;
        std::optional<std::string> m_file_materials;
        std::vector<std::string> m_file_animations;
        std::unordered_map<std::string, std::string> m_texture_swap_map;
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

    struct TemplateDependencies {
        struct ObjectInfo {
            std::string m_type;
            std::string m_name;
            QualifiedName m_ancestry;
        };

        std::vector<ObjectInfo> m_managers;
        std::vector<std::string> m_asset_paths;
        std::vector<ObjectInfo> m_table_objs;
    };

    class Template : public ISerializable {
    public:
        friend class TemplateFactory;

        using json_t = nlohmann::ordered_json;

        Template() = default;
        Template(std::string_view type, bool is_custom);

        Template(const Template &) = default;
        Template(Template &&)      = default;
        ~Template()                = default;

    protected:
        Template(std::string_view type, Deserializer &in) : m_type(type) { deserialize(in); }

    public:
        [[nodiscard]] std::string_view type() const { return m_type; }

        [[nodiscard]] const TemplateDependencies &dependencies() const {
            return m_dependencies;
        }

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

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, JSONError> loadFromJSON(const json_t &the_json);

    protected:
        Result<void, JSONError> cacheEnums(const json_t &enums);
        Result<void, JSONError> cacheStructs(const json_t &structs);

        std::optional<MetaMember> loadMemberEnum(std::string_view name, std::string_view type,
                                                 MetaMember::size_type array_size);

        std::optional<MetaMember> loadMemberStruct(std::string_view name, std::string_view type,
                                                   MetaMember::size_type array_size);

        std::optional<MetaMember>
        loadMemberPrimitive(std::string_view name, std::string_view type,
                            MetaMember::size_type array_size,
                            const std::variant<s64, u64, float, double> &var_min,
                            const std::variant<s64, u64, float, double> &var_max);

        Result<TemplateDependencies, JSONError> loadDependencies(const json_t &dependencies);
        Result<void, JSONError> loadMembers(const json_t &members, std::vector<MetaMember> &out);
        Result<void, JSONError> loadWizards(const json_t &wizards, const json_t &render_infos);

        static void threadLoadTemplate(const std::string &type, bool is_custom);
        static void threadLoadTemplateBlob(const std::string &type, const json_t &the_json,
                                           bool is_custom);

    private:
        std::string m_type;

        TemplateDependencies m_dependencies;
        std::vector<TemplateWizard> m_wizards   = {};

        std::vector<MetaStruct> m_struct_cache = {};
        std::vector<MetaEnum> m_enum_cache     = {};
    };

    class TemplateFactory {
    public:
        using create_ret_t = ScopePtr<Template>;
        using create_err_t = std::variant<FSError, JSONError>;
        using create_t     = Result<create_ret_t, create_err_t>;

        // Cached create method
        static Result<void, FSError> initialize(const fs_path &cache_path);
        static create_t create(std::string_view type, bool include_custom);
        static std::vector<create_ret_t> createAll(bool include_custom);

        static Result<void, FSError> loadFromCacheBlob(bool is_custom);
        static Result<void, FSError> saveToCacheBlob(bool is_custom);

        static bool isCacheMode();
        static void setCacheMode(bool mode);
    };

}  // namespace Toolbox::Object
