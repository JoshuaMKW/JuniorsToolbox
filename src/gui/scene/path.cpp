#include <filesystem>
#include <glad/glad.h>
#include <gui/scene/path.hpp>
#include <gui/scene/rendercommon.hpp>

namespace Toolbox::UI {

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
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
    }

    void PathRenderer::updateGeometry() {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        std::vector<PathPoint> buffer;
        for (auto &line : m_paths) {
            for (auto &point : line) {
                buffer.push_back(point);
            }
        }

        glBufferData(GL_ARRAY_BUFFER, sizeof(PathPoint) * buffer.size(), &buffer[0],
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void PathRenderer::drawPaths(Camera *camera) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_PROGRAM_POINT_SIZE);

        glLineWidth(2.0f);

        glm::mat4 mvp = camera->getProjMatrix() * camera->getViewMatrix() * glm::mat4(1.0);

        glUseProgram(m_program);

        glBindVertexArray(m_vao);

        glUniformMatrix4fv(m_mvp_uniform, 1, 0, &mvp[0][0]);

        int start = 0;
        glUniform1i(m_mode_uniform, GL_TRUE);
        for (auto &line : m_paths) {
            glDrawArrays(GL_POINTS, start, line.size());
            start += line.size();
        }

        start = 0;
        glUniform1i(m_mode_uniform, GL_FALSE);
        for (auto &line : m_paths) {
            glDrawArrays(GL_LINES, start, line.size());
            start += line.size();
        }

        glBindVertexArray(0);
    }
}  // namespace Toolbox::UI