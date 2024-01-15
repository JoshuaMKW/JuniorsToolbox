#include <filesystem>
#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <gui/scene/path.hpp>
#include <gui/scene/renderer.hpp>

namespace Toolbox::UI {
    static float s_arrow_head_size = 100.0f;

    const char *path_vtx_shader_source = "#version 330\n\
        layout (location = 0) in vec3 position;\n\
        layout (location = 1) in vec4 color;\n\
        layout (location = 2) in int size;\n\
        out vec4 line_color;\n\
        uniform mat4 gpu_ModelViewProjectionMatrix;\n\
        void main()\n\
        {\n\
            gl_Position = gpu_ModelViewProjectionMatrix * vec4(position, 1.0);\n\
            gl_PointSize = min(size * 1000, size * 1000 / gl_Position.w);\n\
            line_color = color;\n\
        }";

    const char *path_frg_shader_source = "#version 330\n\
        uniform sampler2D spriteTexture;\n\
        in vec4 line_color;\n\
        uniform bool pointMode;\n\
        void main()\n\
        {\n\
            if(pointMode){\n\
                vec2 p = gl_PointCoord * 2.0 - vec2(1.0);\n\
                float r = sqrt(dot(p,p));\n\
                if(dot(p,p) > r || dot(p,p) < r*0.75){\n\
                    discard;\n\
                } else {\n\
                    gl_FragColor = vec4(1.0,1.0,1.0,1.0);\n\
                }\n\
            } else {\n\
                gl_FragColor = line_color;\n\
            }\n\
        }";

    bool PathRenderer::initPathRenderer() {

        if (!Toolbox::UI::Render::CompileShader(path_vtx_shader_source, nullptr,
                                                path_frg_shader_source, m_program)) {
            // show the user some error message
            return false;
        }

        m_mvp_uniform  = glGetUniformLocation(m_program, "gpu_ModelViewProjectionMatrix");
        m_mode_uniform = glGetUniformLocation(m_program, "pointMode");

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PathPoint),
                              (void *)offsetof(PathPoint, m_position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(PathPoint),
                              (void *)offsetof(PathPoint, m_color));
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(PathPoint),
                               (void *)offsetof(PathPoint, m_point_size));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    PathRenderer::PathRenderer() {}

    PathRenderer::~PathRenderer() {
        // This should check
        m_path_connections.clear();
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
    }

    void PathRenderer::updateGeometry(const RailData &data,
                                      std::unordered_map<std::string, bool> visible_map) {
        size_t rail_count = data.getRailCount();

        if (rail_count == 0)
            return;

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        // TODO: Render shortened lines with arrow tips for connections, fix color relations
        m_path_connections.clear();

        for (size_t i = 0; i < rail_count; ++i) {
            auto rail = data.getRail(i);
            if (visible_map.contains(rail->name()) && visible_map[rail->name()] == false) {
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
                    {node_color.m_r, node_color.m_g, node_color.m_b, 1.0f},
                    128
                };

                for (Rail::Rail::node_ptr_t connection : rail->getNodeConnections(node)) {
                    PathPoint connectionPoint(
                        connection->getPosition(),
                        {node_color.m_r, node_color.m_g, node_color.m_b, 1.0f}, 128);
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

        glBufferData(GL_ARRAY_BUFFER, sizeof(PathPoint) * buffer.size(), &buffer[0],
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

        glLineWidth(5.0f);

        glm::mat4 mvp = camera->getProjMatrix() * camera->getViewMatrix() * glm::mat4(1.0);

        glUseProgram(m_program);

        glBindVertexArray(m_vao);

        glUniformMatrix4fv(m_mvp_uniform, 1, 0, &mvp[0][0]);

        {
            GLint start = 0;

            glUniform1i(m_mode_uniform, GL_TRUE);
            for (auto &connection : m_path_connections) {
                // +1 for self
                glDrawArrays(GL_POINTS, start, 1);
                start += static_cast<GLint>(connection.m_connections.size() * 2);
            }
        }

        {
            GLint start = 0;

            glUniform1i(m_mode_uniform, GL_FALSE);
            for (auto &connection : m_path_connections) {
                for (size_t i = 0; i < connection.m_connections.size(); ++i) {
                    const PathPoint &self_point = connection.m_point;
                    const PathPoint &to_point   = connection.m_connections[i];

                    // Calculate the direction vector of the line segment
                    glm::vec3 dir     = glm::normalize(to_point.m_position - self_point.m_position);

                    // Shorten the line by some factor (e.g., arrowhead size)
                    glm::vec3 adjusted_end_pos = to_point.m_position - dir * s_arrow_head_size;

                    // Update the buffer with the new end point
                    PathPoint adjusted_end_point = {adjusted_end_pos, to_point.m_color,
                                                    to_point.m_point_size};
                    /*glBufferSubData(GL_ARRAY_BUFFER, start * sizeof(PathPoint) + (i + 1) * sizeof(PathPoint),
                                    sizeof(PathPoint), &adjusted_end_point);*/

                    // Draw the shortened line segment
                    glDrawArrays(GL_LINES, start, 2);
                    start += 2;

                    //// Draw arrowhead here (this could be a separate VAO/VBO setup for drawing a
                    //// cone or a simple triangle) For a simple triangle arrowhead:
                    //glm::quat leftRot =
                    //    glm::rotate(rotQuat, glm::radians(150.0f), glm::vec3(0, 0, 1));
                    //glm::quat rightRot =
                    //    glm::rotate(rotQuat, glm::radians(210.0f), glm::vec3(0, 0, 1));

                    //glm::vec3 left  = glm::rotate(leftRot, dir) * s_arrow_head_size;
                    //glm::vec3 right = glm::rotate(rightRot, dir) * s_arrow_head_size;

                    //PathPoint arrowhead[3] = {
                    //    PathPoint(newEndPoint, line[i + 1].m_color, line[i + 1].m_point_size),
                    //    PathPoint(newEndPoint + left, line[i + 1].m_color,
                    //              line[i + 1].m_point_size),
                    //    PathPoint(newEndPoint + right, line[i + 1].m_color,
                    //              line[i + 1].m_point_size)};

                    //glBufferSubData(GL_ARRAY_BUFFER, start * sizeof(PathPoint),
                    //                3 * sizeof(PathPoint), arrowhead);
                    //glDrawArrays(GL_TRIANGLES, start, 3);
                    //start += 3;
                }
            }
        }

        glBindVertexArray(0);
    }
}  // namespace Toolbox::UI