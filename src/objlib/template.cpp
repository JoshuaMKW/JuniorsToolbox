#include "objlib/template.hpp"
#include "color.hpp"
#include "jsonlib.hpp"
#include "magic_enum.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/transform.hpp"
#include <expected>
#include <fstream>
#include <glm/glm.hpp>
#include <optional>

namespace Toolbox::Object {

    Template::Template(std::string_view type) : m_type(type) {
        auto p = std::filesystem::current_path();
        std::ifstream file("./Templates/" + std::string(type) + ".json", std::ios::in);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open template file: " + std::string(type));
        }

        Deserializer in(file.rdbuf());
        auto result = deserialize(in);
        if (!result) {
            throw std::runtime_error("Failed to deserialize template: " + std::string(type));
        }
    }

    std::expected<void, SerialError> Template::serialize(Serializer &out) const { return {}; }

    std::expected<void, SerialError> Template::deserialize(Deserializer &in) {
        json_t template_json;

        auto result = tryJSON(template_json, [&](json_t &j) {
            in.stream() >> j;

            for (auto &item : j.items()) {
                json_t &metadata    = item.value();
                json_t &member_data = metadata["Members"];
                json_t &struct_data = metadata["Structs"];
                json_t &enum_data   = metadata["Enums"];

                TemplateWizard default_wizard;

                cacheEnums(enum_data);
                cacheStructs(struct_data);
                loadMembers(member_data, default_wizard.m_init_members);

                m_wizards.push_back(default_wizard);

                loadWizards(metadata["Wizard"]);
                break;
            }
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message, err.m_reason, err.m_byte, in.filepath());
        }

        return {};
    }

    void Template::cacheEnums(json_t &enums) {
        for (const auto &item : enums.items()) {
            const auto &e  = item.value();
            auto enum_type = magic_enum::enum_cast<MetaType>(e["Type"].get<std::string>());
            if (!enum_type.has_value()) {
                continue;
            }
            bool enum_bitmask = e["Multi"].get<bool>();
            std::vector<MetaEnum::enum_type> values;
            for (auto &value : e["Flags"].items()) {
                auto vs                   = value.value().get<std::string>();
                MetaEnum::enum_type enumv = {value.key(), MetaValue(std::stoi(vs, nullptr, 0))};
                values.push_back(enumv);
            }
            MetaEnum menum(item.key(), enum_type.value(), values, enum_bitmask);
            m_enum_cache.emplace_back(menum);
        }
    }

    void Template::cacheStructs(json_t &structs) {
        std::vector<std::string> visited;
        for (const auto &item : structs.items()) {
            auto name         = item.key();
            auto json_members = item.value();

            std::vector<MetaMember> members;
            loadMembers(json_members, members);

            MetaStruct mstruct(item.key(), members);
            m_struct_cache.emplace_back(mstruct);
        }
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
                              std::make_shared<MetaEnum>(*enum_));
        }
        return MetaMember(name, enums, std::make_shared<MetaEnum>(*enum_));
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
                              std::make_shared<MetaStruct>(*struct_));
        }
        return MetaMember(name, structs, std::make_shared<MetaStruct>(*struct_));
    }

    std::optional<MetaMember> Template::loadMemberPrimitive(std::string_view name,
                                                            std::string_view type,
                                                            MetaMember::size_type array_size) {
        auto vtype = magic_enum::enum_cast<MetaType>(type);
        if (!vtype)
            return {};

        std::vector<MetaValue> values;

        size_t asize = 1;
        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            auto value = std::get<MetaMember::ReferenceInfo>(array_size).m_ref->get<u32>();
            asize      = value ? value.value() : 0;
        } else {
            asize = std::get<u32>(array_size);
        }

        values.reserve(asize);

        for (size_t i = 0; i < asize; ++i) {
            values.emplace_back(MetaValue(vtype.value()));
        }
        if (std::holds_alternative<MetaMember::ReferenceInfo>(array_size)) {
            return MetaMember(name, values, std::get<MetaMember::ReferenceInfo>(array_size),
                              std::make_shared<MetaValue>(vtype.value()));
        }
        return MetaMember(name, values, std::make_shared<MetaValue>(vtype.value()));
    }

    void Template::loadMembers(json_t &members, std::vector<MetaMember> &out) {
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

        for (const auto &item : members.items()) {
            auto member_name = item.key();
            auto member_info = item.value();

            auto member_type = member_info["Type"].get<std::string>();
            if (s_alias_map.contains(member_type)) {
                member_type = s_alias_map.at(member_type);
            }

            MetaMember::size_type member_size;

            auto member_size_info = member_info["ArraySize"];
            if (member_size_info.is_number()) {
                member_size = member_size_info.get<u32>();
            } else {
                auto member_size_str = member_size_info.get<std::string>();
                auto member_it       = std::find_if(out.begin(), out.end(), [&](const auto &e) {
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
                auto member = loadMemberPrimitive(member_name, member_type, member_size);
                if (member) {
                    out.push_back(member.value());
                }
            }
        }
    }

    static MetaMember loadWizardMember(Template::json_t &member_json, MetaMember default_member) {
        default_member.syncArray();

        if (default_member.isTypeStruct()) {
            std::vector<MetaStruct> inst_structs;
            for (size_t i = 0; i < default_member.arraysize(); ++i) {
                std::vector<MetaMember> inst_struct_members;
                auto struct_ = default_member.value<MetaStruct>(i).value();
                for (auto &mbr : struct_->members()) {
                    auto mbr_json = member_json[mbr->name()];
                    inst_struct_members.push_back(loadWizardMember(mbr_json, *mbr));
                }
                inst_structs.emplace_back(struct_->name(), inst_struct_members);
            }
            return MetaMember(default_member.name(), inst_structs, default_member.defaultValue());
        }

        // Todo: Actually use member_json  here :P

        if (default_member.isTypeEnum()) {
            std::vector<MetaEnum> inst_enums;
            for (size_t i = 0; i < default_member.arraysize(); ++i) {
                auto value = MetaEnum(*default_member.value<MetaEnum>(i).value());
                value.loadJSON(member_json);
                inst_enums.push_back(value);
            }
            return MetaMember(default_member.name(), inst_enums, default_member.defaultValue());
        }

        std::vector<MetaValue> inst_values;
        for (size_t i = 0; i < default_member.arraysize(); ++i) {
            auto value = MetaValue(*default_member.value<MetaValue>(i).value());
            value.loadJSON(member_json);
            inst_values.push_back(value);
        }
        return MetaMember(default_member.name(), inst_values, default_member.defaultValue());
    }

    void Template::loadWizards(json_t &wizards) {
        if (m_wizards.size() == 0) {
            return;
        }

        TemplateWizard &default_wizard = m_wizards[0];

        for (const auto &item : wizards.items()) {
            TemplateWizard wizard;
            wizard.m_name    = item.key();
            auto wizard_json = item.value();
            for (auto &member_item : wizard_json.items()) {
                auto member_name = member_item.key();
                auto member_info = member_item.value();

                auto member_it = std::find_if(
                    default_wizard.m_init_members.begin(), default_wizard.m_init_members.end(),
                    [&](const auto &e) { return e.name() == member_name; });
                if (member_it != default_wizard.m_init_members.end()) {
                    auto member = loadWizardMember(member_info, *member_it);
                    wizard.m_init_members.emplace_back(member);
                }
            }
            m_wizards.emplace_back(wizard);
        }
    }

    TemplateFactory::create_t TemplateFactory::create(std::string_view type) {
        static std::unordered_map<std::string, Template> s_template_cache;

        auto type_str = std::string(type);
        if (s_template_cache.contains(type_str)) {
            return std::make_unique<Template>(s_template_cache[type_str]);
        }

        Template template_;
        try {
            template_ = Template(type);
        } catch (std::runtime_error &e) {
            return make_fs_error<std::unique_ptr<Template>>(std::error_code(), e.what());
        }

        TemplateFactory::create_ret_t template_ptr;
        try {
            template_ptr = std::make_unique<Template>(template_);
        } catch (std::runtime_error &e) {
            return make_fs_error<std::unique_ptr<Template>>(std::error_code(), e.what());
        }

        s_template_cache[type_str] = template_;
        return std::make_unique<Template>(template_);
    }

}  // namespace Toolbox::Object