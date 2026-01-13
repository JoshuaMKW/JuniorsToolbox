#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <gui/appmain/scene/camera.hpp>
#include <scene/raildata.hpp>
#include <unordered_map>
#include <vector>
#include <imgui.h>

using namespace Toolbox;

namespace Toolbox::UI {

    struct PathPoint {
        glm::vec3 m_position;
        glm::vec4 m_color;
        uint32_t m_point_size;
    };

    struct PathConnection {
        PathPoint m_point;
        std::vector<PathPoint> m_connections;
    };

    class PathRenderer {
        uint32_t m_program;
        uint32_t m_mvp_uniform;
        uint32_t m_mode_uniform;

        uint32_t m_vao, m_vbo;

        std::vector<PathConnection> m_path_connections;

    public:
        [[nodiscard]] bool initPathRenderer();
        void updateGeometry(const RailData &data,
                            std::unordered_map<UUID64, bool> visible_map);
        void drawPaths(Camera *camera);

        PathRenderer();
        ~PathRenderer();
    };
}  // namespace Toolbox::UI