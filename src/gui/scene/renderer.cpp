// #include <GLFW/glfw3.h>

#include <J3D/Material/J3DUniformBufferObject.hpp>
#include <J3D/Rendering/J3DRendering.hpp>
#include <J3D/Picking/J3DPicking.hpp>

#include <iostream>
#include <unordered_set>

#include "core/input/input.hpp"
#include "core/log.hpp"

#include "gui/application.hpp"
#include "gui/logging/errors.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/settings.hpp"
#include "gui/util.hpp"

#include <glm/gtx/euler_angles.hpp>

#define RENDERER_RAIL_NODE_DIAMETER 200.0f

static std::set<std::string> s_skybox_materials = {"_00_spline", "_01_nyudougumo", "_02_usugumo",
                                                   "_03_sky"};

static const std::unordered_set<std::string> s_selection_blacklist = {
    "Map",
    "MapObjWave",
    "Shimmer",
    "Sky",
};

// Utility function to convert 2D screen space coordinates to a 3D ray in world space
static std::pair<glm::vec3, glm::vec3>
getRayFromMouse(const glm::vec2 &mousePos, Toolbox::Camera &camera, const glm::vec4 &viewport) {
    // Convert mouse position to normalized device coordinates
    glm::vec3 mouseNDC((mousePos.x / viewport.z) * 2.0f - 1.0f,
                       (mousePos.y / viewport.w) * 2.0f - 1.0f, 1.0f);
    mouseNDC.y = -mouseNDC.y;  // Invert Y coordinate

    // Convert to world space
    glm::vec4 rayClip(mouseNDC.x, mouseNDC.y, -1.0f, 1.0f);
    glm::vec4 rayEye   = glm::inverse(camera.getProjMatrix()) * rayClip;
    rayEye             = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
    glm::vec3 rayWorld = glm::vec3(glm::inverse(camera.getViewMatrix()) * rayEye);
    rayWorld           = glm::normalize(rayWorld);

    // Ray origin is the camera position
    glm::vec3 rayOrigin;
    camera.getPos(rayOrigin);

    return {rayOrigin, rayWorld};
}

static bool intersectRayAABB(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                             const glm::vec3 &min, const glm::vec3 &max,
                             float &intersectionDistance) {
    float tMin = std::numeric_limits<float>::lowest();
    float tMax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDirection[i]) < std::numeric_limits<float>::epsilon()) {
            // Ray is parallel to slab. No hit if origin not within slab
            if (rayOrigin[i] < min[i] || rayOrigin[i] > max[i])
                return false;
        } else {
            // Compute the intersection t value of the ray with the near and far slab planes
            float ood = 1.0f / rayDirection[i];
            float t1  = (min[i] - rayOrigin[i]) * ood;
            float t2  = (max[i] - rayOrigin[i]) * ood;

            // Make t1 be intersection with near plane, t2 with far plane
            if (t1 > t2)
                std::swap(t1, t2);

            // Compute the intersection of slab intersection intervals
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);

            // Exit with no collision as soon as slab intersection becomes empty
            if (tMin > tMax)
                return false;
        }
    }
    // Ray intersects all 3 slabs. If tMin is negative, the intersection point is behind the ray
    // origin.
    if (tMin < 0.0f)
        tMin = tMax;  // If tMin is negative, use tMax instead
    if (tMin < 0.0f)
        return false;  // Both tMin and tMax are negative, intersection is behind the ray's origin

    // Set the intersection distance before returning true
    intersectionDistance = tMin;
    return true;
}

static bool intersectRayOBB(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                            const glm::vec3 &obbMin, const glm::vec3 &obbMax,
                            const glm::mat4 &obbTransform, float &intersectionDistance) {
    // Inverse OBB transformation matrix to transform the ray into OBB's local space
    glm::mat4 invTransform = glm::inverse(obbTransform);

    // Transform the ray origin and direction into the OBB's local space
    glm::vec4 localRayOrigin    = invTransform * glm::vec4(rayOrigin, 1.0f);
    glm::vec4 localRayDirection = invTransform * glm::vec4(rayDirection, 0.0f);

    // Perform AABB intersection test in local space
    return intersectRayAABB(glm::vec3(localRayOrigin), glm::vec3(localRayDirection),
                            obbMin, obbMax, intersectionDistance);
}

