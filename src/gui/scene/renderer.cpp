#include <J3D/Material/J3DUniformBufferObject.hpp>
#include <J3D/Rendering/J3DRendering.hpp>
#include <glad/glad.h>
#include <iostream>
#include <GLFW/glfw3.h>

#include "gui/scene/renderer.hpp"
#include "gui/input.hpp"
#include "gui/util.hpp"
#include "gui/application.hpp"

namespace Toolbox::UI {
    namespace Render {
        void PacketSort(J3DRendering::SortFunctionArgs packets) {
            std::sort(packets.begin(), packets.end(),
                      [](const J3DRenderPacket &a, const J3DRenderPacket &b) -> bool {
                          if ((a.SortKey & 0x01000000) != (b.SortKey & 0x01000000)) {
                              return (a.SortKey & 0x01000000) > (b.SortKey & 0x01000000);
                          } else {
                              return a.Material->Name < b.Material->Name;
                          }
                      });
        }

        bool CompileShader(const char *vertex_shader_src, const char *geometry_shader_src,
                           const char *fragment_shader_src, uint32_t &program_id_out) {

            char glErrorLogBuffer[4096];
            uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
            // uint32_t gs = glCreateShader(GL_GEOMETRY_SHADER);  // TODO: add this
            uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);

            glShaderSource(vs, 1, &vertex_shader_src, nullptr);
            glShaderSource(fs, 1, &fragment_shader_src, nullptr);

            glCompileShader(vs);

            int32_t status;
            glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
            if (status == 0) {
                GLint infoLogLength;
                glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &infoLogLength);

                glGetShaderInfoLog(vs, infoLogLength, nullptr, glErrorLogBuffer);

                std::cout << "Compile failure in vertex shader:" << glErrorLogBuffer << std::endl;

                return false;
            }

            glCompileShader(fs);

            glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
            if (status == 0) {
                GLint infoLogLength;
                glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &infoLogLength);

                glGetShaderInfoLog(fs, infoLogLength, nullptr, glErrorLogBuffer);

                std::cout << "Compile failure in fragment shader: " << glErrorLogBuffer
                          << std::endl;

