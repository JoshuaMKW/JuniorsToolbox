#include "objlib/template.hpp"
#include "jsonlib.hpp"
#include "magic_enum.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/transform.hpp"
#include <expected>
#include <optional>

using json = nlohmann::json;

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

    std::expected<void, SerialError> Template::serialize(Serializer &out) const {}

    std::expected<void, SerialError> Template::deserialize(Deserializer &in) {
        json template_json;

        auto result = tryJSON(template_json, [&](json &j) {
            in.stream() >> j;

            m_name = j.at(0).get<std::string>();

            json metadata = j[m_name];
            loadMembers(metadata["Members"], metadata["Structs"], metadata["Enums"]);
            loadWizards(metadata["Wizard"]);
        });

        if (!result) {
            JSONError &err = result.error();
            return std::unexpected(
                make_serial_error(err.m_message, err.m_reason, err.m_byte, in.filepath()));
        }

        return {};
    }

    void Template::cacheEnums(json &enums) {
        for (const auto &item : enums.items()) {
            const auto &e  = item.value();
            auto enum_type = magic_enum::enum_cast<MetaType>(e["type"].get<std::string>());
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
            m_enum_cache.emplace_back(e);
        }
    }

    void Template::cacheStructs(json &structs) {}

    std::optional<MetaMember> Template::loadMemberEnum(std::string_view name, size_t array_size) {
        return std::optional<MetaMember>();
    }

    std::optional<MetaStruct> Template::loadMemberStruct(std::string_view name, size_t array_size) {
        return std::optional<MetaStruct>();
    }

    std::optional<MetaMember>
    Template::loadMemberPrimitive(std::string_view name, std::string_view type, size_t array_size) {
        auto vtype = magic_enum::enum_cast<MetaType>(type);
        if (!vtype)
            return {};

        std::vector<MetaValue> values;
        values.reserve(array_size);
        for (size_t i = 0; i < array_size; ++i) {
        }
        return MetaMember(name, values);
    }

    void Template::loadMembers(json &members, json &structs, json &enums) {}

    void Template::loadWizards(json &wizards) {
        for (const auto &item : wizards.items()) {
            TemplateWizard wizard;
            wizard.m_name = item.key();
            for (const auto &mi : item.value()["members"].items()) {
                /* TODO: Properly initialize these according to members */
                wizard.m_init_members.emplace_back(MetaMember(mi.key(), MetaValue(0)));
            }
            m_wizards.emplace_back(wizard);
        }
    }

}  // namespace Toolbox::Object