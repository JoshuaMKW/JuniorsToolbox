#include <execution>
#include <expected>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>

#include <glm/glm.hpp>

#include "color.hpp"
#include "core/log.hpp"
#include "gui/appmain/settings/settings.hpp"
#include "gui/logging/errors.hpp"
#include "jsonlib.hpp"
#include "magic_enum.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/template.hpp"
#include "objlib/transform.hpp"

namespace Toolbox::Object {

    static std::string ResolveMetaTypeAlias(const std::string &alias) {
        static const std::unordered_map<std::string, std::string> s_alias_map = {
            {"BYTE",   "S8"  },
            {"SHORT",  "S16" },
            {"INT",    "S32" },
            {"LONG",   "S64" },
            {"UBYTE",  "U8"  },
            {"USHORT", "U16" },
            {"UINT",   "U32" },
            {"ULONG",  "U64" },
            {"FLOAT",  "F32" },
            {"DOUBLE", "F64" },
            {"VEC3F",  "VEC3"},
        };

        if (s_alias_map.contains(alias)) {
            return s_alias_map.at(alias);
        }

        return alias;
    }

    Template::Template(std::string_view type, bool include_custom) : m_type(type) {
        if (include_custom) {
            fs_path f_path = "./Templates/Custom/" + std::string(type) + ".json";
            std::ifstream file(f_path, std::ios::in);
            if (file.is_open()) {
                Deserializer in(file.rdbuf());
                auto result = deserialize(in);
                if (result) {
                    return;
                }
            }
        }

        fs_path f_path = "./Templates/Vanilla/" + std::string(type) + ".json";
        std::ifstream file(f_path, std::ios::in);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open template file: " + std::string(type));
        }

