// #include <GLFW/glfw3.h>
#include <J3D/Material/J3DUniformBufferObject.hpp>
#include <J3D/Rendering/J3DRendering.hpp>
#include <iostream>
#include <unordered_set>

#include "gui/application.hpp"
#include "gui/input.hpp"
#include "gui/logging/errors.hpp"
#include "gui/logging/logger.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/util.hpp"

static std::set<std::string> s_skybox_materials = {"_00_spline", "_01_nyudougumo", "_02_usugumo", "_03_sky"};

namespace Toolbox::UI {
    namespace Render {
        void PacketSort(J3DRendering::SortFunctionArgs packets) {
            std::sort(packets.begin(), packets.end(),
                      [](const J3DRenderPacket &a, const J3DRenderPacket &b) -> bool {
                          const bool is_sky_mat_a = s_skybox_materials.contains(a.Material->Name);
                          const bool is_sky_mat_b = s_skybox_materials.contains(b.Material->Name);
                          if (is_sky_mat_a || is_sky_mat_b) {
                              return is_sky_mat_a > is_sky_mat_b;
                          }
                          if ((a.SortKey & 0x01000000) != (b.SortKey & 0x01000000)) {
                              return (a.SortKey & 0x01000000) > (b.SortKey & 0x01000000);
                          } else {
                              return a.Material->Name < b.Material->Name;
                          }
                      });
        }

        bool CompileShader(const char *vertex_shader_src, const char *geometry_shader_src,
                           const char *fragment_shader_src, uint32_t &program_id_out) {

            char gl_error_log_buffer[4096];

            uint32_t vs = 0;
            uint32_t gs = 0;
            uint32_t fs = 0;

            uint32_t pid = glCreateProgram();

            if (vertex_shader_src) {
                vs = glCreateShader(GL_VERTEX_SHADER);

                glShaderSource(vs, 1, &vertex_shader_src, nullptr);

                glCompileShader(vs);

                int32_t status;
                glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
                if (status == 0) {
                    GLint gl_info_log_length;
                    glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &gl_info_log_length);

                    glGetShaderInfoLog(vs, gl_info_log_length, nullptr, gl_error_log_buffer);

                    BaseError error =
                        make_error<void>("Compile failure in vertex shader:", gl_error_log_buffer)
                            .error();
                    logError(error);

                    return false;
                }

                glAttachShader(pid, vs);
            }

            if (geometry_shader_src) {
                gs = glCreateShader(GL_GEOMETRY_SHADER);

                glShaderSource(gs, 1, &geometry_shader_src, nullptr);

                glCompileShader(gs);

                int32_t status;
                glGetShaderiv(gs, GL_COMPILE_STATUS, &status);
                if (status == 0) {
                    GLint gl_info_log_length;
                    glGetShaderiv(gs, GL_INFO_LOG_LENGTH, &gl_info_log_length);

                    glGetShaderInfoLog(gs, gl_info_log_length, nullptr, gl_error_log_buffer);

                    BaseError error =
                        make_error<void>("Compile failure in geometry shader:", gl_error_log_buffer)
                            .error();
                    logError(error);

                    return false;
                }

                glAttachShader(pid, gs);
            }

            if (fragment_shader_src) {
                fs = glCreateShader(GL_FRAGMENT_SHADER);

                glShaderSource(fs, 1, &fragment_shader_src, nullptr);

                glCompileShader(fs);

                int32_t status;
                glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
                if (status == 0) {
                    GLint gl_info_log_length;
                    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &gl_info_log_length);

                    glGetShaderInfoLog(fs, gl_info_log_length, nullptr, gl_error_log_buffer);

                    BaseError error =
                        make_error<void>("Compile failure in fragment shader:", gl_error_log_buffer)
                            .error();
                    logError(error);

                    return false;
                }

                glAttachShader(pid, fs);
            }

            glLinkProgram(pid);
            program_id_out = pid;

            int32_t status;
            glGetProgramiv(pid, GL_LINK_STATUS, &status);
            if (status == 0) {
                GLint logLen;
                glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &logLen);
                glGetProgramInfoLog(pid, logLen, nullptr, gl_error_log_buffer);

                BaseError error =
                    make_error<void>("Linking failure in shaders:", gl_error_log_buffer).error();
                logError(error);

                return false;
            }

            glDetachShader(pid, vs);
            glDetachShader(pid, gs);
            glDetachShader(pid, fs);

            glDeleteShader(vs);
            glDeleteShader(gs);
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

    Renderer::~Renderer() {
        glDeleteFramebuffers(1, &m_fbo_id);
        glDeleteRenderbuffers(1, &m_rbo_id);
        glDeleteTextures(1, &m_tex_id);
    }

    void Renderer::render(std::vector<ISceneObject::RenderInfo> renderables, f32 delta_time) {
        ImVec2 window_pos = ImGui::GetWindowPos();
        m_window_rect     = {
            window_pos,
            {window_pos.x + ImGui::GetWindowWidth(), window_pos.y + ImGui::GetWindowHeight()}
        };

        m_is_window_hovered = ImGui::IsWindowHovered();
        m_is_window_focused = ImGui::IsWindowFocused();

        if (m_is_window_hovered && Input::GetMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
            ImGui::SetWindowFocus();

        if (m_is_window_focused && Input::GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {  // Mouse wrap
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

            auto sky_it = std::find_if(
                renderables.begin(), renderables.end(),
                [](const ISceneObject::RenderInfo &info) { return info.m_obj_type == "Sky"; });
            if (sky_it != renderables.end()) {
                sky_it->m_obj_model->SetTranslation(position);
            }

            std::vector<std::shared_ptr<J3DModelInstance>> models = {};
            for (auto &renderable : renderables) {
                models.push_back(renderable.m_obj_model);
            }

            J3DRendering::Render(delta_time, position, view, projection, models);

            m_path_renderer.drawPaths(&m_camera);
            m_billboard_renderer.drawBillboards(&m_camera);

            m_is_dirty = false;
        }
        viewportEnd();
    }

    void Renderer::initializeData(const SceneInstance &scene) {
        initializePaths(scene.getRailData(), {});
        initializeBillboards();
    }

    void Renderer::initializePaths(const RailData &rail_data,
                                   std::unordered_map<std::string, bool> visible_map) {
        m_path_renderer.updateGeometry(rail_data, visible_map);
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