static bool intersectRaySphere(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                               const glm::vec3 &sphereCenter, float sphereRadius,
                               float &intersectionDistance) {// Vector from Ray Origin to Sphere Center
    glm::vec3 L = sphereCenter - rayOrigin;

    // Project L onto the ray direction (tca = distance to the closest point on ray to center)
    float tca = glm::dot(L, rayDirection);

    // If tca is negative, the sphere is behind the ray origin
    // (unless we are inside, but let's assume outside for selection)
    if (tca < 0)
        return false;

    // d2 = squared distance from sphere center to the closest point on the ray
    // This calculation is much more stable than b*b - 4ac
    float d2 = glm::dot(L, L) - tca * tca;

    // If the squared perpendicular distance is greater than radius squared, we missed
    float radius2 = sphereRadius * sphereRadius;
    if (d2 > radius2)
        return false;

    // Calculate the distance from the closest point to the intersection surfaces
    float thc = sqrt(radius2 - d2);

    // t0 = first intersection (entry), t1 = second intersection (exit)
    float t0 = tca - thc;
    float t1 = tca + thc;

    // Handle being inside the sphere or very close
    if (t0 < 0) {
        t0 = t1;  // Use exit point if inside
        if (t0 < 0)
            return false;  // Both behind
    }

    intersectionDistance = t0;
    return true;
}

namespace Toolbox::UI {
    namespace Render {
        void PacketSort(J3D::Rendering::RenderPacketVector& packets) {
            std::sort(packets.begin(), packets.end(),
                      [](const J3DRenderPacket &a, const J3DRenderPacket &b) -> bool {
                          // Sort bias
                          {
                              u8 sort_bias_a = static_cast<u8>((a.SortKey & 0xFF000000) >> 24);
                              u8 sort_bias_b = static_cast<u8>((b.SortKey & 0xFF000000) >> 24);
                              if (sort_bias_a != sort_bias_b) {
                                  return sort_bias_a > sort_bias_b;
                              }
                          }

                          // Opaque or alpha test
                          {
                              u8 sort_alpha_a = static_cast<u8>((a.SortKey & 0x800000) >> 23);
                              u8 sort_alpha_b = static_cast<u8>((b.SortKey & 0x800000) >> 23);
                              if (sort_alpha_a != sort_alpha_b) {
                                  return sort_alpha_a > sort_alpha_b;
                              }
                          }

                          return a.Material->Name < b.Material->Name;
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
                    LogError(error);

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
                    LogError(error);

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
                    LogError(error);

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
                LogError(error);

                return false;
            }

            if (vertex_shader_src) {
                glDetachShader(pid, vs);
                glDeleteShader(vs);
            }
            if (geometry_shader_src) {
                glDeleteShader(gs);
                glDetachShader(pid, gs);
            }
            if (fragment_shader_src) {
                glDeleteShader(fs);
                glDetachShader(pid, fs);
            }

            return true;
        }
    }  // namespace Render

