#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <gui/appmain/scene/camera.hpp>
#include <imgui.h>
#include <scene/raildata.hpp>
#include <unordered_map>
#include <vector>

using namespace Toolbox;

namespace Toolbox::UI {

    struct PathPoint {
        glm::vec3 m_position;
        glm::vec4 m_color;
    };

    struct PathConnection {
        PathPoint m_point;
        std::vector<PathPoint> m_connections;
    };

    class PathRenderer {
    public:
        PathRenderer();
        ~PathRenderer();

    public:
        [[nodiscard]] bool initPathRenderer();

        [[nodiscard]] float getPointSize() const { return m_point_size; }
        void setPointSize(float size) { m_point_size = size; }

        [[nodiscard]] float getLineThickness() const { return m_line_thickness; }
        void setLineThickness(float thickness) { m_line_thickness = thickness; }

        void setScreenResolution(float x, float y) { m_resolution = glm::vec2(x, y); }

        void updateGeometry(const RailData &data, std::unordered_map<UUID64, bool> visible_map);
        void drawPaths(Camera *camera);

    private:
        uint32_t m_program           = 0;
        uint32_t m_mvp_uniform       = 0;
        uint32_t m_resolution_id     = 0;
        uint32_t m_point_size_id     = 0;
        uint32_t m_line_color_id     = 0;
        uint32_t m_line_thickness_id = 0;
        uint32_t m_mode_uniform      = 0;

        uint32_t m_vao = 0, m_vbo = 0;

        glm::vec2 m_resolution;
        float m_point_size     = 25.0f;
        float m_line_thickness = 5.0f;
        std::vector<PathConnection> m_path_connections;
    };
}  // namespace Toolbox::UI