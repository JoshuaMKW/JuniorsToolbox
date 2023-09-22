#pragma once

#include <glm/glm.hpp>
#include <format>

namespace Toolbox::Object {
    struct Transform {
        glm::vec3 m_translation;
        glm::vec3 m_rotation;
        glm::vec3 m_scale;
        bool operator==(const Transform &other) const = default;
    };
}  // namespace Toolbox::Object

template <> struct std::formatter<Toolbox::Object::Transform> : std::formatter<string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Object::Transform &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(T: {}, R: {}, S: {})", obj.m_translation,
                       obj.m_rotation, obj.m_scale);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};