                return false;
            }

            program_id_out = glCreateProgram();

            glAttachShader(program_id_out, vs);
            glAttachShader(program_id_out, fs);

            glLinkProgram(program_id_out);

            glGetProgramiv(program_id_out, GL_LINK_STATUS, &status);
            if (status == 0) {
                GLint logLen;
                glGetProgramiv(program_id_out, GL_INFO_LOG_LENGTH, &logLen);
                glGetProgramInfoLog(program_id_out, logLen, nullptr, glErrorLogBuffer);

                std::cout << "Shader Program Linking Error: " << glErrorLogBuffer << std::endl;

                return false;
            }

            glDetachShader(program_id_out, vs);
            glDetachShader(program_id_out, fs);

            glDeleteShader(vs);
            glDeleteShader(fs);

            return true;
        }
    }  // namespace Render

    Renderer::Renderer() {
        m_camera.setPerspective(150, 16 / 9, 100, 600000);
        m_camera.setOrientAndPosition({0, 1, 0}, {0, 0, 1}, {0, 0, 0});
        m_camera.updateCamera();
        J3DRendering::SetSortFunction(Render::PacketSort);

        // Create framebuffer for this window
        glGenFramebuffers(1, &m_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);

        if (!m_path_renderer.initPathRenderer()) {
            // show some error
        }

        // 128x128 billboards, 10 unique images
        if (!m_billboard_renderer.initBillboardRenderer(128, 10)) {
            // show some error
        }

        m_billboard_renderer.loadBillboardTexture("res/question.png", 0);
    }

    void Renderer::render(std::vector<std::shared_ptr<J3DModelInstance>> renderables,
                          f32 delta_time) {
        ImVec2 window_pos    = ImGui::GetWindowPos();
        m_window_rect = {
            window_pos,
            {window_pos.x + ImGui::GetWindowWidth(), window_pos.y + ImGui::GetWindowHeight()}
        };

        m_is_window_hovered = ImGui::IsWindowHovered();
        m_is_window_focused = ImGui::IsWindowFocused();

        if (m_is_window_hovered && Input::GetMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
            ImGui::SetWindowFocus();

        if (m_is_window_focused &&
            Input::GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {  // Mouse wrap
            ImVec2 mouse_pos   = ImGui::GetMousePos();
            ImVec2 window_pos  = ImGui::GetWindowPos();
            ImVec2 window_size = ImGui::GetWindowSize();

            bool wrapped = false;

            if (mouse_pos.x < m_window_rect.Min.x) {
                mouse_pos.x += window_size.x;
                wrapped = true;
            } else if (mouse_pos.x >= m_window_rect.Max.x) {
                mouse_pos.x -= window_size.x;
                wrapped = true;
            }

            if (mouse_pos.y < m_window_rect.Min.y) {
                mouse_pos.y += window_size.y;
                wrapped = true;
            } else if (mouse_pos.y >= m_window_rect.Max.y) {
                mouse_pos.y -= window_size.y;
                wrapped = true;
            }

            ImVec2 viewport_pos = MainApplication::instance().windowScreenPos();
            mouse_pos.x += viewport_pos.x;
            mouse_pos.y += viewport_pos.y;

            if (wrapped) {
                Input::SetMousePosition(mouse_pos, false);
            }
        }

        viewportBegin();
        if (m_is_dirty) {
            glm::mat4 projection, view;
            projection = m_camera.getProjMatrix();
            view       = m_camera.getViewMatrix();

            J3DUniformBufferObject::SetProjAndViewMatrices(projection, view);

            glm::vec3 position;
            m_camera.getPos(position);

            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            J3DRendering::Render(delta_time, position, view, projection, renderables);

            m_path_renderer.drawPaths(&m_camera);
            m_billboard_renderer.drawBillboards(&m_camera);

            m_is_dirty = false;
        }
        viewportEnd();
    }

    void Renderer::initializeData(const SceneInstance &scene) {
        initializePaths(scene.getRailData());
        initializeBillboards();
    }

    void Renderer::initializePaths(const RailData &rail_data) {
        m_path_renderer.m_paths.clear();
        for (auto &rail : rail_data) {
            for (auto &node : rail->nodes()) {
                std::vector<PathPoint> connections;

                PathPoint nodePoint(node->getPosition(), glm::vec4(1.0, 0.0, 1.0, 1.0), 64);

                for (auto connection : rail->getNodeConnections(node)) {
                    PathPoint connectionPoint(connection->getPosition(),
                                              glm::vec4(1.0, 0.0, 1.0, 1.0), 64);
                    connections.push_back(nodePoint);
                    connections.push_back(connectionPoint);
                };
                m_path_renderer.m_paths.push_back(connections);
            }
        }
        m_path_renderer.updateGeometry();
    }

    void Renderer::initializeBillboards() {
        m_billboard_renderer.m_billboards.push_back(Billboard(glm::vec3(0, 1000, 0), 128, 0));
    }

    void Renderer::viewportBegin() {
        ImGuiStyle &style = ImGui::GetStyle();

        m_window_size_prev = m_window_size;
        m_window_size      = ImGui::GetWindowSize();
        m_render_size      = ImVec2(m_window_size.x - style.WindowPadding.x * 2,
                                    m_window_size.y - style.WindowPadding.y * 2 -
                                        (style.FramePadding.y * 2.0f + ImGui::GetTextLineHeight()));

        // bind the framebuffer we want to render to
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);

        if (!m_is_dirty) {
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            return;
        }

        auto size_x = static_cast<GLsizei>(m_window_size.x);
        auto size_y = static_cast<GLsizei>(m_window_size.y);

        // window was resized, new texture and depth storage needed for framebuffer
        glDeleteRenderbuffers(1, &m_rbo_id);
        glDeleteTextures(1, &m_tex_id);

        glGenTextures(1, &m_tex_id);
        glBindTexture(GL_TEXTURE_2D, m_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_x, size_y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex_id, 0);

        glGenRenderbuffers(1, &m_rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size_x, size_y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  m_rbo_id);

        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glViewport(0, 0, size_x, size_y);
    }

    void Renderer::viewportEnd() {
        ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(m_tex_id)), m_render_size,
                     {0.0f, 1.0f}, {1.0f, 0.0f});

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool Renderer::inputUpdate() {
        if (m_is_window_hovered && m_is_window_focused) {
            if (Input::GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {
                ImVec2 mouse_delta = Input::GetMouseDelta();

                m_camera.turnLeftRight(-mouse_delta.x * 0.005f);
                m_camera.tiltUpDown(-mouse_delta.y * 0.005f);

                m_is_dirty = true;
            }

            float lr_delta = 0, ud_delta = 0;
            if (Input::GetKey(GLFW_KEY_A)) {
                lr_delta -= 1;
                m_is_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_D)) {
                lr_delta += 1;
                m_is_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_W)) {
                ud_delta += 1;
                m_is_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_S)) {
                ud_delta -= 1;
                m_is_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_LEFT_SHIFT)) {
                lr_delta *= 10;
                ud_delta *= 10;
            }

            lr_delta *= 10;
            ud_delta *= 10;

            m_camera.translateLeftRight(-lr_delta);
            m_camera.translateFwdBack(ud_delta);
        }

        if (m_window_size.x != m_window_size_prev.x || m_window_size.y != m_window_size_prev.y) {
            m_camera.setAspect(m_render_size.y > 0 ? m_render_size.x / m_render_size.y
                                                   : FLT_EPSILON * 2);
            m_is_dirty = true;
        }

        if (m_is_dirty)
            m_camera.updateCamera();

        return true;
    }

}  // namespace Toolbox::UI