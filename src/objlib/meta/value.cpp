#include "objlib/meta/value.hpp"
#include "color.hpp"
#include "objlib/transform.hpp"
#include <format>
#include <glm/glm.hpp>
#include <string>

template <> struct std::formatter<glm::vec3> : std::formatter<string_view> {
    template <typename FormatContext> auto format(const glm::vec3 &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(x: {}, y: {}, z: {})", obj.x, obj.y, obj.z);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

template <> struct std::formatter<Toolbox::Object::Transform> : std::formatter<string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Object::Transform &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(T: {}, R: {}, S: {})", obj.m_translation,
                       obj.m_rotation, obj.m_scale);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

namespace Toolbox::Object {

    std::expected<void, JSONError> MetaValue::loadJSON(const nlohmann::json &json_value) {
        return tryJSON(json_value, [this](const json &j) {
            switch (m_type) {
            case MetaType::S8:
                m_value = j.get<s8>();
                break;
            case MetaType::U8:
                m_value = j.get<u8>();
                break;
            case MetaType::S16:
                m_value = j.get<s16>();
                break;
            case MetaType::U16:
                m_value = j.get<u16>();
                break;
            case MetaType::S32:
                m_value = j.get<s32>();
                break;
            case MetaType::U32:
                m_value = j.get<u32>();
                break;
            case MetaType::F32:
                m_value = j.get<f32>();
                break;
            case MetaType::F64:
                m_value = j.get<f64>();
                break;
            case MetaType::STRING:
                m_value = j.get<std::string>();
                break;
            case MetaType::VEC3: {
                glm::vec3 new_value;
                new_value.x = j[0].get<f32>();
                new_value.y = j[1].get<f32>();
                new_value.z = j[2].get<f32>();
                m_value = new_value;
                break;
            }
            case MetaType::TRANSFORM: {
                Transform new_value;
                new_value.m_translation.x = j[0][0].get<f32>();
                new_value.m_translation.y = j[0][1].get<f32>();
                new_value.m_translation.z = j[0][2].get<f32>();
                new_value.m_rotation.x    = j[1][0].get<f32>();
                new_value.m_rotation.y    = j[1][1].get<f32>();
                new_value.m_rotation.z    = j[1][2].get<f32>();
                new_value.m_scale.x       = j[2][0].get<f32>();
                new_value.m_scale.y       = j[2][1].get<f32>();
                new_value.m_scale.z       = j[2][2].get<f32>();
                m_value                       = new_value;
                break;
            }
            case MetaType::RGB: {
                Color::RGB24 new_value(j[0].get<u8>(), j[1].get<u8>(), j[2].get<u8>());
                m_value = new_value;
                break;
            }
            case MetaType::RGBA: {
                Color::RGBA32 new_value(j[0].get<u8>(), j[1].get<u8>(), j[2].get<u8>(), j[3].get<u8>());
                m_value = new_value;
                break;
            }
            }
        });
    }

    std::string MetaValue::toString() const {
        switch (m_type) {
        case MetaType::S8:
            return std::format("{}", std::get<s8>(m_value));
        case MetaType::U8:
            return std::format("{}", std::get<u8>(m_value));
        case MetaType::S16:
            return std::format("{}", std::get<s16>(m_value));
        case MetaType::U16:
            return std::format("{}", std::get<u16>(m_value));
        case MetaType::S32:
            return std::format("{}", std::get<s32>(m_value));
        case MetaType::U32:
            return std::format("{}", std::get<u32>(m_value));
        case MetaType::F32:
            return std::format("{}", std::get<f32>(m_value));
        case MetaType::F64:
            return std::format("{}", std::get<f64>(m_value));
        case MetaType::STRING:
            return std::format("\"{}\"", std::get<std::string>(m_value));
        case MetaType::VEC3:
            return std::format("{}", std::get<glm::vec3>(m_value));
        case MetaType::TRANSFORM:
            return std::format("{}", std::get<Transform>(m_value));
        case MetaType::RGB:
            return std::format("{}", std::get<Color::RGB24>(m_value));
        case MetaType::RGBA:
            return std::format("{}", std::get<Color::RGBA32>(m_value));
        case MetaType::COMMENT:
            return std::get<std::string>(m_value);
        default:
            return "null";
        }
    }

}  // namespace Toolbox::Object