#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <gui/scene/camera.hpp>
#include <scene/raildata.hpp>
#include <vector>

using namespace Toolbox::Scene;

namespace Toolbox::UI {

    struct PathPoint {
        glm::vec3 m_position;
        glm::vec4 m_color;
        uint32_t m_point_size;
    };

    class PathRenderer {
        uint32_t m_program;
        uint32_t m_mvp_uniform;
        uint32_t m_mode_uniform;

        uint32_t m_vao, m_vbo;

        std::vector<std::vector<PathPoint>> m_path_connections;

    public:
        [[nodiscard]] bool initPathRenderer();
        void updateGeometry(const RailData &data);
        void drawPaths(Camera *camera);

        PathRenderer();
        ~PathRenderer();
    };
}  // namespace Toolbox::UI