        Deserializer in(file.rdbuf());
        auto result = deserialize(in);
        if (!result) {
            throw std::runtime_error("Failed to deserialize template: " + std::string(type));
        }
    }

    Result<void, SerialError> Template::serialize(Serializer &out) const { return {}; }

    Result<void, SerialError> Template::deserialize(Deserializer &in) {
        json_t template_json;

        auto result = tryJSON(template_json, [&](json_t &j) {
            in.stream() >> j;

            for (auto &item : j.items()) {
                const json_t &metadata    = item.value();
                const json_t &member_data = metadata["Members"];
                const json_t &struct_data = metadata["Structs"];
                const json_t &enum_data   = metadata["Enums"];

                if (!member_data.is_object()) {
                    return make_json_error<void>("TEMPLATE",
                                                 "Template JSON is missing 'Members' object.", 0);
                }

                if (!struct_data.is_object()) {
                    return make_json_error<void>("TEMPLATE",
                                                 "Template JSON is missing 'Structs' object.", 0);
                }

                if (!enum_data.is_object()) {
                    return make_json_error<void>("TEMPLATE",
                                                 "Template JSON is missing 'Enums' object.", 0);
                }

                TemplateWizard default_wizard;

                cacheEnums(enum_data);
                cacheStructs(struct_data);
                loadMembers(member_data, default_wizard.m_init_members);

                m_wizards.push_back(default_wizard);

                auto rendering_it = metadata.find("Rendering");
                loadWizards(metadata["Wizard"],
                            rendering_it == metadata.end() ? json_t() : metadata["Rendering"]);
                break;
            }

            return Result<void, JSONError>();
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           in.filepath());
        }

        return {};
    }

    Result<void, JSONError> Template::loadFromJSON(const json_t &the_json) {
        auto result = tryJSON(the_json, [&](const json_t &j) {
            const json_t &member_data = j["Members"];
            const json_t &struct_data = j["Structs"];
            const json_t &enum_data   = j["Enums"];

            if (!member_data.is_object()) {
                return make_json_error<void>("TEMPLATE",
                                             "Template JSON is missing 'Members' object.", 0);
            }

            if (!struct_data.is_object()) {
                return make_json_error<void>("TEMPLATE",
                                             "Template JSON is missing 'Structs' object.", 0);
            }

            if (!enum_data.is_object()) {
                return make_json_error<void>("TEMPLATE", "Template JSON is missing 'Enums' object.",
                                             0);
            }

            TemplateWizard default_wizard;

            cacheEnums(enum_data);
            cacheStructs(struct_data);
            loadMembers(member_data, default_wizard.m_init_members);

            m_wizards.push_back(default_wizard);

            auto rendering_it = j.find("Rendering");
            loadWizards(j["Wizard"], rendering_it == j.end() ? json_t() : j["Rendering"]);

            return Result<void, JSONError>();
        });

        if (!result) {
            return std::unexpected(result.error());
        }

        return {};
    }

    Result<void, JSONError> Template::cacheEnums(const json_t &enums) {
        for (const auto &item : enums.items()) {
            const json_t &e = item.value();
            if (!e.contains("Type")) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum definition is missing 'Type' field for enum: " + item.key(),
                    0);
            }

            const json_t &enum_type_json = e["Type"];
            if (!enum_type_json.is_string()) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum 'Type' field is not a string for enum: " + item.key(), 0);
            }

            if (!e.contains("Flags")) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum definition is missing 'Flags' field for enum: " + item.key(),
                    0);
            }

            const json_t &enum_flags_json = e["Flags"];
            if (!enum_flags_json.is_object()) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum 'Flags' field is not an object for enum: " + item.key(), 0);
            }

            if (!e.contains("Multi")) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum definition is missing 'Multi' field for enum: " + item.key(),
                    0);
            }

            const json_t &enum_multi_json = e["Multi"];
            if (!enum_multi_json.is_boolean()) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum 'Multi' field is not a boolean for enum: " + item.key(), 0);
            }

            std::string member_type = ResolveMetaTypeAlias(enum_type_json.get<std::string>());

            auto enum_type = magic_enum::enum_cast<MetaType>(member_type);
            if (!enum_type.has_value()) {
                continue;
            }

            const bool enum_bitmask = enum_multi_json.get<bool>();

            std::vector<MetaEnum::enum_type> values;
            values.reserve(enum_flags_json.size());

            for (auto &value : enum_flags_json.items()) {
                std::string value_str = value.value().get<std::string>();
                MetaValue meta_value(enum_type.value());
                switch (enum_type.value()) {
                case MetaType::S8:
                    meta_value.set(static_cast<s8>(std::stoi(value_str, nullptr, 0)));
                    break;
                case MetaType::U8:
                    meta_value.set(static_cast<u8>(std::stoi(value_str, nullptr, 0)));
                    break;
                case MetaType::S16:
                    meta_value.set(static_cast<s16>(std::stoi(value_str, nullptr, 0)));
                    break;
                case MetaType::U16:
                    meta_value.set(static_cast<u16>(std::stoi(value_str, nullptr, 0)));
                    break;
                case MetaType::S32:
                    meta_value.set(static_cast<s32>(std::stoi(value_str, nullptr, 0)));
                    break;
                case MetaType::U32:
                    meta_value.set(static_cast<u32>(std::stoi(value_str, nullptr, 0)));
                    break;
                }
                MetaEnum::enum_type enumv = {value.key(), meta_value};
                values.emplace_back(value.key(), meta_value);
            }
            m_enum_cache.emplace_back(item.key(), enum_type.value(), values, enum_bitmask);
        }
        return {};
    }

    Result<void, JSONError> Template::cacheStructs(const json_t &structs) {
        for (const auto &item : structs.items()) {
            const std::string &name    = item.key();
            const json_t &json_members = item.value();

            std::vector<MetaMember> members;
            Result<void, JSONError> result = loadMembers(json_members, members);
            if (!result) {
                return result;
            }

            m_struct_cache.emplace_back(item.key(), members);
        }
        return {};
    }

    std::optional<MetaMember> Template::loadMemberEnum(std::string_view name, std::string_view type,
                                                       MetaMember::size_type array_size) {
        auto enum_ = std::find_if(m_enum_cache.begin(), m_enum_cache.end(),
                                  [&](const auto &e) { return e.name() == type; });
        if (enum_ == m_enum_cache.end()) {
            return {};
        }

        std::vector<MetaEnum> enums;

        size_t asize = 1;
        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            auto value = std::get<MetaMember::ReferenceInfo>(array_size).m_ref->get<u32>();
            asize      = value ? value.value() : 0;
        } else {
            asize = std::get<u32>(array_size);
        }

        enums.reserve(asize);
        for (size_t i = 0; i < asize; ++i) {
            enums.emplace_back(*enum_);
        }

        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            return MetaMember(name, enums, std::get<MetaMember::ReferenceInfo>(array_size),
                              make_referable<MetaEnum>(*enum_));
        }

        return MetaMember(name, enums, make_referable<MetaEnum>(*enum_));
    }

    std::optional<MetaMember> Template::loadMemberStruct(std::string_view name,
                                                         std::string_view type,
                                                         MetaMember::size_type array_size) {
        auto struct_ = std::find_if(m_struct_cache.begin(), m_struct_cache.end(),
                                    [&](const auto &e) { return e.name() == type; });
        if (struct_ == m_struct_cache.end()) {
            return {};
        }
        std::vector<MetaStruct> structs;

        size_t asize = 1;
        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            auto value = std::get<MetaMember::ReferenceInfo>(array_size).m_ref->get<u32>();
            asize      = value ? value.value() : 0;
        } else {
            asize = std::get<u32>(array_size);
        }

        structs.reserve(asize);
        for (size_t i = 0; i < asize; ++i) {
            structs.emplace_back(*struct_);
        }

        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            return MetaMember(name, structs, std::get<MetaMember::ReferenceInfo>(array_size),
                              make_referable<MetaStruct>(*struct_));
        }

        return MetaMember(name, structs, make_referable<MetaStruct>(*struct_));
    }

    static void
    setMinMaxForValue(MetaValue &value,
                      const std::variant<std::monostate, s64, u64, float, double> &var_min,
                      const std::variant<std::monostate, s64, u64, float, double> &var_max) {
        switch (value.type()) {
        case MetaType::S8:
            if (std::holds_alternative<s64>(var_min)) {
                value.setMin(static_cast<s8>(std::get<s64>(var_min)));
            }
            if (std::holds_alternative<s64>(var_max)) {
                value.setMax(static_cast<s8>(std::get<s64>(var_max)));
            }
            break;
        case MetaType::U8:
            if (std::holds_alternative<u64>(var_min)) {
                value.setMin(static_cast<u8>(std::get<u64>(var_min)));
            }
            if (std::holds_alternative<u64>(var_max)) {
                value.setMax(static_cast<u8>(std::get<u64>(var_max)));
            }
            break;
        case MetaType::S16:
            if (std::holds_alternative<s64>(var_min)) {
                value.setMin(static_cast<s16>(std::get<s64>(var_min)));
            }
            if (std::holds_alternative<s64>(var_max)) {
                value.setMax(static_cast<s16>(std::get<s64>(var_max)));
            }
            break;
        case MetaType::U16:
            if (std::holds_alternative<u64>(var_min)) {
                value.setMin(static_cast<u16>(std::get<u64>(var_min)));
            }
            if (std::holds_alternative<u64>(var_max)) {
                value.setMax(static_cast<u16>(std::get<u64>(var_max)));
            }
            break;
        case MetaType::S32:
            if (std::holds_alternative<s64>(var_min)) {
                value.setMin(static_cast<s32>(std::get<s64>(var_min)));
            }
            if (std::holds_alternative<s64>(var_max)) {
                value.setMax(static_cast<s32>(std::get<s64>(var_max)));
            }
            break;
        case MetaType::U32:
            if (std::holds_alternative<u64>(var_min)) {
                value.setMin(static_cast<u32>(std::get<u64>(var_min)));
            }
            if (std::holds_alternative<u64>(var_max)) {
                value.setMax(static_cast<u32>(std::get<u64>(var_max)));
            }
            break;
        case MetaType::F32:
            if (std::holds_alternative<float>(var_min)) {
                value.setMin(std::get<float>(var_min));
            }
            if (std::holds_alternative<float>(var_max)) {
                value.setMax(std::get<float>(var_max));
            }
            break;
        case MetaType::F64:
            if (std::holds_alternative<double>(var_min)) {
                value.setMin(std::get<double>(var_min));
            }
            if (std::holds_alternative<double>(var_max)) {
                value.setMax(std::get<double>(var_max));
            }
            break;
        default:
            break;
        }
    }

    std::optional<MetaMember> Template::loadMemberPrimitive(
        std::string_view name, std::string_view type, MetaMember::size_type array_size,
        const std::variant<std::monostate, s64, u64, float, double> &var_min,
        const std::variant<std::monostate, s64, u64, float, double> &var_max) {
        auto vtype = magic_enum::enum_cast<MetaType>(type);
        if (!vtype)
            return {};

        std::vector<MetaValue> values;
        RefPtr<MetaValue> default_val = make_referable<MetaValue>(vtype.value());
        setMinMaxForValue(*default_val, var_min, var_max);

        size_t asize = 1;
        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            auto value = std::get<MetaMember::ReferenceInfo>(array_size).m_ref->get<u32>();
            asize      = value ? value.value() : 0;
        } else {
            asize = std::get<u32>(array_size);
        }

        values.reserve(asize);

        for (size_t i = 0; i < asize; ++i) {
            values.emplace_back(vtype.value());
            setMinMaxForValue(values.back(), var_min, var_max);
        }

        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            return MetaMember(name, values, std::get<MetaMember::ReferenceInfo>(array_size),
                              default_val);
        }

        return MetaMember(name, values, default_val);
    }

    Result<TemplateDependencies, JSONError> Template::loadDependencies(const json_t &dependencies) {
        TemplateDependencies deps;

        if (dependencies.contains("Managers")) {
            const json_t &managers = dependencies["Managers"];
            if (!managers.is_object()) {
                return make_json_error<TemplateDependencies>(
                    "TEMPLATE", "'Managers' dependency is not an object.", 0);
            }

            for (const auto &item : managers.items()) {
                const json_t &item_json = item.value();
                if (!item_json.contains("Name")) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'Managers' dependency is missing field 'Name'.", 0);
                }

                const json_t &name_json = item_json["Name"];
                if (!name_json.is_string()) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'Managers' dependency has malformed 'Name' field.", 0);
                }

                if (!item_json.contains("Ancestry")) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'Managers' dependency is missing field 'Name'.", 0);
                }

                const json_t &ancestry_json = item_json["Ancestry"];
                if (!ancestry_json.is_array()) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'Managers' dependency has malformed 'Ancestry' field.", 0);
                }

                TemplateDependencies::ObjectInfo manager_info;
                manager_info.m_type = item.key();
                manager_info.m_name = name_json.get<std::string>();
                manager_info.m_ancestry =
                    QualifiedName(ancestry_json.get<std::vector<std::string>>());

                deps.m_managers.emplace_back(std::move(manager_info));
            }
        }

        if (dependencies.contains("AssetPaths")) {
            const json_t asset_paths = dependencies["AssetPaths"];
            if (!asset_paths.is_array()) {
                return make_json_error<TemplateDependencies>(
                    "TEMPLATE", "'AssetPaths' dependency is not an array.", 0);
            }

            for (const auto &item : asset_paths.items()) {
                const json_t &asset_path_json = item.value();
                if (!asset_path_json.is_string()) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'AssetPaths' dependency encountered a non-string.", 0);
                }
                deps.m_asset_paths.emplace_back(std::move(asset_path_json.get<std::string>()));
            }
        }

        if (dependencies.contains("TablesBin")) {
            const json_t table_objs = dependencies["TablesBin"];
            if (!table_objs.is_object()) {
                return make_json_error<TemplateDependencies>(
                    "TEMPLATE", "'TablesBin' dependency is not an object.", 0);
            }

            for (const auto &item : table_objs.items()) {
                const json_t &item_json = item.value();
                if (!item_json.contains("Name")) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'TablesBin' dependency is missing field 'Name'.", 0);
                }

                const json_t &name_json = item_json["Name"];
                if (!name_json.is_string()) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'TablesBin' dependency has malformed 'Name' field.", 0);
                }

                if (!item_json.contains("Ancestry")) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'TablesBin' dependency is missing field 'Name'.", 0);
                }

                const json_t &ancestry_json = item_json["Ancestry"];
                if (!ancestry_json.is_array()) {
                    return make_json_error<TemplateDependencies>(
                        "TEMPLATE", "'TablesBin' dependency has malformed 'Ancestry' field.", 0);
                }

                TemplateDependencies::ObjectInfo table_obj_info;
                table_obj_info.m_type = item.key();
                table_obj_info.m_name = name_json.get<std::string>();
                table_obj_info.m_ancestry =
                    QualifiedName(ancestry_json.get<std::vector<std::string>>());

                deps.m_table_objs.emplace_back(std::move(table_obj_info));
            }
        }

        return deps;
    }

    Result<void, JSONError> Template::loadMembers(const json_t &members,
                                                  std::vector<MetaMember> &out) {
        for (const auto &item : members.items()) {
            const std::string member_name = item.key();
            const json_t &member_info     = item.value();

            if (!member_info.contains("Type")) {
                return make_json_error<void>(
                    "TEMPLATE",
                    "Member definition is missing 'Type' field for member: " + member_name, 0);
            }

            const json_t &member_type_json = member_info["Type"];
            if (!member_type_json.is_string()) {
                return make_json_error<void>(
                    "TEMPLATE", "Member 'Type' field is not a string for member: " + member_name,
                    0);
            }

            if (!member_info.contains("ArraySize")) {
                return make_json_error<void>(
                    "TEMPLATE",
                    "Member definition is missing 'ArraySize' field for member: " + member_name, 0);
            }

            const json_t &member_array_size_json = member_info["ArraySize"];
            if (!member_array_size_json.is_number() && !member_array_size_json.is_string()) {
                return make_json_error<void>(
                    "TEMPLATE",
                    "Member 'ArraySize' field is not a number or string for member: " + member_name,
                    0);
            }

            const std::string member_type =
                ResolveMetaTypeAlias(member_type_json.get<std::string>());

            MetaMember::size_type member_size;

            const json_t &member_size_info = member_info["ArraySize"];
            if (member_size_info.is_number()) {
                member_size = member_size_info.get<u32>();
            } else {
                std::string member_size_str = member_size_info.get<std::string>();
                auto member_it = std::find_if(out.begin(), out.end(), [&](const auto &e) {
                    return e.name() == member_size_str;
                });
                if (member_it != out.end()) {
                    auto value = member_it->value<MetaValue>(0);
                    if (value) {
                        member_size = MetaMember::ReferenceInfo(value.value(), member_it->name());
                    }
                }
            }

            auto member_type_enum = magic_enum::enum_cast<MetaType>(member_type);
            if (!member_type_enum.has_value()) {
                // This is a struct or enum
                {
                    auto enum_it =
                        std::find_if(m_enum_cache.begin(), m_enum_cache.end(),
                                     [&](const auto &e) { return e.name() == member_type; });
                    if (enum_it != m_enum_cache.end()) {
                        std::optional<MetaMember> member =
                            loadMemberEnum(member_name, member_type, member_size);
                        if (member) {
                            out.emplace_back(std::move(member.value()));
                        }
                        continue;
                    }
                }

                {
                    auto struct_it =
                        std::find_if(m_struct_cache.begin(), m_struct_cache.end(),
                                     [&](const auto &e) { return e.name() == member_type; });
                    if (struct_it != m_struct_cache.end()) {
                        const char *mname = member_name.c_str();
                        auto member       = loadMemberStruct(member_name, member_type, member_size);
                        if (member) {
                            out.push_back(member.value());
                        }
                        continue;
                    }
                }
            } else {
                std::variant<std::monostate, s64, u64, float, double> var_min, var_max;

                if (member_info.contains("Min")) {
                    switch (member_type_enum.value()) {
                    case MetaType::S8:
                    case MetaType::S16:
                    case MetaType::S32:
                        var_min = member_info["Min"].get<s64>();
                        break;
                    case MetaType::U8:
                    case MetaType::U16:
                    case MetaType::U32:
                        var_min = member_info["Min"].get<u64>();
                        break;
                    case MetaType::F32:
                        var_min = member_info["Min"].get<float>();
                        break;
                    case MetaType::F64:
                        var_min = member_info["Min"].get<double>();
                        break;
                    default:
                        break;
                    }
                }

                if (member_info.contains("Max")) {
                    switch (member_type_enum.value()) {
                    case MetaType::S8:
                    case MetaType::S16:
                    case MetaType::S32:
                        var_max = member_info["Max"].get<s64>();
                        break;
                    case MetaType::U8:
                    case MetaType::U16:
                    case MetaType::U32:
                        var_max = member_info["Max"].get<u64>();
                        break;
                    case MetaType::F32:
                        var_max = member_info["Max"].get<float>();
                        break;
                    case MetaType::F64:
                        var_max = member_info["Max"].get<double>();
                        break;
                    default:
                        break;
                    }
                }

                std::optional<MetaMember> member =
                    loadMemberPrimitive(member_name, member_type, member_size, var_min, var_max);
                if (member) {
                    out.emplace_back(std::move(member.value()));
                }
            }
        }
        return {};
    }

    static Result<MetaMember, JSONError> loadWizardMember(const Template::json_t &member_json,
                                                          MetaMember default_member) {
        default_member.syncArray();

        const size_t array_size = default_member.arraysize();

        if (default_member.isTypeStruct()) {
            std::vector<MetaStruct> inst_structs;
            inst_structs.reserve(array_size);

            for (size_t i = 0; i < array_size; ++i) {
                RefPtr<MetaStruct> struct_ = default_member.value<MetaStruct>(i).value();
                const std::vector<MetaStruct::MemberT> &struct_members = struct_->members();

                std::vector<MetaMember> inst_struct_members;
                inst_struct_members.reserve(struct_members.size());

                for (MetaStruct::MemberT mbr : struct_members) {
                    if (!member_json.contains(mbr->name())) {
                        return make_json_error<MetaMember>(
                            "TEMPLATE", "Wizard does not contain struct member: " + mbr->name(), 0);
                    }
                    const Template::json_t &mbr_json            = member_json[mbr->name()];
                    Result<MetaMember, JSONError> member_result = loadWizardMember(mbr_json, *mbr);
                    if (!member_result) {
                        return std::unexpected(member_result.error());
                    }
                    inst_struct_members.emplace_back(std::move(member_result.value()));
                }
                inst_structs.emplace_back(struct_->name(), inst_struct_members);
            }

            MetaMember::size_type var_array_size = default_member.arraysize_();
            if (std::holds_alternative<MetaMember::ReferenceInfo>(var_array_size)) {
                return MetaMember(default_member.name(), inst_structs,
                                  std::get<MetaMember::ReferenceInfo>(var_array_size),
                                  default_member.defaultValue());
            }

            return MetaMember(default_member.name(), inst_structs, default_member.defaultValue());
        }

        if (default_member.isTypeEnum()) {
            std::vector<MetaEnum> inst_enums;
            inst_enums.reserve(array_size);

            for (size_t i = 0; i < array_size; ++i) {
                MetaEnum value = MetaEnum(*default_member.value<MetaEnum>(i).value());
                Result<void, JSONError> result = value.loadJSON(member_json);
                if (!result) {
                    return std::unexpected(result.error());
                }
                inst_enums.emplace_back(std::move(value));
            }

            MetaMember::size_type var_array_size = default_member.arraysize_();
            if (std::holds_alternative<MetaMember::ReferenceInfo>(var_array_size)) {
                return MetaMember(default_member.name(), inst_enums,
                                  std::get<MetaMember::ReferenceInfo>(var_array_size),
                                  default_member.defaultValue());
            }

            return MetaMember(default_member.name(), inst_enums, default_member.defaultValue());
        }

        std::vector<MetaValue> inst_values;
        inst_values.reserve(array_size);

        for (size_t i = 0; i < array_size; ++i) {
            MetaValue value = MetaValue(*default_member.value<MetaValue>(i).value());
            value.loadJSON(member_json);
            inst_values.emplace_back(std::move(value));
        }

        MetaMember::size_type var_array_size = default_member.arraysize_();
        if (std::holds_alternative<MetaMember::ReferenceInfo>(var_array_size)) {
            return MetaMember(default_member.name(), inst_values,
                              std::get<MetaMember::ReferenceInfo>(var_array_size),
                              default_member.defaultValue());
        }

        return MetaMember(default_member.name(), inst_values, default_member.defaultValue());
    }

    Result<void, JSONError> Template::loadWizards(const json_t &wizards,
                                                  const json_t &render_infos) {
        if (m_wizards.size() == 0) {
            return {};
        }

        for (const auto &item : wizards.items()) {
            TemplateWizard &default_wizard = m_wizards[0];
            TemplateWizard wizard;

            wizard.m_name             = item.key();
            const json_t &wizard_json = item.value();

            if (wizard_json.contains("Name")) {
                wizard.m_obj_name = wizard_json["Name"];
            }

            if (wizard_json.contains("Dependencies")) {
                auto dep_result = loadDependencies(wizard_json["Dependencies"]);
                if (!dep_result) {
                    return Result<void, JSONError>(std::unexpected(dep_result.error()));
                }
                wizard.m_dependencies = dep_result.value();
            }

            if (!wizard_json.contains("Members")) {
                return make_json_error<void>("TEMPLATE", "Wizard json has no Members attribute!",
                                             0);
            }

            const json_t &members_json = wizard_json["Members"];
            if (!members_json.is_object()) {
                return make_json_error<void>("TEMPLATE",
                                             "Wizard json has malformed Members attribute!", 0);
            }

            for (auto &member_item : members_json.items()) {
                const std::string &member_name = member_item.key();
                const json_t &member_info      = member_item.value();

                auto member_it = std::find_if(
                    default_wizard.m_init_members.begin(), default_wizard.m_init_members.end(),
                    [&](const auto &e) { return e.name() == member_name; });

                if (member_it != default_wizard.m_init_members.end()) {
                    Result<MetaMember, JSONError> member_result =
                        loadWizardMember(member_info, *member_it);
                    if (!member_result) {
                        return std::unexpected(member_result.error());
                    }
                    wizard.m_init_members.emplace_back(std::move(member_result.value()));
                }
            }

            if (render_infos.contains(wizard.m_name)) {
                auto &render_info = render_infos[wizard.m_name];
                if (render_info.contains("Model")) {
                    wizard.m_render_info.m_file_model = render_info["Model"];
                }
                if (render_info.contains("Materials")) {
                    wizard.m_render_info.m_file_materials = render_info["Materials"];
                }
                if (render_info.contains("Animations")) {
                    wizard.m_render_info.m_file_animations = render_info["Animations"];
                }
                if (render_info.contains("Textures")) {
                    wizard.m_render_info.m_texture_swap_map = render_info["Textures"];
                }
            } else if (render_infos.contains("Default")) {
                auto &render_info = render_infos["Default"];
                if (render_info.contains("Model")) {
                    wizard.m_render_info.m_file_model = render_info["Model"];
                }
                if (render_info.contains("Materials")) {
                    wizard.m_render_info.m_file_materials = render_info["Materials"];
                }
                if (render_info.contains("Animations")) {
                    wizard.m_render_info.m_file_animations = render_info["Animations"];
                }
                if (render_info.contains("Textures")) {
                    wizard.m_render_info.m_texture_swap_map = render_info["Textures"];
                }
            }

            m_wizards.emplace_back(std::move(wizard));
        }

        return {};
    }

    static std::mutex s_templates_mutex;
    static fs_path s_cache_path = "./Templates/.cache/";

    std::unordered_map<std::string, Template> g_template_cache_base;
    std::unordered_map<std::string, Template> g_template_cache_custom;
    std::unordered_map<std::string, TemplateRenderInfo> g_object_render_infos;

    void Template::threadLoadTemplate(const std::string &type, bool is_custom) {
        Template template_;
        try {
            template_ = Template(type, is_custom);
        } catch (std::runtime_error &e) {
            TOOLBOX_ERROR(e.what());
            return;
        }
        s_templates_mutex.lock();
        if (is_custom) {
            g_template_cache_custom[type] = template_;
        } else {
            g_template_cache_base[type] = template_;
        }
        s_templates_mutex.unlock();
        return;
    }

    void Template::threadLoadTemplateBlob(const std::string &type, const json_t &the_json,
                                          bool is_custom) {
        Template template_;
        try {
            template_.m_type = type;
            template_.loadFromJSON(the_json);
        } catch (std::runtime_error &e) {
            TOOLBOX_ERROR(e.what());
            return;
        }
        s_templates_mutex.lock();
        if (is_custom) {
            g_template_cache_custom[type] = template_;
        } else {
            g_template_cache_base[type] = template_;
        }
        s_templates_mutex.unlock();
        return;
    }

    Result<void, FSError> TemplateFactory::initialize(const fs_path &cache_path) {
        s_cache_path = cache_path;

        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return std::unexpected(cwd_result.error());
        }

        bool templates_preloaded = false;
        if (isCacheMode()) {
            templates_preloaded |= loadFromCacheBlob(false).has_value();
            templates_preloaded |= loadFromCacheBlob(true).has_value();
        }

        const fs_path cwd              = cwd_result.value();
        const fs_path load_base_path   = cwd / "Templates/Vanilla";
        const fs_path load_custom_path = cwd / "Templates/Custom";

        if (!templates_preloaded) {
            struct TemplateLoadInfo {
                std::string m_type;
                bool m_is_custom;
            };

            std::vector<TemplateLoadInfo> template_infos;
            template_infos.reserve(1024);

            for (auto &subpath : std::filesystem::directory_iterator{load_base_path}) {
                if (!std::filesystem::is_regular_file(subpath)) {
                    continue;
                }

                auto type_str = subpath.path().stem().string();
                template_infos.emplace_back(type_str, false);
            }

            for (auto &subpath : std::filesystem::directory_iterator{load_custom_path}) {
                if (!std::filesystem::is_regular_file(subpath)) {
                    continue;
                }

                auto type_str = subpath.path().stem().string();
                template_infos.emplace_back(type_str, true);
            }

            std::for_each(std::execution::par, template_infos.begin(), template_infos.end(),
                          [](const TemplateLoadInfo &info) {
                              Template::threadLoadTemplate(info.m_type, info.m_is_custom);
                          });

            if (isCacheMode()) {
                auto res = saveToCacheBlob(false);  // Base templates
                if (!res) {
                    return std::unexpected(res.error());
                }

                res = saveToCacheBlob(true);  // Custom templates
                if (!res) {
                    return std::unexpected(res.error());
                }
            }
        }

        const fs_path obj_render_infos_path = cwd / "Templates/object_info_map.json";
        auto exists                         = Filesystem::is_regular_file(obj_render_infos_path);
        if (!exists) {
            return std::unexpected(exists.error());
        }

        if (!exists.value()) {
            return make_fs_error<void>(std::error_code(),
                                       {"Object render info map doesn't exist!"});
        }

        std::ifstream info_in = std::ifstream(obj_render_infos_path, std::ios::in);
        Template::json_t render_info_json;
        info_in >> render_info_json;

        auto result =
            tryJSON(render_info_json, [&](const Template::json_t &j) -> Result<void, JSONError> {
                for (const auto &[key, val] : j.items()) {
                    TemplateRenderInfo info;
                    if (val.contains("Textures")) {
                        const Template::json_t &textures_json = val.at("Textures");
                        if (!textures_json.is_object()) {
                            return make_json_error<void>(
                                "TemplateFactory",
                                std::format("Textures field of object kind {} is not a dictionary",
                                            key),
                                0);
                        }
                        info.m_texture_swap_map = textures_json;
                    }
                    if (val.contains("Animations")) {
                        const Template::json_t &animations_json = val.at("Animations");
                        if (!animations_json.is_array()) {
                            return make_json_error<void>(
                                "TemplateFactory",
                                std::format("Animations field of object kind {} is not a list",
                                            key),
                                0);
                        }
                        info.m_file_animations = animations_json;
                    }
                    if (val.contains("Model")) {
                        const Template::json_t &model_json = val.at("Model");
                        if (!model_json.is_string()) {
                            return make_json_error<void>(
                                "TemplateFactory",
                                std::format("Model field of object kind {} is not a list", key), 0);
                        }
                        info.m_file_model = model_json;
                    }
                    g_object_render_infos[key] = std::move(info);
                }

                return {};
            });

        return {};
    }

    Result<void, FSError> TemplateFactory::loadFromCacheBlob(bool is_custom) {
        auto blob_path = s_cache_path / (is_custom ? "blob_custom.json" : "blob.json");

        auto path_result = Toolbox::Filesystem::is_regular_file(blob_path);
        if (!path_result) {
            return std::unexpected(path_result.error());
        }

        if (!path_result.value()) {
            return make_fs_error<void>(std::error_code(),
                                       {"[TEMPLATE_FACTORY] blob.json not found!"});
        }

        std::ifstream file(blob_path, std::ios::in);
        if (!file.is_open()) {
            return make_fs_error<void>(std::error_code(),
                                       {"[TEMPLATE_FACTORY] Failed to open cache blob!"});
        }

        Deserializer in(file.rdbuf());
        Template::json_t blob_json;
        in.stream() >> blob_json;

        std::vector<Template::json_t::iterator> json_iters;
        json_iters.reserve(blob_json.size());

        // 2. Populate the vector (Standard iterators are bidirectional)
        for (auto it = blob_json.begin(); it != blob_json.end(); ++it) {
            json_iters.emplace_back(it);
        }

        std::for_each(std::execution::par, json_iters.begin(), json_iters.end(),
                      [is_custom](const Template::json_t::iterator &info) {
                          Template::threadLoadTemplateBlob(info.key(), info.value(), is_custom);
                      });

        return {};
    }

    Result<void, FSError> TemplateFactory::saveToCacheBlob(bool is_custom) {
        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return std::unexpected(cwd_result.error());
        }

        Template::json_t blob_json;

        {
            const fs_path &cwd              = cwd_result.value();
            const fs_path load_from_path =
                cwd / (is_custom ? "Templates/Custom" : "Templates/Vanilla");

            std::vector<fs_path> save_infos;
            save_infos.reserve(1024);

            for (auto &subpath : std::filesystem::directory_iterator{load_from_path}) {
                if (!Filesystem::is_regular_file(subpath.path()).value_or(false)) {
                    continue;
                }

                save_infos.emplace_back(subpath.path());
            }

            std::vector<std::optional<Template::json_t>> parsed_jsons(save_infos.size());

            std::transform(
                std::execution::par_unseq, save_infos.begin(), save_infos.end(),
                parsed_jsons.begin(), [](const fs_path &path) -> std::optional<Template::json_t> {
                    std::ifstream file(path, std::ios::in);
                    if (!file.is_open()) {
                        TOOLBOX_ERROR_V("[TEMPLATE_FACTORY] Failed to open template json {}",
                                        path.filename().string());
                        return std::nullopt;
                    }

                    Deserializer in(file.rdbuf());
                    Template::json_t t_json;
                    in.stream() >> t_json;

                    // Return the parsed JSON. Move semantics handle this efficiently
                    // behind the scenes without heavy copying.
                    return t_json;
                });

            for (auto &parsed_opt : parsed_jsons) {
                if (parsed_opt.has_value()) {
                    blob_json.update(std::move(parsed_opt.value()));
                }
            }
        }

        const fs_path blob_path = s_cache_path / (is_custom ? "blob_custom.json" : "blob.json");
        
        if (!Filesystem::exists(blob_path.parent_path()).value_or(false)) {
            auto result = Filesystem::create_directories(blob_path.parent_path());
            if (!result) {
                return std::unexpected(result.error());
            }
            if (!result.value()) {
                TOOLBOX_ERROR("[TEMPLATE_FACTORY] Failed to create cache directory!");
            }
        }

        Filesystem::is_directory(blob_path.parent_path())
            .and_then([&](bool is_dir) {
                if (!is_dir) {
                    return Filesystem::create_directory(blob_path.parent_path());
                }
                return Result<bool, FSError>();
            })
            .and_then([&](bool result) {
                std::ofstream file(blob_path, std::ios::out);
                if (!file.is_open()) {
                    return make_fs_error<bool>(std::error_code(),
                                               {"[TEMPLATE_FACTORY] Failed to open cache blob!"});
                }

                Serializer out(file.rdbuf());
                out.stream() << blob_json;

                file.close();
                return Result<bool, FSError>();
            })
            .or_else([](const FSError &error) {
                Toolbox::UI::LogError(error);
                return Result<bool, FSError>();
            });

        return {};
    }

    static bool s_cache_mode = false;

    bool TemplateFactory::isCacheMode() { return s_cache_mode; }

    void TemplateFactory::setCacheMode(bool mode) { s_cache_mode = mode; }

    TemplateFactory::create_t TemplateFactory::create(std::string_view type, bool include_custom) {
        auto type_str = std::string(type);

        if (include_custom) {
            if (g_template_cache_custom.contains(type_str)) {
                return make_scoped<Template>(g_template_cache_custom[type_str]);
            }
        }

        if (g_template_cache_base.contains(type_str)) {
            return make_scoped<Template>(g_template_cache_base[type_str]);
        }

        Template template_;
        try {
            template_ = Template(type, include_custom);
        } catch (std::runtime_error &e) {
            return make_fs_error<ScopePtr<Template>>(std::error_code(), {e.what()});
        }

        TemplateFactory::create_ret_t template_ptr;
        try {
            template_ptr = make_scoped<Template>(template_);
        } catch (std::runtime_error &e) {
            return make_fs_error<ScopePtr<Template>>(std::error_code(), {e.what()});
        }

        g_template_cache_base[type_str] = template_;
        return make_scoped<Template>(template_);
    }

    std::vector<TemplateFactory::create_ret_t> TemplateFactory::createAll(bool include_custom) {
        std::vector<TemplateFactory::create_ret_t> ret;
        ret.reserve(include_custom ? g_template_cache_base.size() + g_template_cache_custom.size()
                                   : g_template_cache_base.size());

        for (auto &item : g_template_cache_base) {
            ret.push_back(make_scoped<Template>(item.second));
        }

        if (include_custom) {
            for (auto &item : g_template_cache_custom) {
                ret.emplace_back(make_scoped<Template>(item.second));
            }
        }
        return ret;
    }

    ScopePtr<TemplateRenderInfo> TemplateFactory::findRenderInfo(const std::string &obj_field) {
        if (g_object_render_infos.contains(obj_field)) {
            return make_scoped<TemplateRenderInfo>(g_object_render_infos[obj_field]);
        }
        return nullptr;
    }

}  // namespace Toolbox::Object
