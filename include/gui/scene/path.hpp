#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <gui/scene/camera.hpp>
#include <vector>

namespace Toolbox::UI {

    typedef struct {
        glm::vec3 m_position;
        glm::vec4 m_color;
        uint32_t m_point_size;
    } PathPoint;

    class PathRenderer {
        uint32_t m_program;
        uint32_t m_mvp_uniform;
        uint32_t m_mode_uniform;

        uint32_t m_vao, m_vbo;

    public:
        // TODO: Update this so that this is private and provide add/delete path methods
        std::vector<std::vector<PathPoint>> m_paths;

        [[nodiscard]] bool initPathRenderer();
        void updateGeometry();
        void drawPaths(Camera *camera);

        PathRenderer();
        ~PathRenderer();
    };
}  // namespace Toolbox::UI