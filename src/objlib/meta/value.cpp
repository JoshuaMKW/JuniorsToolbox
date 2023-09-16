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
            return std::get<std::string>(m_value);
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