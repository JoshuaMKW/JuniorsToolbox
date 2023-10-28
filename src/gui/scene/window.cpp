#include <glad/glad.h>

#include "gui/scene/window.hpp"

#include "gui/input.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include "gui/application.hpp"
#include <gui/IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iconv.h>

#if WIN32
#include <windows.h>
#endif

namespace Toolbox::UI {

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

    /* icu

    includes:

    #include <unicode/ucnv.h>
    #include <unicode/unistr.h>
    #include <unicode/ustring.h>

    std::string Utf8ToSjis(const std::string& value)
    {
        icu::UnicodeString src(value.c_str(), "utf8");
        int length = src.extract(0, src.length(), NULL, "shift_jis");

        std::vector<char> result(length + 1);
        src.extract(0, src.length(), &result[0], "shift_jis");

        return std::string(result.begin(), result.end() - 1);
    }

    std::string SjisToUtf8(const std::string& value)
    {
        icu::UnicodeString src(value.c_str(), "shift_jis");
        int length = src.extract(0, src.length(), NULL, "utf8");

        std::vector<char> result(length + 1);
        src.extract(0, src.length(), &result[0], "utf8");

        return std::string(result.begin(), result.end() - 1);
    }
    */

    std::string Utf8ToSjis(const std::string &value) {
        iconv_t conv = iconv_open("SHIFT_JISX0213", "UTF-8");
        if (conv == (iconv_t)(-1)) {
            throw std::runtime_error("Error opening iconv for UTF-8 to Shift-JIS conversion");
        }

        size_t inbytesleft = value.size();
        char *inbuf        = const_cast<char *>(value.data());

        size_t outbytesleft = value.size() * 2;
        std::string sjis(outbytesleft, '\0');
        char *outbuf = &sjis[0];

        if (iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)(-1)) {
            throw std::runtime_error("Error converting from UTF-8 to Shift-JIS");
        }