    Renderer::Renderer() {
        m_camera.setPerspective(glm::radians(70.0f), 16.0f / 9.0f, 10.0f,
                                100000.0f);
        m_camera.setOrientAndPosition({0, 1, 0}, {0, 0, 1}, {0, 0, 0});
        m_camera.updateCamera();

        J3D::Picking::InitFramebuffer(800, 600);
        J3D::Rendering::SetSortFunction(Render::PacketSort);

        // Create framebuffer for this window
        glGenFramebuffers(1, &m_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);

        if (!m_path_renderer.initPathRenderer()) {
            // show some error
        }

        // 128x128 billboards, 10 unique images
        if (!m_billboard_renderer.initBillboardRenderer(RENDERER_RAIL_NODE_DIAMETER, 10)) {
            // show some error
        }

        m_billboard_renderer.loadBillboardTexture("res/question.png", 0);

        m_gizmo_op   = ImGuizmo::OPERATION::TRANSLATE;
        m_gizmo_mode = ImGuizmo::WORLD;

        m_gizmo_matrix = glm::identity<glm::mat4x4>();
        m_gizmo_matrix_prev = glm::identity<glm::mat4x4>();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    Renderer::~Renderer() {
        glDeleteFramebuffers(1, &m_fbo_id);
        glDeleteRenderbuffers(1, &m_rbo_id);
        glDeleteTextures(1, &m_tex_id);

        J3D::Picking::DestroyFramebuffer();
    }

    void Renderer::render(std::vector<ISceneObject::RenderInfo> renderables, TimeStep delta_time) {
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 window_pos = ImGui::GetWindowPos();
        m_window_rect     = {
            window_pos,
            {window_pos.x + ImGui::GetWindowWidth(), window_pos.y + ImGui::GetWindowHeight()}
        };
        m_window_size_prev = m_window_size;
        m_window_size      = ImGui::GetWindowSize();
        m_render_rect      = {m_window_rect.Min + style.WindowPadding,
                              m_window_rect.Max - style.WindowPadding};

        ImVec2 relative_window_pos = ImGui::GetCursorPos();

        // For the window bar
        m_render_rect.Min.y += relative_window_pos.y - style.WindowPadding.y;
        m_render_size = ImVec2(m_render_rect.GetWidth(), m_render_rect.GetHeight());

        ImVec2 mouse_pos = ImGui::GetMousePos();

        m_is_window_hovered = ImGui::IsWindowHovered();
        m_is_window_focused = ImGui::IsWindowFocused();

        bool right_click      = Input::GetMouseButton(Input::MouseButton::BUTTON_RIGHT);
        bool right_click_down = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);

        if (m_render_rect.Contains(mouse_pos)) {
            if (right_click) {
                m_is_view_manipulating = true;
            }
            if (right_click_down) {
                m_is_window_focused = true;
                ImGui::SetWindowFocus();
            }
        }

        if (m_is_window_focused && m_is_view_manipulating &&
            Input::GetMouseButton(Input::MouseButton::BUTTON_RIGHT)) {  // Mouse wrap
            bool wrapped = false;

            if (mouse_pos.x < m_render_rect.Min.x) {
                mouse_pos.x += m_render_size.x;
                wrapped = true;
            } else if (mouse_pos.x >= m_render_rect.Max.x) {
                mouse_pos.x -= m_render_size.x;
                wrapped = true;
            }

            if (mouse_pos.y < m_render_rect.Min.y) {
                mouse_pos.y += m_render_size.y;
                wrapped = true;
            } else if (mouse_pos.y >= m_render_rect.Max.y) {
                mouse_pos.y -= m_render_size.y;
                wrapped = true;
            }

            if (wrapped) {
                Input::SetMousePosition(mouse_pos.x, mouse_pos.y, false);
            }
        } else {
            m_is_view_manipulating = false;
        }

        viewportBegin();
        if (m_is_view_dirty) {
            glm::mat4 projection, view;
            projection = m_camera.getProjMatrix();
            view       = m_camera.getViewMatrix();

            J3DUniformBufferObject::ClearUBO();
            J3DUniformBufferObject::SetProjAndViewMatrices(projection, view);

            glm::vec3 position;
            m_camera.getPos(position);

            // Sensible defaults to avoid state leaks
            {
                glEnable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glClearDepth(1.0f);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            auto sky_it = std::find_if(renderables.begin(), renderables.end(),
                                       [](const ISceneObject::RenderInfo &info) {
                                           return info.m_object->type() == "Sky";
                                       });
            if (sky_it != renderables.end()) {
                sky_it->m_model->SetTranslation(position);
            }

            std::vector<RefPtr<J3DModelInstance>> models = {};
            std::vector<RefPtr<J3DModelInstance>> pick_models = {};
            models.reserve(renderables.size());
            pick_models.reserve(renderables.size());

            for (auto &renderable : renderables) {
                models.emplace_back(renderable.m_model);
                if (!s_selection_blacklist.contains(renderable.m_object->type())) {
                    pick_models.emplace_back(renderable.m_model);
                }
            }

            J3D::Rendering::RenderPacketVector packets =
                J3D::Rendering::SortPackets(models, position);
            J3D::Rendering::Render(delta_time, view, projection, packets);

            m_path_renderer.drawPaths(&m_camera);
            m_billboard_renderer.drawBillboards(&m_camera);

            J3D::Rendering::RenderPacketVector pick_packets =
                J3D::Rendering::SortPackets(pick_models, position);
            J3D::Picking::RenderPickingScene(pick_packets);
            m_is_view_dirty = false;
        }
        viewportEnd();
    }

    void Renderer::initializeData(const SceneInstance &scene) {
        initializePaths(scene.getRailData(), {});
        initializeBillboards();
    }

    void Renderer::initializePaths(const RailData &rail_data,
                                   std::unordered_map<UUID64, bool> visible_map) {
        m_path_renderer.updateGeometry(rail_data, visible_map);
    }

    void Renderer::initializeBillboards() {
        // m_billboard_renderer.m_billboards.push_back(Billboard(glm::vec3(0, 1000, 0), 128, 0));
    }

    void Renderer::viewportBegin() {
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 cursor_pos     = ImGui::GetCursorScreenPos();
        ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(cursor_pos.x, cursor_pos.y, m_render_size.x, m_render_size.y);

        // bind the framebuffer we want to render to
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);

        if (!m_is_view_dirty) {
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            return;
        }

        J3D::Picking::ResizeFramebuffer(static_cast<GLsizei>(m_render_size.x / 4.0f),
                                        static_cast<GLsizei>(m_render_size.y / 4.0f));

        auto size_x = static_cast<GLsizei>(m_render_size.x);
        auto size_y = static_cast<GLsizei>(m_render_size.y);

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
        ImGui::Image((ImTextureID)m_tex_id, m_render_size,
                     {0.0f, 1.0f}, {1.0f, 0.0f});

        if (m_render_gizmo) {
            bool shift_held = Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT);
            bool ctrl_held  = Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL);

