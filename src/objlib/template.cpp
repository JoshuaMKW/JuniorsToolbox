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

        fs_path f_path = "./Templates/" + std::string(type) + ".json";
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
                    return make_json_error<void>(
                        "TEMPLATE", "Template JSON is missing 'Members' object.",
                                                 0);
                }

                if (!struct_data.is_object()) {
                    return make_json_error<void>(
                        "TEMPLATE", "Template JSON is missing 'Structs' object.",
                                                 0);
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

            if (!e["Type"].is_string()) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum 'Type' field is not a string for enum: " + item.key(), 0);
            }

            if (!e.contains("Flags")) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum definition is missing 'Flags' field for enum: " + item.key(),
                    0);
            }

            if (!e["Flags"].is_object()) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum 'Flags' field is not an object for enum: " + item.key(), 0);
            }

            if (!e.contains("Multi")) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum definition is missing 'Multi' field for enum: " + item.key(),
                    0);
            }

            if (!e["Multi"].is_boolean()) {
                return make_json_error<void>(
                    "TEMPLATE", "Enum 'Multi' field is not a boolean for enum: " + item.key(), 0);
            }

            std::string member_type = e["Type"].get<std::string>();

            if (s_alias_map.contains(member_type)) {
                member_type = s_alias_map.at(member_type);
            }

            auto enum_type = magic_enum::enum_cast<MetaType>(member_type);
            if (!enum_type.has_value()) {
                continue;
            }

            bool enum_bitmask = e["Multi"].get<bool>();
            std::vector<MetaEnum::enum_type> values;
            for (auto &value : e["Flags"].items()) {
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
                values.push_back(enumv);
            }
            MetaEnum menum(item.key(), enum_type.value(), values, enum_bitmask);
            m_enum_cache.emplace_back(menum);
        }
        return {};
    }

    Result<void, JSONError> Template::cacheStructs(const json_t &structs) {
        std::vector<std::string> visited;
        for (const auto &item : structs.items()) {
            const std::string name     = item.key();
            const json_t &json_members = item.value();

            std::vector<MetaMember> members;
            Result<void, JSONError> result = loadMembers(json_members, members);
            if (!result) {
                return result;
            }

            MetaStruct mstruct(item.key(), members);
            m_struct_cache.emplace_back(mstruct);
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

    static void setMinMaxForValue(MetaValue &value,
                                  const std::variant<s64, u64, float, double> &var_min,
                                  const std::variant<s64, u64, float, double> &var_max) {
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

    std::optional<MetaMember>
    Template::loadMemberPrimitive(std::string_view name, std::string_view type,
                                  MetaMember::size_type array_size,
                                  const std::variant<s64, u64, float, double> &var_min,
                                  const std::variant<s64, u64, float, double> &var_max) {
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
            const json_t managers = dependencies["Managers"];
            if (!managers.is_object()) {
                return make_json_error<TemplateDependencies>(
                    "TEMPLATE", "'Managers' dependency is not an object.", 0);
            }

            for (const auto &item : managers.items()) {
                TemplateDependencies::ObjectInfo manager_info;
                manager_info.m_type = item.key();
                manager_info.m_name = item.value()["Name"].get<std::string>();
                manager_info.m_ancestry =
                    QualifiedName(item.value()["Ancestry"].get<std::vector<std::string>>());

                deps.m_managers.push_back(manager_info);
            }
        }

        if (dependencies.contains("AssetPaths")) {
            const json_t asset_paths = dependencies["AssetPaths"];
            if (!asset_paths.is_array()) {
                return make_json_error<TemplateDependencies>(
                    "TEMPLATE", "'AssetPaths' dependency is not an array.", 0);
            }

            for (const auto &item : asset_paths.items()) {
                deps.m_asset_paths.push_back(item.value().get<std::string>());
            }
        }

        if (dependencies.contains("TablesBin")) {
            const json_t table_objs = dependencies["TablesBin"];
            if (!table_objs.is_object()) {
                return make_json_error<TemplateDependencies>(
                    "TEMPLATE", "'TablesBin' dependency is not an object.", 0);
            }

            for (const auto &item : table_objs.items()) {
                TemplateDependencies::ObjectInfo table_obj_info;
                table_obj_info.m_type = item.key();
                table_obj_info.m_name = item.value()["Name"].get<std::string>();
                table_obj_info.m_ancestry =
                    QualifiedName(item.value()["Ancestry"].get<std::vector<std::string>>());

                deps.m_table_objs.push_back(table_obj_info);
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

            if (!member_info["Type"].is_string()) {
                return make_json_error<void>(
                    "TEMPLATE", "Member 'Type' field is not a string for member: " + member_name,
                    0);
            }

            if (!member_info.contains("ArraySize")) {
                return make_json_error<void>(
                    "TEMPLATE",
                    "Member definition is missing 'ArraySize' field for member: " + member_name, 0);
            }

            if (!member_info["ArraySize"].is_number() && !member_info["ArraySize"].is_string()) {
                return make_json_error<void>(
                    "TEMPLATE",
                    "Member 'ArraySize' field is not a number or string for member: " + member_name,
                    0);
            }

            std::string member_type = member_info["Type"].get<std::string>();
            if (s_alias_map.contains(member_type)) {
                member_type = s_alias_map.at(member_type);
            }

            // TODO: Fetch min/max if they exist in JSON and correctly translate to metavalues.

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
                        auto member = loadMemberEnum(member_name, member_type, member_size);
                        if (member) {
                            out.push_back(member.value());
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
                std::variant<s64, u64, float, double> var_min;
                std::variant<s64, u64, float, double> var_max;

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

                auto member =
                    loadMemberPrimitive(member_name, member_type, member_size, var_min, var_max);
                if (member) {
                    out.push_back(member.value());
                }
            }
        }
        return {};
    }

    static Result<MetaMember, JSONError> loadWizardMember(const Template::json_t &member_json,
                                                          MetaMember default_member) {
        default_member.syncArray();

        if (default_member.isTypeStruct()) {
            std::vector<MetaStruct> inst_structs;
            for (size_t i = 0; i < default_member.arraysize(); ++i) {
                std::vector<MetaMember> inst_struct_members;
                RefPtr<MetaStruct> struct_ = default_member.value<MetaStruct>(i).value();
                for (auto &mbr : struct_->members()) {
                    if (!member_json.contains(mbr->name())) {
                        return make_json_error<MetaMember>(
                            "TEMPLATE", "Wizard does not contain struct member: " + mbr->name(), 0);
                    }
                    const Template::json_t &mbr_json = member_json[mbr->name()];
                    Result<MetaMember, JSONError> member_result = loadWizardMember(mbr_json, *mbr);
                    if (!member_result) {
                        return std::unexpected(member_result.error());
                    }
                    inst_struct_members.emplace_back(member_result.value());
                }
                inst_structs.emplace_back(struct_->name(), inst_struct_members);
            }
            MetaMember::size_type array_size = default_member.arraysize_();
            if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
                return MetaMember(default_member.name(), inst_structs,
                                  std::get<MetaMember::ReferenceInfo>(array_size),
                                  default_member.defaultValue());
            }
            return MetaMember(default_member.name(), inst_structs, default_member.defaultValue());
        }

        if (default_member.isTypeEnum()) {
            std::vector<MetaEnum> inst_enums;
            for (size_t i = 0; i < default_member.arraysize(); ++i) {
                MetaEnum value = MetaEnum(*default_member.value<MetaEnum>(i).value());
                Result<void, JSONError> result = value.loadJSON(member_json);
                if (!result) {
                    return std::unexpected(result.error());
                }
                inst_enums.push_back(value);
            }
            MetaMember::size_type array_size = default_member.arraysize_();
            if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
                return MetaMember(default_member.name(), inst_enums,
                                  std::get<MetaMember::ReferenceInfo>(array_size),
                                  default_member.defaultValue());
            }
            return MetaMember(default_member.name(), inst_enums, default_member.defaultValue());
        }

        std::vector<MetaValue> inst_values;
        for (size_t i = 0; i < default_member.arraysize(); ++i) {
            auto value = MetaValue(*default_member.value<MetaValue>(i).value());
            value.loadJSON(member_json);
            inst_values.push_back(value);
        }
        MetaMember::size_type array_size = default_member.arraysize_();
        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            return MetaMember(default_member.name(), inst_values,
                              std::get<MetaMember::ReferenceInfo>(array_size),
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
                return make_json_error<void>("TEMPLATE", "Wizard json has malformed Members attribute!",
                                             0);
            }

            for (auto &member_item : members_json.items()) {
                const std::string &member_name = member_item.key();
                const json_t &member_info      = member_item.value();

                auto member_it = std::find_if(
                    default_wizard.m_init_members.begin(), default_wizard.m_init_members.end(),
                    [&](const auto &e) { return e.name() == member_name; });

                if (member_it != default_wizard.m_init_members.end()) {
                    Result<MetaMember, JSONError> member_result = loadWizardMember(member_info, *member_it);
                    if (!member_result) {
                        return std::unexpected(member_result.error());
                    }
                    wizard.m_init_members.emplace_back(member_result.value());
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

            m_wizards.push_back(wizard);
        }

        return {};
    }

    static std::mutex s_templates_mutex;
    std::unordered_map<std::string, Template> g_template_cache_base;
    std::unordered_map<std::string, Template> g_template_cache_custom;
    static fs_path s_cache_path = "./Templates/.cache/";

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

        if (!templates_preloaded) {
            std::vector<std::thread> threads;

            auto &cwd                = cwd_result.value();
            fs_path load_base_path   = cwd / "Templates";
            fs_path load_custom_path = cwd / "Templates/Custom";

            for (auto &subpath : std::filesystem::directory_iterator{load_base_path}) {
                if (!std::filesystem::is_regular_file(subpath)) {
                    continue;
                }

                auto type_str = subpath.path().stem().string();
                threads.emplace_back(std::thread(Template::threadLoadTemplate, type_str, false));
            }

            for (auto &subpath : std::filesystem::directory_iterator{load_custom_path}) {
                if (!std::filesystem::is_regular_file(subpath)) {
                    continue;
                }

                auto type_str = subpath.path().stem().string();
                threads.emplace_back(std::thread(Template::threadLoadTemplate, type_str, true));
            }

            for (auto &th : threads) {
                th.join();
            }

            if (isCacheMode()) {
                auto res = saveToCacheBlob(false);
                if (!res) {
                    return std::unexpected(res.error());
                }

                res = saveToCacheBlob(true);
                if (!res) {
                    return std::unexpected(res.error());
                }
            }
        }

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

        for (auto &item : blob_json.items()) {
            Template::threadLoadTemplateBlob(item.key(), item.value(), is_custom);
        }

        return {};
    }

    Result<void, FSError> TemplateFactory::saveToCacheBlob(bool is_custom) {
        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return std::unexpected(cwd_result.error());
        }

        Template::json_t blob_json;

        {
            std::vector<std::thread> threads;

            auto &cwd              = cwd_result.value();
            fs_path load_from_path = cwd / (is_custom ? "Templates/Custom" : "Templates");

            for (auto &subpath : std::filesystem::directory_iterator{load_from_path}) {
                auto type_str = subpath.path().stem().string();
                threads.push_back(std::thread(
                    [&](const std::string &type_str, const std::filesystem::path &path) {
                        if (std::filesystem::exists(path) &&
                            !std::filesystem::is_regular_file(path)) {
                            return;
                        }

                        std::ifstream file(path, std::ios::in);
                        if (!file.is_open()) {
                            TOOLBOX_ERROR_V("[TEMPLATE_FACTORY] Failed to open template json {}",
                                            path.filename().string());
                            return;
                        }

                        Deserializer in(file.rdbuf());
                        Template::json_t t_json;
                        in.stream() >> t_json;

                        s_templates_mutex.lock();
                        for (auto &[key, value] : t_json.items()) {
                            blob_json[key] = value;
                        }
                        s_templates_mutex.unlock();

                        file.close();
                    },
                    type_str, subpath.path()));
            }

            for (auto &th : threads) {
                th.join();
            }
        }

        auto blob_path = s_cache_path / (is_custom ? "blob_custom.json" : "blob.json");
        if (!std::filesystem::exists(blob_path.parent_path())) {
            auto result = Toolbox::Filesystem::create_directories(blob_path.parent_path());
            if (!result) {
                return std::unexpected(result.error());
            }
            if (!result.value()) {
                TOOLBOX_ERROR("[TEMPLATE_FACTORY] Failed to create cache directory!");
            }
        }
        Toolbox::Filesystem::is_directory(blob_path.parent_path())
            .and_then([&](bool is_dir) {
                if (!is_dir) {
                    return Toolbox::Filesystem::create_directory(blob_path.parent_path());
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

}  // namespace Toolbox::Object
