#pragma once

#include <glm/glm.hpp>

namespace Toolbox::Object {
    struct Transform {
        glm::vec3 m_translation;
        glm::vec3 m_rotation;
        glm::vec3 m_scale;
        bool operator==(const Transform &other) const = default;
    };
}  // namespace Toolbox::Object