            float snap[3];
            if ((m_gizmo_op & (ImGuizmo::OPERATION::SCALE | ImGuizmo::OPERATION::SCALEU))) {
                snap[0] = 0.1f;
                snap[1] = 0.1f;
                snap[2] = 0.1f;
            } else if ((m_gizmo_op & ImGuizmo::OPERATION::ROTATE)) {
                snap[0] = 15.0f;
                snap[1] = 15.0f;
                snap[2] = 15.0f;
            } else if ((m_gizmo_op & ImGuizmo::OPERATION::TRANSLATE)) {
                snap[0] = 50.0f;
                snap[1] = 50.0f;
                snap[2] = 50.0f;
            }

            const bool was_updating = m_gizmo_active;

            m_gizmo_matrix_prev = m_gizmo_matrix;
            m_gizmo_updated = ImGuizmo::Manipulate(
                glm::value_ptr(m_camera.getViewMatrix()), glm::value_ptr(m_camera.getProjMatrix()),
                m_gizmo_op, m_gizmo_mode, glm::value_ptr(m_gizmo_matrix), nullptr,
                ctrl_held ? snap : nullptr);
            m_gizmo_active = ImGuizmo::IsUsing();

            if (m_gizmo_active && !was_updating) {
                m_gizmo_matrix_start = m_gizmo_matrix;
                TOOLBOX_DEBUG_LOG("Initializing start matrix!");
            }

            if (was_updating && !m_gizmo_active) {
                // Finished manipulating
                //m_gizmo_matrix_start = glm::identity<glm::mat4x4>();
                //TOOLBOX_DEBUG_LOG("Deinitializing start matrix!");
            }
        }

        glm::vec3 camera_pos;
        glm::vec3 camera_dir;

