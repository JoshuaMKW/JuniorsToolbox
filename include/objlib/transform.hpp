#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <format>

namespace Toolbox::Object {
    struct Transform {
        glm::vec3 m_translation;
        glm::vec3 m_rotation;
        glm::vec3 m_scale;

        bool operator==(const Transform &other) const = default;

        static Transform Identity() {
            return {
                {0.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f}
            };
        }

        static Transform FromMat4x4(const glm::mat4x4 &mtx) {
            glm::vec3 new_translation;
            glm::quat new_rotation_quat;
            glm::vec3 new_scale;
            glm::vec3 skew;
            glm::vec4 perspective;
            bool success = glm::decompose(mtx, new_scale, new_rotation_quat, new_translation, skew,
                                          perspective);
            glm::vec3 new_rotation_euler = glm::degrees(glm::eulerAngles(new_rotation_quat));
            return {new_translation, new_rotation_euler, new_scale};
        }

        explicit operator glm::mat4x4() const {
            glm::mat4x4 gizmo_transform =
                glm::translate(glm::identity<glm::mat4x4>(), m_translation);

            glm::vec3 euler_rot;
            euler_rot.x = glm::radians(m_rotation.x);
            euler_rot.y = glm::radians(m_rotation.y);
            euler_rot.z = glm::radians(m_rotation.z);

            glm::quat quat_rot = glm::angleAxis(euler_rot.z, glm::vec3(0.0f, 0.0f, 1.0f)) *
                                 glm::angleAxis(euler_rot.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
                                 glm::angleAxis(euler_rot.x, glm::vec3(1.0f, 0.0f, 0.0f));

            glm::mat4x4 obb_rot_mtx = glm::toMat4(quat_rot);
            gizmo_transform         = gizmo_transform * obb_rot_mtx;

            return glm::scale(gizmo_transform, m_scale);
        }

        Transform operator*(const glm::mat4x4 &mtx) const {
            glm::mat4x4 current_mtx = static_cast<glm::mat4x4>(*this);
            glm::mat4x4 final_mtx   = mtx * current_mtx;

            glm::vec3 new_translation;
            glm::quat new_rotation_quat;
            glm::vec3 new_scale;
            glm::vec3 skew;
            glm::vec4 perspective;

            bool success = glm::decompose(final_mtx, new_scale, new_rotation_quat, new_translation,
                                          skew, perspective);

            glm::vec3 new_rotation_euler = glm::degrees(glm::eulerAngles(new_rotation_quat));

            return {new_translation, new_rotation_euler, new_scale};
        }
    };
}  // namespace Toolbox::Object

inline Toolbox::Object::Transform operator*(const glm::mat4x4 &lhs,
                                     const Toolbox::Object::Transform &rhs) {
    glm::mat4x4 mat_rhs = glm::mat4x4(rhs);
    return Toolbox::Object::Transform::FromMat4x4(lhs * mat_rhs);
}

template <> struct std::formatter<Toolbox::Object::Transform> : std::formatter<string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Object::Transform &obj, FormatContext &ctx) const {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(T: {}, R: {}, S: {})", obj.m_translation,
                       obj.m_rotation, obj.m_scale);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};