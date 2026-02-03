#include <filesystem>
#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <gui/appmain/scene/path.hpp>
#include <gui/appmain/scene/renderer.hpp>

namespace Toolbox::UI {
    static float s_arrow_head_size = 100.0f;

    const char *s_path_vtx_shader_source = R"(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color;
out vec4 v_color_geo;

// Pass the view/projection matrices to the GS, or do it here.
// Doing it here is usually slightly faster if you don't need world-space logic in GS.
uniform mat4 u_mvp; // Model-View-Projection Matrix
uniform float u_point_size; // Size of each point

void main() {
    // Transform to Clip Space, but don't divide by w yet
    gl_Position = u_mvp * vec4(position, 1.0);
    gl_PointSize = min(u_point_size * 1000, u_point_size * 1000 / gl_Position.w);
    v_color_geo = color;
}
)";

    const char *s_path_geo_shader_source = R"(
#version 330 core

layout (lines) in;                              // Input type: Lines
layout (triangle_strip, max_vertices = 4) out;  // Output type: Strip

in vec4 v_color_geo[];

out vec4 f_color;

uniform float u_thickness;    // Desired thickness in pixels
uniform vec2  u_resolution; // Viewport dimensions (width, height) in pixels

void main() {
    // 1. Get input vertices in Clip Space
    vec4 p1 = gl_in[0].gl_Position;
    vec4 p2 = gl_in[1].gl_Position;

    // 2. Convert to NDC (Normalized Device Coordinates)
    // We must divide by w manually.
    // Note: We protect against w=0 division, though unlikely in valid scenes.
    vec2 p1_ndc = p1.xy / p1.w;
    vec2 p2_ndc = p2.xy / p2.w;

    // 3. Handle Aspect Ratio
    // In NDC, the screen is a square [-1, 1]. On a wide monitor, this stretches.
    // We convert to "Screen Space" relative units to calculate the correct normal.
    // Aspect Ratio = Width / Height
    float aspectRatio = u_resolution.x / u_resolution.y;
    
    // Scale X by aspect ratio to work in a square coordinate system
    vec2 p1_screen = p1_ndc;
    p1_screen.x *= aspectRatio;
    
    vec2 p2_screen = p2_ndc;
    p2_screen.x *= aspectRatio;

    // 4. Calculate Direction and Normal
    vec2 dir = normalize(p2_screen - p1_screen);
    vec2 normal = vec2(-dir.y, dir.x); // Perpendicular vector (-y, x)

    // 5. Calculate Offset size in NDC
    // Thickness is in pixels.
    // NDC size is 2.0 (from -1 to 1).
    // So: (Thickness / ViewportHeight) * 2.0
    // We divide by viewport height because Y is our non-stretched reference axis.
    vec2 offset = normal * (u_thickness / u_resolution.y);
    
    // Undo the aspect ratio adjustment for the X offset so it fits back into NDC
    offset.x /= aspectRatio;

    f_color = v_color_geo[0];

    // 6. Emit Vertices
    // We need to output in Clip Space (multiply by w back in)
    // Vertex 1: P1 + Offset
    gl_Position = vec4((p1_ndc + offset) * p1.w, p1.z, p1.w);
    EmitVertex();

    // Vertex 2: P1 - Offset
    gl_Position = vec4((p1_ndc - offset) * p1.w, p1.z, p1.w);
    EmitVertex();

    f_color = v_color_geo[1];

    // Vertex 3: P2 + Offset
    gl_Position = vec4((p2_ndc + offset) * p2.w, p2.z, p2.w);
    EmitVertex();

    // Vertex 4: P2 - Offset
    gl_Position = vec4((p2_ndc - offset) * p2.w, p2.z, p2.w);
    EmitVertex();

    EndPrimitive();
}
)";

    const char *s_path_frg_shader_source = R"(
#version 330 core

in vec4 f_color;