        m_camera.getPos(camera_pos);
        m_camera.getDir(camera_dir);

        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        float font_size       = ImGui::GetFontSize();
        ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        f32 text_box_height = font_size;

        ImGui::SetCursorPos(
            {window_padding.x, m_window_size.y - text_box_height * 2.0f - window_padding.y});
        {
            std::string camera_pos_str = std::format("Position {}", camera_pos);

            ImVec2 text_size = ImGui::CalcTextSize(camera_pos_str.c_str());
            ImVec2 text_pos  = ImGui::GetCursorScreenPos();

            ImVec4 text_bg_color = {0.0f, 0.0f, 0.0f, 0.75f};

            draw_list->AddRectFilled(text_pos, text_pos + text_size,
                                     ImGui::GetColorU32(text_bg_color));

            ImGui::Text(camera_pos_str.c_str());
        }

        ImGui::SetCursorPos(
            {window_padding.x, m_window_size.y - text_box_height - window_padding.y});
        {
            std::string camera_dir_str = std::format("View Direction {}", camera_dir);

            ImVec2 text_size = ImGui::CalcTextSize(camera_dir_str.c_str());
            ImVec2 text_pos  = ImGui::GetCursorScreenPos();

            ImVec4 text_bg_color = {0.0f, 0.0f, 0.0f, 0.75f};

            draw_list->AddRectFilled(text_pos, text_pos + text_size,
                                     ImGui::GetColorU32(text_bg_color));

            ImGui::Text(camera_dir_str.c_str());
        }

