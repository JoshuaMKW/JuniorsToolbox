#pragma once

#include "types.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Toolbox {

    enum class BoundingType {
        Box,
        Spheroid,
    };

    struct BoundingBox {
        glm::vec3 m_center, m_size;
        glm::quat m_rotation = {0.0f, 0.0f, 0.0f, 1.0f};

        BoundingBox() = default;
        BoundingBox(const glm::vec3 &center, const glm::vec3 &size) {
            this->m_center = center;
            this->m_size   = size;
        }
        BoundingBox(const glm::vec3 &center, const glm::vec3 &size, const glm::quat &rotation) {
            this->m_center   = center;
            this->m_size     = size;
            this->m_rotation = rotation;
        }

        glm::vec3 center() const { return m_center; }

        glm::vec3 sample(f32 lx, f32 ly, f32 lz, f32 scale = 1.0f,
                         BoundingType type = BoundingType::Box) const {
            glm::vec4 size = glm::vec4(m_size, 1.0f);

            glm::mat4x4 rot = glm::identity<glm::mat4x4>();
            rot *= glm::toMat4(m_rotation);

            // Calculate the point in the local space of the box.
            glm::vec4 local_point;
            local_point.x = (lx - 0.5f) * m_size.x;
            local_point.y = (ly - 0.5f) * m_size.y;
            local_point.z = (lz - 0.5f) * m_size.z;

            if (type == BoundingType::Box) {
                local_point *= scale;

                // Transform this point to the global space.
                glm::vec4 global_point = (local_point * rot);
                return {global_point.x + m_center.x, global_point.y + m_center.y,
                        global_point.z + m_center.z};
            } else {
                // Step 3: Calculate the magnitude of the normalized point and normalize the point
                // to ensure it lies within the unit sphere.
                float magnitude =
                    std::sqrtf(local_point.x * local_point.x + local_point.y * local_point.y +
                               local_point.z * local_point.z);

                local_point.x /= magnitude;
                local_point.y /= magnitude;
                local_point.z /= magnitude;

                // Step 4: Scale the point by the half-lengths of the box to get the corresponding
                // point in the spheroid space.
                glm::vec4 spheroid_point = local_point * (size * 0.5f * scale);

                // Transform this point to the global space.
                glm::vec4 global_point = (spheroid_point * rot);
                return {global_point.x + m_center.x, global_point.y + m_center.y,
                        global_point.z + m_center.z};
            }
        }

        bool contains(const glm::vec3 &point, f32 scale = 1.0f,
                      BoundingType type = BoundingType::Box) const {
            glm::mat4x4 rot = glm::identity<glm::mat4x4>();
            rot *= glm::toMat4(m_rotation);

            glm::mat4x4 inv_rot = glm::inverse(rot);

            glm::vec4 local_point = glm::vec4(point - m_center, 1.0f);
            glm::vec4 inv_point   = local_point * inv_rot;

            if (type == BoundingType::Box) {
                return (std::abs(inv_point.x) <= m_size.x * 0.5f * scale) &&
                       (std::abs(inv_point.y) <= m_size.y * 0.5f * scale) &&
                       (std::abs(inv_point.z) <= m_size.z * 0.5f * scale);
            } else {
                // Normalize the point's coordinates with respect to the half-lengths of the box.
                inv_point.x /= (m_size.x * 0.5f * scale);
                inv_point.y /= (m_size.y * 0.5f * scale);
                inv_point.z /= (m_size.z * 0.5f * scale);

                // The point is inside the spheroid if the sum of the squares of its normalized
                // coordinates is less than or equal to 1.
                return std::sqrtf((inv_point.x * inv_point.x + inv_point.y * inv_point.y +
                                   inv_point.z * inv_point.z)) <= 1.0f;
            }
        }
    };

}  // namespace Toolbox
