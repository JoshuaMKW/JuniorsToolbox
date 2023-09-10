#pragma once

namespace Toolbox::Object {
    struct Transform {
        /*glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;*/
        bool operator==(const Transform &other) const = default;
    };
}  // namespace Toolbox::Object