        {
            std::string fps_str = std::format("{:.2f} FPS", ImGui::GetIO().Framerate);

            ImVec2 text_size = ImGui::CalcTextSize(fps_str.c_str());

            ImGui::SetCursorPos({m_window_size.x - text_size.x - window_padding.x,
                                 m_window_size.y - text_box_height - window_padding.y});

            ImVec2 text_pos = ImGui::GetCursorScreenPos();

            ImVec4 text_bg_color = {0.0f, 0.0f, 0.0f, 0.75f};

            draw_list->AddRectFilled(text_pos, text_pos + text_size,
                                     ImGui::GetColorU32(text_bg_color));

            ImGui::Text(fps_str.c_str());
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::setGizmoTransform(const Transform &transform) {
        glm::mat4x4 gizmo_transform =
            glm::translate(glm::identity<glm::mat4x4>(), transform.m_translation);

        glm::vec3 euler_rot;
        euler_rot.x = glm::radians(transform.m_rotation.x);
        euler_rot.y = glm::radians(transform.m_rotation.y);
        euler_rot.z = glm::radians(transform.m_rotation.z);

        glm::quat quat_rot = glm::angleAxis(euler_rot.z, glm::vec3(0.0f, 0.0f, 1.0f)) *
                             glm::angleAxis(euler_rot.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
                             glm::angleAxis(euler_rot.x, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4x4 obb_rot_mtx = glm::toMat4(quat_rot);
        gizmo_transform         = gizmo_transform * obb_rot_mtx;

        gizmo_transform = glm::scale(gizmo_transform, transform.m_scale);
        setGizmoTransform(gizmo_transform);
    }

    bool Renderer::inputUpdate(TimeStep delta_time) {
        const AppSettings &settings =
            GUIApplication::instance().getSettingsManager().getCurrentProfile();

        if (m_is_view_manipulating && Input::GetMouseButton(Input::MouseButton::BUTTON_RIGHT)) {
            double delta_x, delta_y;
            Input::GetMouseDelta(delta_x, delta_y);

            m_camera.turnLeftRight(-delta_x * settings.m_camera_sensitivity * delta_time * 0.25f);
            m_camera.tiltUpDown(-delta_y * settings.m_camera_sensitivity * delta_time * 0.25f);

            m_is_view_dirty = true;
        }

        if (m_is_window_focused) {
            bool translate_held   = settings.m_gizmo_translate_mode_keybind.isInputMatching();
            bool rotate_held      = settings.m_gizmo_rotate_mode_keybind.isInputMatching();
            bool scale_held       = settings.m_gizmo_scale_mode_keybind.isInputMatching();

            if (translate_held) {
                m_gizmo_op = ImGuizmo::OPERATION::TRANSLATE;
            } else if (rotate_held) {
                m_gizmo_op = ImGuizmo::OPERATION::ROTATE;
            } else if (scale_held) {
                m_gizmo_op = ImGuizmo::OPERATION::SCALE | ImGuizmo::OPERATION::SCALEU;
            }

            // Camera movement
            {
                float lr_delta = 0, ud_delta = 0;
                if (Input::GetKey(Input::KeyCode::KEY_A)) {
                    lr_delta -= 1;
                }
                if (Input::GetKey(Input::KeyCode::KEY_D)) {
                    lr_delta += 1;
                }
                if (Input::GetKey(Input::KeyCode::KEY_W)) {
                    ud_delta += 1;
                }
                if (Input::GetKey(Input::KeyCode::KEY_S)) {
                    ud_delta -= 1;
                }
                if (Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT)) {
                    lr_delta *= 10.0f;
                    ud_delta *= 10.0f;
                }

                if (m_is_window_hovered) {
                    double delta_x, delta_y;
                    Input::GetMouseScrollDelta(delta_x, delta_y);
                    lr_delta += static_cast<float>(delta_x * 10.0f);
                    ud_delta += static_cast<float>(delta_y * 10.0f);
                }

                lr_delta *= static_cast<float>(settings.m_camera_speed * delta_time * 500.0f);
                ud_delta *= static_cast<float>(settings.m_camera_speed * delta_time * 500.0f);

                m_camera.translateLeftRight(-lr_delta);
                m_camera.translateFwdBack(ud_delta);

                if (lr_delta != 0.0f || ud_delta != 0.0f) {
                    m_is_view_dirty = true;
                }
            }
        }

        /*if (m_window_size.x != m_window_size_prev.x || m_window_size.y != m_window_size_prev.y) {
            m_camera.setAspect(m_render_size.y > 0 ? m_render_size.x / m_render_size.y
                                                   : FLT_EPSILON * 2);
        }*/

        float aspect = m_render_size.y > 0 ? m_render_size.x / m_render_size.y : FLT_EPSILON * 2;
        m_camera.setPerspective(glm::radians(settings.m_camera_fov), aspect, settings.m_near_plane,
                                settings.m_far_plane);
        m_camera.updateCamera();

        return true;
    }

    Renderer::selection_variant_t
    Renderer::findSelection(std::vector<ISceneObject::RenderInfo> renderables,
                            std::vector<RefPtr<Rail::RailNode>> rail_nodes, bool &should_reset) {
        const bool left_click  = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT);
        const bool right_click = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);

        should_reset = false;
        if (!m_is_window_hovered && !m_is_window_focused) {
            return std::nullopt;
        }

        // Mouse pos is absolute
        ImVec2 mouse_pos = ImGui::GetMousePos();
        glm::vec3 cam_pos;
        m_camera.getPos(cam_pos);

        // Get point on render window
        glm::vec3 selection_point = {mouse_pos.x - m_render_rect.Min.x,
                                     mouse_pos.y - m_render_rect.Min.y, 0};

        if (!left_click && !right_click) {
            return std::nullopt;
        }

        should_reset = true;

        // Generate ray from mouse position
        auto [rayOrigin, rayDirection] =
            getRayFromMouse(glm::vec2(selection_point.x, selection_point.y), m_camera,
                            glm::vec4(m_render_rect.Min.x, m_render_rect.Min.y,
                                      m_render_rect.Max.x - m_render_rect.Min.x,
                                      m_render_rect.Max.y - m_render_rect.Min.y));

        float nearest_intersection = std::numeric_limits<float>::max();

        selection_variant_t selected_item;
        if (J3D::Picking::IsPickingEnabled()) {
            selected_item = findObjectByJ3DPicking(
                renderables, static_cast<int>(selection_point.x),
                                                   static_cast<int>(selection_point.y),
                                                   nearest_intersection, s_selection_blacklist);

        } else {
            selected_item = findObjectByOBBIntersection(
                renderables, static_cast<int>(selection_point.x),
                static_cast<int>(selection_point.y), nearest_intersection, s_selection_blacklist);
        }

        for (auto &node : rail_nodes) {
            float this_intersection;

            if (intersectRaySphere(rayOrigin, rayDirection, node->getPosition(),
                                   RENDERER_RAIL_NODE_DIAMETER / 2.0f,
                                   this_intersection)) {
                // Intersection detected, check if nearest and use
                if (this_intersection >= nearest_intersection) {
                    continue;
                }
                nearest_intersection = this_intersection;
                selected_item        = node;
            }
        }

        return selected_item;
    }