uniform sampler2D spriteTexture;
uniform bool u_point_mode;
void main()
{
    if(u_point_mode){
        vec2 p = gl_PointCoord * 2.0 - vec2(1.0);
        float r = sqrt(dot(p,p));
        if(dot(p,p) > r || dot(p,p) < r*0.75){
            discard;
        } else {
            gl_FragColor = f_color;
        }
    } else {
        gl_FragColor = f_color;
    }
})";

    bool PathRenderer::initPathRenderer() {

        if (!Render::CompileShader(s_path_vtx_shader_source, s_path_geo_shader_source, s_path_frg_shader_source,
                                   m_program)) {
            // show the user some error message
            return false;
        }

        m_mvp_uniform       = glGetUniformLocation(m_program, "u_mvp");
        m_resolution_id     = glGetUniformLocation(m_program, "u_resolution");
        m_point_size_id     = glGetUniformLocation(m_program, "u_point_size");
        m_line_color_id     = glGetUniformLocation(m_program, "u_line_color");
        m_line_thickness_id = glGetUniformLocation(m_program, "u_thickness");
        m_mode_uniform      = glGetUniformLocation(m_program, "u_point_mode");

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        // Configure a layout for PathPoint
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PathPoint),
                              (void *)offsetof(PathPoint, m_position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(PathPoint),
                              (void *)offsetof(PathPoint, m_color));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    PathRenderer::PathRenderer() : m_resolution() {}

    PathRenderer::~PathRenderer() {
        // This should check
        m_path_connections.clear();
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
    }

    void PathRenderer::updateGeometry(const RailData &data,
                                      std::unordered_map<UUID64, bool> visible_map) {
        size_t rail_count = data.getRailCount();

        if (rail_count == 0)
            return;

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        // TODO: Render shortened lines with arrow tips for connections, fix color relations
        m_path_connections.clear();
        m_path_connections.reserve(rail_count * 16);  // Estimate upper bound

        for (size_t i = 0; i < rail_count; ++i) {
            auto rail = data.getRail(i);
            if (visible_map.contains(rail->getUUID()) && visible_map[rail->getUUID()] == false) {
                continue;
            }

            float node_hue = (static_cast<float>(i) / (rail_count - 1)) * 360.0f;

            size_t node_count = rail->getNodeCount();
            for (size_t j = 0; j < node_count; ++j) {
                Rail::Rail::node_ptr_t node = rail->nodes()[j];

                float node_lerp       = static_cast<float>(j) / (node_count - 1);
                float node_saturation = std::lerp(0.5f, 1.0f, node_lerp);
                float node_brightness = std::lerp(1.0f, 0.5f, node_lerp);
                auto node_color =
                    Color::HSVToColor<Color::RGBShader>(node_hue, node_saturation, node_brightness);

                PathConnection p_connection{};
                p_connection.m_point = {
                    node->getPosition(),
                    {node_color.m_r, node_color.m_g, node_color.m_b, 1.0f}
                };

                for (Rail::Rail::node_ptr_t connection : rail->getNodeConnections(node)) {
                    PathPoint connectionPoint(
                        connection->getPosition(),
                        {node_color.m_r, node_color.m_g, node_color.m_b, 1.0f});
                    p_connection.m_connections.push_back(connectionPoint);
                };
                m_path_connections.push_back(p_connection);
            }
        }

        std::vector<PathPoint> buffer;
        for (auto &connection : m_path_connections) {
            // For the connecting points, we adjust back the end and add an arrowhead
            for (auto &point : connection.m_connections) {
                buffer.push_back(connection.m_point);
                buffer.push_back(point);
            }
        }

        m_vertex_count = buffer.size();

        glBufferData(GL_ARRAY_BUFFER, sizeof(PathPoint) * buffer.size(), buffer.data(),
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void PathRenderer::drawPaths(Camera *camera) {
        if (m_path_connections.size() == 0)
            return;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_PROGRAM_POINT_SIZE);

        glm::mat4 mvp = camera->getProjMatrix() * camera->getViewMatrix() * glm::mat4(1.0);

        glUseProgram(m_program);

        glBindVertexArray(m_vao);

        glUniform1f(m_point_size_id, m_point_size);
        glUniform1f(m_line_thickness_id, m_line_thickness);
        glUniform2f(m_resolution_id, m_resolution.x, m_resolution.y);
        glUniformMatrix4fv(m_mvp_uniform, 1, 0, &mvp[0][0]);

        {
            GLint start = 0;

            glUniform1i(m_mode_uniform, GL_FALSE);
            glDrawArrays(GL_LINES, 0, m_vertex_count);
        }

        {
            GLint start = 0;

            glUniform1i(m_mode_uniform, GL_TRUE);
            for (auto &connection : m_path_connections) {
                // +1 for self
                glDrawArrays(GL_POINTS, start, 1);
                start += static_cast<GLint>(connection.m_connections.size() * 2);
            }
        }

        glBindVertexArray(0);
    }
}  // namespace Toolbox::UI