        sjis.resize(sjis.size() - outbytesleft);
        iconv_close(conv);
        return sjis;
    }

    std::string SjisToUtf8(const std::string &value) {
        iconv_t conv = iconv_open("UTF-8", "SHIFT_JISX0213");
        if (conv == (iconv_t)(-1)) {
            throw std::runtime_error("Error opening iconv for Shift-JIS to UTF-8 conversion");
        }

        size_t inbytesleft = value.size();
        char *inbuf        = const_cast<char *>(value.data());

        size_t outbytesleft = value.size() * 3;
        std::string utf8(outbytesleft, '\0');
        char *outbuf = &utf8[0];

        if (iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)(-1)) {
            throw std::runtime_error("Error converting from Shift-JIS to UTF-8");
        }

        utf8.resize(utf8.size() - outbytesleft);
        iconv_close(conv);
        return utf8;
    }

    void DrawTree(std::shared_ptr<Toolbox::Object::ISceneObject> root) {
        std::string node_name = std::format("{} ({})", root->type(), root->getNameRef().name());
        if (root->isGroupObject()) {
            bool open = ImGui::TreeNode(SjisToUtf8(node_name).c_str());
            if (open) {
                auto objects = std::dynamic_pointer_cast<Toolbox::Object::GroupSceneObject>(root)
                                   ->getChildren();
                if (objects.has_value()) {
                    for (auto object : objects.value()) {
                        DrawTree(object);
                    }
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::Text(SjisToUtf8(node_name).c_str());
        }
    }

    SceneWindow::~SceneWindow() {
        // J3DRendering::Cleanup();
        m_renderables.erase(m_renderables.begin(), m_renderables.end());
        m_model_cache.clear();

        glDeleteFramebuffers(1, &m_fbo_id);
        glDeleteRenderbuffers(1, &m_rbo_id);
        glDeleteTextures(1, &m_tex_id);
    }

    SceneWindow::SceneWindow() : DockWindow() {
        m_camera.setPerspective(150, 16 / 9, 100, 300000);
        m_camera.setOrientAndPosition({0, 1, 0}, {0, 0, 1}, {0, 0, 0});
        m_camera.updateCamera();
        J3DRendering::SetSortFunction(PacketSort);

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

    bool SceneWindow::loadData(const std::filesystem::path &path) {
        if (!Toolbox::exists(path)) {
            return false;
        }

        if (Toolbox::is_directory(path)) {
            if (path.filename() != "scene") {
                return false;
            }

            m_model_cache.erase(m_model_cache.begin(), m_model_cache.end());

            m_current_scene = std::make_unique<Toolbox::Scene::SceneInstance>(path);

            J3DModelLoader loader;
            for (const auto &entry : std::filesystem::directory_iterator(path / "mapobj")) {
                if (entry.path().extension() == ".bmd") {
                    bStream::CFileStream modelStream(entry.path().string(), bStream::Endianess::Big,
                                                     bStream::OpenMode::In);

                    m_model_cache.insert(
                        {entry.path().stem().string(), loader.Load(&modelStream, 0)});
                }
            }

            for (const auto &entry : std::filesystem::directory_iterator(path / "map" / "map")) {
                if (entry.path().extension() == ".bmd") {
                    std::cout << "[gui]: loading model " << entry.path().filename().string()
                              << std::endl;
                    bStream::CFileStream modelStream(entry.path().string(), bStream::Endianess::Big,
                                                     bStream::OpenMode::In);

                    m_model_cache.insert(
                        {entry.path().stem().string(), loader.Load(&modelStream, 0)});
                }
            }

            if (m_current_scene != nullptr) {
                auto rail_data = m_current_scene->getRailData();

                for (auto rail : rail_data) {
                    for (auto node : rail->nodes()) {
                        std::vector<PathPoint> connections;

                        PathPoint nodePoint =
                            (PathPoint){node->getPosition(), glm::vec4(1.0, 0.0, 1.0, 1.0), 64};

                        for (auto connection : rail->getNodeConnections(node)) {
                            PathPoint connectionPoint = (PathPoint){
                                connection->getPosition(), glm::vec4(1.0, 0.0, 1.0, 1.0), 64};
                            connections.push_back(nodePoint);
                            connections.push_back(connectionPoint);
                        };
                        m_path_renderer.m_paths.push_back(connections);
                    }
                }

                m_billboard_renderer.m_billboards.push_back(
                    (Billboard){glm::vec3(0, 1000, 0), 128, 0});
            }
            m_path_renderer.updateGeometry();

            return true;
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool SceneWindow::update(f32 deltaTime) {
        if (m_is_render_window_hovered && m_is_render_window_focused) {
            if (Input::GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {
                ImVec2 mouse_delta = Input::GetMouseDelta();

                m_camera.TurnLeftRight(-mouse_delta.x * 0.005);
                m_camera.TiltUpDown(-mouse_delta.y * 0.005);

                m_is_viewport_dirty = true;
            }

            float lr_delta = 0, ud_delta = 0;
            if (Input::GetKey(GLFW_KEY_A)) {
                lr_delta -= 1;
                m_is_viewport_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_D)) {
                lr_delta += 1;
                m_is_viewport_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_W)) {
                ud_delta += 1;
                m_is_viewport_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_S)) {
                ud_delta -= 1;
                m_is_viewport_dirty = true;
            }
            if (Input::GetKey(GLFW_KEY_LEFT_SHIFT)) {
                lr_delta *= 10;
                ud_delta *= 10;
            }

            lr_delta *= 10;
            ud_delta *= 10;

            m_camera.TranslateLeftRight(-lr_delta);
            m_camera.TranslateFwdBack(ud_delta);
        }

        if (m_render_window_size.x != m_render_window_size_prev.x ||
            m_render_window_size.y != m_render_window_size_prev.y) {
            m_camera.setAspect(m_render_size.y > 0 ? m_render_size.x / m_render_size.y
                                                   : FLT_EPSILON);
            m_is_viewport_dirty = true;
        }

        if (m_is_viewport_dirty)
            m_camera.updateCamera();

        return true;
    }

    void SceneWindow::viewportBegin(bool is_dirty) {
        ImGuiStyle &style = ImGui::GetStyle();

        m_render_window_size_prev = m_render_window_size;
        m_render_window_size      = ImGui::GetWindowSize();
        m_render_size             = ImVec2(m_render_window_size.x - style.WindowPadding.x * 2,
                                           m_render_window_size.y - style.WindowPadding.y * 2 -
                                               (style.FramePadding.y * 2.0f + ImGui::GetTextLineHeight()));

        // bind the framebuffer we want to render to
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);

        if (!is_dirty) {
            glBindTexture(GL_TEXTURE_2D, m_tex_id);
            return;
        }

        // window was resized, new texture and depth storage needed for framebuffer
        glDeleteRenderbuffers(1, &m_rbo_id);
        glDeleteTextures(1, &m_tex_id);

        glGenTextures(1, &m_tex_id);
        glBindTexture(GL_TEXTURE_2D, m_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_render_window_size.x, m_render_window_size.y, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex_id, 0);

        glGenRenderbuffers(1, &m_rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_render_window_size.x,
                              m_render_window_size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  m_rbo_id);

        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glViewport(0, 0, m_render_window_size.x, m_render_window_size.y);
    }

    void SceneWindow::viewportEnd() {
        ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(m_tex_id)), m_render_size,
                     {0.0f, 1.0f}, {1.0f, 0.0f});

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void SceneWindow::renderBody(f32 deltaTime) {
        ImGuiWindowClass hierarchyOverride;
        hierarchyOverride.ClassId =
            ImGui::GetID(getWindowChildUID(*this, "Hierarchy Editor").c_str());
        hierarchyOverride.ParentViewportId = m_window_id;
        hierarchyOverride.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingOverOther;
        ImGui::SetNextWindowClass(&hierarchyOverride);

        ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

        if (ImGui::Begin(getWindowChildUID(*this, "Hierarchy Editor").c_str())) {
            ImGui::Text("Map Objects");
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) /* Check if scene is loaded here*/) {
                // Add Object
            }

            ImGui::Separator();

            // Render Objects

            if (m_current_scene != nullptr) {
                auto root = m_current_scene->getObjHierarchy().getRoot();
                DrawTree(root);
            }

            ImGui::Spacing();
            ImGui::Text("Scene Info");
            ImGui::Separator();

            if (m_current_scene != nullptr) {
                auto root = m_current_scene->getTableHierarchy().getRoot();
                DrawTree(root);
            }
        }
        ImGui::End();

        ImGuiWindowClass propertiesOverride;
        propertiesOverride.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingOverOther;
        ImGui::SetNextWindowClass(&propertiesOverride);

        ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

        if (ImGui::Begin(getWindowChildUID(*this, "Properties Editor").c_str())) {
            // TODO: Render properties
        }
        ImGui::End();

        renderScene(deltaTime);

        // mGrid.Render(m_camera.GetPosition(), m_camera.GetProjectionMatrix(),
        // m_camera.GetViewMatrix());
    }

    void SceneWindow::renderScene(f32 delta_time) {
        m_renderables.clear();

        // Render Models here
        if (m_model_cache.count("map") != 0) {
            m_renderables.push_back(m_model_cache["map"]->GetInstance());
        }

        if (m_model_cache.count("sky") != 0) {
            m_renderables.push_back(m_model_cache["sky"]->GetInstance());
        }

        if (m_model_cache.count("sea") != 0) {
            m_renderables.push_back(m_model_cache["sea"]->GetInstance());
        }

        // perhaps find a way to limit this so it only happens when we need to re-render?
        if (m_current_scene != nullptr) {
            m_current_scene->getObjHierarchy().getRoot()->performScene(m_renderables,
                                                                       m_model_cache);
        }

        m_is_render_window_open = ImGui::Begin(getWindowChildUID(*this, "Scene View").c_str());
        if (m_is_render_window_open) {
            ImVec2 window_pos    = ImGui::GetWindowPos();
            m_render_window_rect = {
                window_pos,
                {window_pos.x + ImGui::GetWindowWidth(), window_pos.y + ImGui::GetWindowHeight()}
            };

            m_is_render_window_hovered = ImGui::IsWindowHovered();
            m_is_render_window_focused = ImGui::IsWindowFocused();

            if (m_is_render_window_hovered && Input::GetMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
                ImGui::SetWindowFocus();

            if (m_is_render_window_focused &&
                Input::GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)) {  // Mouse wrap
                ImVec2 mouse_pos   = ImGui::GetMousePos();
                ImVec2 window_pos  = ImGui::GetWindowPos();
                ImVec2 window_size = ImGui::GetWindowSize();

                bool wrapped = false;

                if (mouse_pos.x < m_render_window_rect.Min.x) {
                    mouse_pos.x += window_size.x;
                    wrapped = true;
                } else if (mouse_pos.x >= m_render_window_rect.Max.x) {
                    mouse_pos.x -= window_size.x;
                    wrapped = true;
                }

                if (mouse_pos.y < m_render_window_rect.Min.y) {
                    mouse_pos.y += window_size.y;
                    wrapped = true;
                } else if (mouse_pos.y >= m_render_window_rect.Max.y) {
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

            viewportBegin(m_is_viewport_dirty);
            if (m_is_viewport_dirty) {
                glm::mat4 projection, view;
                projection = m_camera.getProjMatrix();
                view       = m_camera.getViewMatrix();

                J3DUniformBufferObject::SetProjAndViewMatrices(&projection, &view);

                glm::vec3 position;
                m_camera.getPos(position);

                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                J3DRendering::Render(delta_time, position, view, projection, m_renderables);

                m_path_renderer.drawPaths(&m_camera);
                m_billboard_renderer.drawBillboards(&m_camera);

                m_is_viewport_dirty = false;
            }
            viewportEnd();
        }
        ImGui::End();
    }

    void SceneWindow::buildDockspace(ImGuiID dockspace_id) {
        m_dock_node_left_id =
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        m_dock_node_up_left_id   = ImGui::DockBuilderSplitNode(m_dock_node_left_id, ImGuiDir_Down,
                                                               0.5f, nullptr, &m_dock_node_left_id);
        m_dock_node_down_left_id = ImGui::DockBuilderSplitNode(
            m_dock_node_up_left_id, ImGuiDir_Down, 0.5f, nullptr, &m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Scene View").c_str(), dockspace_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Hierarchy Editor").c_str(),
                                     m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Properties Editor").c_str(),
                                     m_dock_node_down_left_id);
    }

    void SceneWindow::renderMenuBar() {}

};  // namespace Toolbox::UI