    RefPtr<ISceneObject>
    Renderer::findObjectByJ3DPicking(std::vector<ISceneObject::RenderInfo> renderables,
                                     int selection_x, int selection_y, float &intersection_z,
                                     const std::unordered_set<std::string> &exclude_set) {
        TOOLBOX_DEBUG_LOG_V("Selection pt (x: {}, y: {})", selection_x, selection_y);
        if (J3D::Picking::IsPickingEnabled()) {
            J3D::Picking::ModelMaterialIdPair query_pair =
                J3D::Picking::Query(selection_x / 4.0f, (m_render_size.y - selection_y) / 4.0f);
            for (const ISceneObject::RenderInfo &info : renderables) {
                if (exclude_set.contains(info.m_object->type())) {
                    continue;
                }
                if (info.m_model->GetModelId() == std::get<0>(query_pair)) {
                    glm::vec3 selection_origin;
                    m_camera.getPos(selection_origin);

                    intersection_z = glm::length(selection_origin - info.m_transform.m_translation);
                    return info.m_object;
                }
            }
        }
        return RefPtr<ISceneObject>();
    }

    RefPtr<ISceneObject>
    Renderer::findObjectByOBBIntersection(std::vector<ISceneObject::RenderInfo> renderables,
                                          int selection_x, int selection_y, float &intersection_z,
                                          const std::unordered_set<std::string> &exclude_set) {
        float nearest_intersection = std::numeric_limits<float>::max();

        // Generate ray from mouse position
        auto [rayOrigin, rayDirection] =
            getRayFromMouse(glm::vec2(selection_x, selection_y), m_camera,
                            glm::vec4(m_render_rect.Min.x, m_render_rect.Min.y,
                                      m_render_rect.Max.x - m_render_rect.Min.x,
                                      m_render_rect.Max.y - m_render_rect.Min.y));

        RefPtr<ISceneObject> selected_obj = nullptr;

        for (auto &renderable : renderables) {
            if (exclude_set.contains(renderable.m_object->type())) {
                continue;
            }

            glm::vec3 min, max;

            // Bounding box is local
            renderable.m_model->GetBoundingBox(min, max);

            glm::mat4x4 obb_transform = glm::identity<glm::mat4x4>();
            obb_transform = glm::translate(obb_transform, renderable.m_transform.m_translation);

            glm::mat4x4 obb_rot_mtx = glm::eulerAngleXYZ(renderable.m_transform.m_rotation.x,
                                                         renderable.m_transform.m_rotation.y,
                                                         renderable.m_transform.m_rotation.z);

            obb_transform = obb_transform * obb_rot_mtx;

            if (renderable.m_object->type() != "SunModel") {
                obb_transform = glm::scale(obb_transform, renderable.m_transform.m_scale);
            }

            float this_intersection;

            // Perform ray-box intersection test
            if (intersectRayOBB(rayOrigin, rayDirection, min, max, obb_transform,
                                this_intersection)) {
                // Intersection detected, check if nearest and use
                if (this_intersection >= nearest_intersection) {
                    continue;
                }
                nearest_intersection = this_intersection;
                selected_obj         = renderable.m_object;
            }
        }

        intersection_z = nearest_intersection;
        return selected_obj;
    }

}  // namespace Toolbox::UI