#include <IconsForkAwesome.h>

#include <netpp/http/router.h>
#include <netpp/netpp.h>
#include <netpp/tls/security.h>

#include <J3D/Material/J3DUniformBufferObject.hpp>

#include <iostream>
#include <span>
#include <string>
#include <thread>

// #include <GLFW/glfw3.h>
// #include <glad/glad.h>

#include <ImGuiFileDialog.h>
#include <J3D/J3DModelLoader.hpp>
#include <bstream.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb/stb_image.h>

#include "core/core.hpp"
#include "core/input/input.hpp"
#include "dolphin/hook.hpp"
#include "gui/application.hpp"
#include "gui/debugger/window.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/font.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/logging/errors.hpp"
#include "gui/logging/window.hpp"
#include "gui/pad/window.hpp"
#include "gui/project/window.hpp"
#include "gui/scene/ImGuizmo.h"
#include "gui/scene/window.hpp"
#include "gui/settings/window.hpp"
#include "gui/status/modal_failure.hpp"
#include "gui/status/modal_success.hpp"
#include "gui/themes.hpp"
#include "gui/updater/modal.hpp"
#include "gui/util.hpp"
#include "platform/service.hpp"
#include "scene/layout.hpp"
#include <nfd_glfw3.h>

// void ImGuiSetupTheme(bool, float);

namespace Toolbox {

    void DealWithGLErrors(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        case GL_DEBUG_SEVERITY_LOW:
            // TOOLBOX_INFO_V("(OpenGL) {}", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            TOOLBOX_WARN_V("(OpenGL - src: {}, type: {}, id: {}) {}", source, type, id, message);
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            TOOLBOX_ERROR_V("(OpenGL - src: {}, type: {}, id: {}) {}", source, type, id, message);
            break;
        }
    }

    GUIApplication::GUIApplication() {
        m_dockspace_built = false;
        m_dockspace_id    = ImGuiID();
        m_render_window   = nullptr;
        m_windows         = {};
    }

    GUIApplication &GUIApplication::instance() {
        static GUIApplication _inst;
        return _inst;
    }

    void GUIApplication::onInit(int argc, const char **argv) {
        // Initialize GLFW
        if (!glfwInit()) {
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

        if (!netpp::sockets_initialize()) {
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

        // Initialize the AppData directory
        const fs_path &app_data_path = GUIApplication::getAppDataPath();
        if (!Filesystem::exists(app_data_path).value_or(false)) {
            if (!Filesystem::create_directories(app_data_path).value_or(false)) {
                std::string err =
                    TOOLBOX_FORMAT_FN("[INIT] Failed to create application data directory at {}",
                                      app_data_path.string());
                fprintf(stderr, err.c_str());
                setExitCode(EXIT_CODE_FAILED_SETUP);
                stop();
                return;
            }
        }

        // TODO: Load application settings

        // Initialize the resource manager
        {
            fs_path cwd = Filesystem::current_path().value();

            m_resource_manager.includeResourcePath(cwd / "Fonts", true);
            m_resource_manager.includeResourcePath(cwd / "Fonts/Markdown", true);
            m_resource_manager.includeResourcePath(cwd / "Images", true);
            m_resource_manager.includeResourcePath(cwd / "Images", true);
            m_resource_manager.includeResourcePath(cwd / "Images/Icons", true);
            m_resource_manager.includeResourcePath(cwd / "Images/Icons/Filesystem", true);
            m_resource_manager.includeResourcePath(cwd / "Templates", false);
            m_resource_manager.includeResourcePath(cwd / "Themes", true);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DEPTH_BITS, 32);
        glfwWindowHint(GLFW_SAMPLES, 4);

        m_render_window = glfwCreateWindow(1280, 720, "Junior's Toolbox " TOOLBOX_VERSION_TAG, nullptr, nullptr);
        if (m_render_window == nullptr) {
            glfwTerminate();
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

        // Set window icon
        initializeIcon();

        glfwSetCharCallback(m_render_window, ImGui_ImplGlfw_CharCallback);
        glfwSetKeyCallback(m_render_window, Input::GLFWKeyCallback);
        glfwSetCursorPosCallback(m_render_window, Input::GLFWMousePositionCallback);
        glfwSetMouseButtonCallback(m_render_window, Input::GLFWMouseButtonCallback);
        glfwSetScrollCallback(m_render_window, Input::GLFWMouseScrollCallback);

        glfwMakeContextCurrent(m_render_window);
        gladLoadGL();
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glfwSwapInterval(1);

        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(DealWithGLErrors, nullptr);

        // Initialize imgui
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifdef IMGUI_HAS_VIEWPORT
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // This is necessary as ImGui thinks the render view is empty space
        // when the viewport is outside the main viewport
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        // TODO: Work around until further notice
        // Essentially, OpenGL style glfw crashes when dynamic
        // resizing an ImGui viewport for too long,
        // possibly due to ImGui refreshing every frame
        // ---
        // See: https://github.com/ocornut/imgui/issues/4534
        // and: https://github.com/ocornut/imgui/issues/3321
        // ---
        // Also review gui/window.hpp for window flags used
        // ---
#ifdef IMGUI_ENABLE_VIEWPORT_WORKAROUND
        io.ConfigViewportsNoAutoMerge  = true;
        io.ConfigViewportsNoDecoration = false;
#endif
#endif

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(m_render_window, false);
        ImGui_ImplOpenGL3_Init("#version 150");

        ImGuiPlatformIO &platform_io      = ImGui::GetPlatformIO();
        platform_io.Platform_CreateWindow = ImGui_ImplGlfw_CreateWindow_Ex;
        platform_io.Renderer_RenderWindow = ImGui_ImplOpenGL3_RenderWindow_Ex;

        DragDropManager::instance().initialize();
        m_drag_drop_source_delegate = DragDropDelegateFactory::createDragDropSourceDelegate();
        m_drag_drop_target_delegate = DragDropDelegateFactory::createDragDropTargetDelegate();

        if (!m_settings_manager.initialize(app_data_path / "Profiles")) {
            TOOLBOX_ERROR("[INIT] Failed to initialize settings manager!");
        }

        const AppSettings &settings = m_settings_manager.getCurrentProfile();

        if (!FontManager::instance().initialize()) {
            TOOLBOX_ERROR("[INIT] Failed to initialize font manager!");
        } else {
            FontManager::instance().setCurrentFont(settings.m_font_family, settings.m_font_size);
        }

        TemplateFactory::setCacheMode(
            m_settings_manager.getCurrentProfile().m_is_template_cache_allowed);
        TRY(TemplateFactory::initialize(app_data_path / ".cache/Templates/"))
            .error([](const FSError &error) { LogError(error); });

        TRY(m_theme_manager.initialize()).error([](const FSError &error) { LogError(error); });

        DolphinHookManager::instance().setDolphinPath(settings.m_dolphin_path);

        m_dolphin_communicator.setRefreshRate(settings.m_dolphin_refresh_rate);
        m_dolphin_communicator.tStart(false, nullptr);

        m_task_communicator.tStart(false, nullptr);

        createWindow<LoggingWindow>("Application Log");

        determineEnvironmentConflicts();
        hookClipboardIntoGLFW();

        if (settings.m_update_frequency != UpdateFrequency::NEVER) {
            RefPtr<UpdaterModal> updater_win = createWindow<UpdaterModal>("Update Checker");
            if (updater_win) {
                updater_win->hide();
            }
        }
    }

    void GUIApplication::onUpdate(TimeStep delta_time) {
        // Try to make sure we return an error if anything's fucky
        if (m_render_window == nullptr || glfwWindowShouldClose(m_render_window)) {
            stop();
            return;
        }

        // Apply input callbacks to detached viewports
        if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) {
            ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
            for (int i = 0; i < platform_io.Viewports.Size; ++i) {
                auto window = static_cast<GLFWwindow *>(platform_io.Viewports[i]->PlatformHandle);
                glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
                glfwSetKeyCallback(window, Input::GLFWKeyCallback);
                glfwSetCursorPosCallback(window, Input::GLFWMousePositionCallback);
                glfwSetMouseButtonCallback(window, Input::GLFWMouseButtonCallback);
                glfwSetScrollCallback(window, Input::GLFWMouseScrollCallback);
            }
        }

        // Render logic
        {
            glfwPollEvents();
            Input::UpdateInputState();

            render(delta_time);

            Input::PostUpdateInputState();
        }

        // Update settings
        {
            const AppSettings &settings = m_settings_manager.getCurrentProfile();

            DolphinHookManager::instance().setDolphinPath(settings.m_dolphin_path);
            m_dolphin_communicator.setRefreshRate(settings.m_dolphin_refresh_rate);
            FontManager::instance().setCurrentFont(settings.m_font_family, settings.m_font_size);

            TemplateFactory::setCacheMode(settings.m_is_template_cache_allowed);
            m_project_manager.getSceneLayoutManager().setIncludeCustomObjects(
                settings.m_is_custom_obj_allowed);
        }
    }

    void GUIApplication::onExit() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_render_window);
        glfwTerminate();

        J3DUniformBufferObject::DestroyUBO();

        m_windows.clear();

        m_dolphin_communicator.tKill(true);
        m_task_communicator.tKill(true);

        DragDropManager::instance().shutdown();

        netpp::sockets_deinitialize();
    }

    void GUIApplication::onEvent(RefPtr<BaseEvent> ev) {
        CoreApplication::onEvent(ev);

        if (ev->getType() == EVENT_DROP) {
            DragDropManager::instance().destroyDragAction(
                DragDropManager::instance().getCurrentDragAction());
        } else if (ev->getType() == EVENT_DRAG_MOVE || ev->getType() == EVENT_DRAG_ENTER ||
                   ev->getType() == EVENT_DRAG_LEAVE) {
            if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT, true)) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
            }
        }

        switch (ev->getType()) {
        case EVENT_DRAG_ENTER:
        case EVENT_DRAG_MOVE: {
            if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT, true)) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
            }
            break;
        }
        case EVENT_DRAG_LEAVE: {
            if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT, true)) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
            } else {
                RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
                if (action) {
                    action->setTargetUUID(0);
                }
            }
            break;
        }
        }
    }

    RefPtr<ImWindow> GUIApplication::findWindow(UUID64 uuid) {
        auto it = std::find_if(m_windows.begin(), m_windows.end(),
                               [&uuid](const auto &window) { return window->getUUID() == uuid; });
        return it != m_windows.end() ? *it : nullptr;
    }

    RefPtr<ImWindow> GUIApplication::findWindow(const std::string &title,
                                                const std::string &context) {
        auto it = std::find_if(m_windows.begin(), m_windows.end(),
                               [&title, &context](const auto &window) {
                                   return window->name() == title && window->context() == context;
                               });
        return it != m_windows.end() ? *it : nullptr;
    }

    std::vector<RefPtr<ImWindow>> GUIApplication::findWindows(const std::string &title) {
        std::vector<RefPtr<ImWindow>> result;
        std::copy_if(m_windows.begin(), m_windows.end(), std::back_inserter(result),
                     [&title](RefPtr<ImWindow> window) { return window->title() == title; });
        return result;
    }

#ifdef _WIN32
    const fs_path &GUIApplication::getAppDataPath() const {
        static fs_path app_data_path = []() -> fs_path {
            char *appdata = nullptr;
            size_t len    = 0;
            _dupenv_s(&appdata, &len, "APPDATA");
            if (appdata) {
                fs_path path = fs_path(appdata) / "JuniorsToolbox";
                free(appdata);
                return path;
            } else {
                return fs_path();
            }
        }();
        return app_data_path;
    }
#else
    const fs_path &GUIApplication::getAppDataPath() const {
        static fs_path app_data_path = []() -> fs_path {
            char *home = std::getenv("HOME");
            if (home) {
                fs_path path = fs_path(home) / ".juniorstoolbox";
                return path;
            } else {
                return fs_path();
            }
        }();
        return app_data_path;
    }
#endif

    RefPtr<ImWindow> GUIApplication::getImWindowFromPlatformWindow(Platform::LowWindow low_window) {
        for (RefPtr<ImWindow> window : m_windows) {
            if (window->getLowHandle() == low_window) {
                return window;
            }
        }

        return nullptr;
    }

    void GUIApplication::showSuccessModal(ImWindow *parent, const std::string &title,
                                          const std::string &message) {
        m_success_modal_queue.emplace_back(parent, title, message);
    }

    void GUIApplication::showErrorModal(ImWindow *parent, const std::string &title,
                                        const std::string &message) {
        m_error_modal_queue.emplace_back(parent, title, message);
    }

    bool GUIApplication::registerDragDropSource(Platform::LowWindow window) {
        return m_drag_drop_source_delegate->initializeForWindow(window);
    }

    void GUIApplication::deregisterDragDropSource(Platform::LowWindow window) {
        m_drag_drop_source_delegate->shutdownForWindow(window);
    }

    bool GUIApplication::registerDragDropTarget(Platform::LowWindow window) {
        return m_drag_drop_target_delegate->initializeForWindow(window);
    }

    void GUIApplication::deregisterDragDropTarget(Platform::LowWindow window) {
        m_drag_drop_target_delegate->shutdownForWindow(window);
    }

    bool GUIApplication::startDragAction(Platform::LowWindow source, RefPtr<DragAction> action) {
        m_pending_drag_action = action;
        m_pending_drag_window = source;
        return true;
    }

    void GUIApplication::registerDolphinOverlay(UUID64 scene_uuid, const std::string &name,
                                                SceneWindow::render_layer_cb cb) {
        RefPtr<SceneWindow> scene_window = ref_cast<SceneWindow>(findWindow(scene_uuid));
        if (scene_window) {
            scene_window->registerOverlay(name, cb);
        }
    }

    void GUIApplication::deregisterDolphinOverlay(UUID64 scene_uuid, const std::string &name) {
        RefPtr<SceneWindow> scene_window = ref_cast<SceneWindow>(findWindow(scene_uuid));
        if (scene_window) {
            scene_window->deregisterOverlay(name);
        }
    }

    void GUIApplication::initializeIcon() {
        auto result = m_resource_manager.getRawData(
            "toolbox.png", m_resource_manager.getResourcePathUUID(fs_path("Images") / "Icons"));
        if (!result) {
            TOOLBOX_ERROR("Failed to load toolbox icon!");
            return;
        }

        std::span<u8> data_span = std::move(result.value());

        // Load image data
        {
            int width, height, channels;
            stbi_uc *data = stbi_load_from_memory(data_span.data(), data_span.size(), &width,
                                                  &height, &channels, 4);

            GLFWimage icon = {width, height, data};
            glfwSetWindowIcon(m_render_window, 1, &icon);

            stbi_image_free(data);
        }
    }

    void GUIApplication::render(TimeStep delta_time) {  // Begin actual rendering
        glfwMakeContextCurrent(m_render_window);

        // The context renders both the ImGui elements and the background elements.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();

        ImGui::PushFont(FontManager::instance().getCurrentFont(),
                        FontManager::instance().getCurrentFontSize());

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        m_dockspace_id = ImGui::DockSpaceOverViewport(0, viewport);

        m_dockspace_built = ImGui::DockBuilderGetNode(m_dockspace_id);
        if (!m_dockspace_built) {
            ImGui::DockBuilderAddNode(m_dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderFinish(m_dockspace_id);
        }

#ifdef TOOLBOX_DEBUG
        ImGui::ShowMetricsWindow();
        ImGui::ShowDebugLogWindow();
#endif

        renderMenuBar();

        // Update dock context
        for (auto &window : m_windows) {
            if (!m_dockspace_built && !m_docked_map[window->getUUID()]) {
                std::string window_name =
                    std::format("{}###{}", window->title(), window->getUUID());
                ImGui::DockBuilderDockWindow(window_name.c_str(), m_dockspace_id);
                m_docked_map[window->getUUID()] = true;
            }
        }

        // UPDATE LOOP: We update layers here so the ImGui dockspaces can take effect first.
        {
            m_windows_processing = true;
            CoreApplication::onUpdate(delta_time);
            m_windows_processing = false;

            for (auto it = m_windows_to_gc.begin(); it != m_windows_to_gc.end();) {
                const std::pair<GCTimeInfo, RefPtr<ImWindow>> &window_gc_data = m_windows_to_gc.front();
                if (!window_gc_data.first.isReadyToGC()) {
                    ++it;
                    continue;
                }

                RefPtr<ImWindow> window = window_gc_data.second;
                it = m_windows_to_gc.erase(it);

                removeLayer(window);
                std::erase(m_windows, window);
            }

            // Loop over queued status modals
            if (!m_success_modal_queue.empty()) {
                SuccessModal &modal = m_success_modal_queue.front();
                if (!modal.is_open()) {
                    modal.open();
                }

                if (!modal.render()) {
                    modal.close();  // It is somehow invisible
                }

                if (modal.is_closed()) {
                    m_success_modal_queue.erase(m_success_modal_queue.begin());
                }
            } else {
                // Loop over queued status modals
                if (!m_error_modal_queue.empty()) {
                    FailureModal &modal = m_error_modal_queue.front();
                    if (!modal.is_open()) {
                        modal.open();
                    }

                    if (!modal.render()) {
                        modal.close();  // It is somehow invisible
                    }

                    if (modal.is_closed()) {
                        m_error_modal_queue.erase(m_error_modal_queue.begin());
                    }
                }
            }

            gcClosedWindows();

            while (!m_windows_to_add.empty()) {
                RefPtr<ImWindow> window = m_windows_to_add.front();
                m_windows_to_add.erase(m_windows_to_add.begin());

                addLayer(window);
                m_windows.push_back(window);
            }
        }

        // Render drag icon
        {
            if (m_await_drag_drop_destroy) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
                m_await_drag_drop_destroy = false;
            }

            if (DragDropManager::instance().getCurrentDragAction() &&
                Input::GetMouseButtonUp(MouseButton::BUTTON_LEFT, true)) {
                m_await_drag_drop_destroy = true;
            }

            if (RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction()) {
                double x, y;
                Input::GetMousePosition(x, y);

                action->setHotSpot({(float)x, (float)y});

                const ImVec2 &hotspot = action->getHotSpot();

                ImGuiViewportP *viewport =
                    ImGui::FindHoveredViewportFromPlatformWindowStack(hotspot);
                if (viewport) {
                    const ImVec2 size = {96, 96};
                    const ImVec2 pos  = {hotspot.x - size.x / 2.0f, hotspot.y - size.y / 1.1f};

                    ImGuiStyle &style = ImGui::GetStyle();

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
                    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                          ImGui::GetStyleColorVec4(ImGuiCol_Text));

                    ImGui::SetNextWindowBgAlpha(1.0f);
                    ImGui::SetNextWindowPos(pos);
                    ImGui::SetNextWindowSize(size);
                    if (ImGui::Begin("###Drag Icon", nullptr,
                                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                         ImGuiWindowFlags_NoFocusOnAppearing |
                                         ImGuiWindowFlags_NoMouseInputs)) {
                        action->render(size);

                        // ImVec2 status_size = {16, 16};
                        // ImGui::SetCursorScreenPos(pos + size - status_size);

                        // ImGui::DrawSquare(
                        //     pos + size - (status_size / 2), status_size.x, IM_COL32_WHITE,
                        //     ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_HeaderActive]));

                        const DragAction::DropState &state = action->getDropState();
                        if (state.m_valid_target) {

                        } else {
                            ResourceManager &res_manager = m_resource_manager;
                            UUID64 directory_id = res_manager.getResourcePathUUID("Images/Icons");
                            RefPtr<const ImageHandle> img =
                                res_manager.getImageHandle("invalid_circle.png", directory_id)
                                    .value_or(nullptr);
                            if (img) {
                                const ImVec2 stat_size = {16.0f, 16.0f};

                                ImagePainter painter;
                                painter.render(*img, pos + size - stat_size - style.WindowPadding,
                                               stat_size);
                            }
                        }

                        if (ImGuiWindow *win = ImGui::GetCurrentWindow()) {
                            if (win->Viewport != ImGui::GetMainViewport()) {
                                Platform::SetWindowClickThrough(
                                    (Platform::LowWindow)win->Viewport->PlatformHandleRaw, true);
                                m_drag_drop_viewport = win->Viewport;
                            } else {
                                m_drag_drop_viewport = nullptr;
                            }
                        }

                        ImGui::Dummy({0, 0});  // Submit an empty item so ImGui resizes the parent
                                               // window bounds.
                    }
                    ImGui::End();

                    ImGui::PopStyleColor(1);
                    ImGui::PopStyleVar(1);
                }
            }
        }

        ImGui::PopFont();

        // Render imgui frame
        {
            ImGui::Render();
            finalizeFrame();
        }

        if (m_pending_drag_action) {
#if 0
            const MimeData &data = m_pending_drag_action->getPayload();
            DropType out;
            bool result = m_drag_drop_source_delegate->startDragDrop(
                m_pending_drag_window, data, m_pending_drag_action->getSupportedDropTypes(), &out);
            TOOLBOX_DEBUG_LOG_V("DragDropSourceDelegate::startDragDrop action type: {}", int(out));
#else
            // TODO: Fix DoDragDrop locking ImGui/GLFW
            DragDropManager::instance().destroyDragAction(m_pending_drag_action);
            m_pending_drag_action = nullptr;
#endif
        }
    }

    void GUIApplication::renderMenuBar() {
        ImGui::BeginMainMenuBar();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_FK_FOLDER_OPEN " Open...")) {
                m_is_file_dialog_open = true;
            }
            if (ImGui::MenuItem(ICON_FK_FOLDER_OPEN " Open Folder...")) {
                m_is_dir_dialog_open = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem(ICON_FK_FLOPPY_O " Save All")) {
                for (auto &window : m_windows) {
                    (void)window->onSaveData(std::nullopt);
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem(ICON_FK_WINDOW_CLOSE " Close All")) {
                for (auto &window : m_windows) {
                    removeLayer(window);
                }
                m_windows.clear();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem(ICON_FK_COG " Settings")) {
                createWindow<SettingsWindow>("Application Settings");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::SeparatorText("Utilities");

            if (ImGui::MenuItem("Application Log")) {
                createWindow<LoggingWindow>("Application Log");
            }

            if (ImGui::MenuItem("Memory Debugger")) {
                createWindow<DebuggerWindow>("Memory Debugger");
            }

            ImGui::SeparatorText("Asset Editors");

            if (ImGui::MenuItem("BMG")) {
                // TODO: createWindow<BMGEditorWindow>("BMG Editor");
            }

            if (ImGui::MenuItem("PAD")) {
                createWindow<PadInputWindow>("Pad Recorder");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FK_QUESTION_CIRCLE)) {
            if (ImGui::MenuItem("About")) {
                // TODO: Create about window
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        if (m_is_dir_dialog_open) {
            if (!FileDialog::instance()->isAlreadyOpen()) {
                FileDialog::instance()->openDialog(m_render_window, m_load_path, true);
            }
            m_is_dir_dialog_open = false;
        }

        if (m_is_file_dialog_open) {
            if (!FileDialog::instance()->isAlreadyOpen()) {
                FileDialogFilter filter;
                filter.addFilter("Nintendo Scene Archive", "szs,arc");
                FileDialog::instance()->openDialog(m_render_window, m_load_path, false, filter);
            }
            m_is_file_dialog_open = false;
        }

        if (FileDialog::instance()->isDone(m_render_window)) {
            FileDialog::instance()->close();
            if (FileDialog::instance()->isOk()) {
                std::filesystem::path selected_path = FileDialog::instance()->getFilenameResult();
                bool is_dir = Filesystem::is_directory(selected_path).value_or(false);
                if (is_dir) {
                    if (m_project_manager.loadProjectFolder(selected_path)) {
                        TOOLBOX_INFO_V("Loaded project folder: {}", selected_path.string());
                        RefPtr<ProjectViewWindow> project_window =
                            createWindow<ProjectViewWindow>("Project View");
                        if (!project_window->onLoadData(selected_path)) {
                            GUIApplication::instance().showErrorModal(
                                nullptr, "Application",
                                "Failed to open the folder as a project!\n\n - (Check application "
                                "log for details)");
                            project_window->close();
                        }
                    }
                }

                // if (selected_path.extension() == ".szs" || selected_path.extension() == ".arc") {
                //     RefPtr<SceneWindow> window = createWindow<SceneWindow>("Scene Editor");
                //     if (!window->onLoadData(selected_path)) {
                //         GUIApplication::instance().showErrorModal(
                //             nullptr, "Application",
                //             "Failed to open the file as a scene!\n\n - (Check application "
                //             "log for details)");
                //         window->close();
                //     }
                // }
            }
        }

        if (ImGui::BeginPopupModal("Scene Load Error", NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Error Loading Scene\n\n");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void GUIApplication::finalizeFrame() {
        // Update buffer size
        {
            int width, height;
            glfwGetFramebufferSize(m_render_window, &width, &height);
            glViewport(0, 0, width, height);
        }

        // Clear the buffer
        // glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiStyle &style = ImGui::GetStyle();
        ImGuiIO &io       = ImGui::GetIO();

        // Update backend framework
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) {
#ifndef IMGUI_ENABLE_VIEWPORT_WORKAROUND
            style.WindowRounding              = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
#endif

            GLFWwindow *backup_window = glfwGetCurrentContext();
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
            glfwMakeContextCurrent(backup_window);
            m_render_window = backup_window;
        }

        // Swap buffers to reset context for next frame
        glfwSwapBuffers(m_render_window);
    }

    bool GUIApplication::determineEnvironmentConflicts() {
        bool conflicts_found = false;

        TRY(Platform::IsServiceRunning("NahimicService"))
            .err([](const BaseError &error) { LogError(error); })
            .ok([&conflicts_found](bool running) {
                if (running) {
                    TOOLBOX_WARN("Found Nahimic service running on this system, this could cause "
                                 "crashes to occur on window resize!");
                    conflicts_found = true;
                }
            });

        return conflicts_found;
    }

    void GUIApplication::gcClosedWindows() {
        // Check for closed windows that need destroyed
        for (auto it = m_windows.begin(); it != m_windows.end();) {
            RefPtr<ImWindow> win = *it;
            if (win->isClosed()) {
                win->onDetach();
                if (win->destroyOnClose()) {
                    it = m_windows.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    bool GUIApplication::GCTimeInfo::isReadyToGC() const {
        TimePoint now = std::chrono::high_resolution_clock::now();
        TimeStep dur  = TimeStep(m_closed_time, now);
        return dur.seconds() >= m_seconds_to_close;
    }

    void FileDialog::openDialog(ImGuiWindow *parent_window,
                                const std::filesystem::path &starting_path, bool is_directory,
                                std::optional<FileDialogFilter> maybe_filters) {
        GLFWwindow *op_window = static_cast<GLFWwindow *>(parent_window->Viewport->PlatformHandle);
        openDialog(op_window, starting_path, is_directory, maybe_filters);
    }

    void FileDialog::openDialog(GLFWwindow *parent_window,
                                const std::filesystem::path &starting_path, bool is_directory,
                                std::optional<FileDialogFilter> maybe_filters) {
        if (m_thread_initialized) {
            m_thread.join();
        } else {
            m_thread_initialized = true;
        }
        m_thread_running = true;
        m_closed         = false;
        m_result         = NFD_ERROR;

        m_control_info.m_owner         = parent_window;
        m_control_info.m_starting_path = starting_path.string();
        m_control_info.m_default_name  = "";
        m_control_info.m_opt_filters   = maybe_filters;
        m_control_info.m_file_mode     = FileNameMode::MODE_OPEN;
        m_control_info.m_is_directory  = is_directory;

        m_thread = std::thread([&]() { NFD_OpenDialogRoutine(*this); });
    }

    void FileDialog::saveDialog(ImGuiWindow *parent_window,
                                const std::filesystem::path &starting_path,
                                const std::string &default_name, bool is_directory,
                                std::optional<FileDialogFilter> maybe_filters) {
        GLFWwindow *op_window = static_cast<GLFWwindow *>(parent_window->Viewport->PlatformHandle);
        saveDialog(op_window, starting_path, default_name, is_directory, maybe_filters);
    }

    void FileDialog::saveDialog(GLFWwindow *parent_window,
                                const std::filesystem::path &starting_path,
                                const std::string &default_name, bool is_directory,
                                std::optional<FileDialogFilter> maybe_filters) {
        if (m_thread_initialized) {
            m_thread.join();
        } else {
            m_thread_initialized = true;
        }
        m_thread_running = true;
        m_closed         = false;
        m_result         = NFD_ERROR;

        m_control_info.m_owner         = parent_window;
        m_control_info.m_starting_path = starting_path.string();
        m_control_info.m_default_name  = "";
        m_control_info.m_opt_filters   = maybe_filters;
        m_control_info.m_file_mode     = FileNameMode::MODE_SAVE;
        m_control_info.m_is_directory  = is_directory;

        m_thread = std::thread([&]() { NFD_SaveDialogRoutine(*this); });
    }

    nfdresult_t FileDialog::NFD_OpenDialogRoutine(FileDialog &self) {
        NFD_Init();

        if (self.m_control_info.m_is_directory) {
            nfdpickfolderu8args_t args;
            args.defaultPath = self.m_control_info.m_starting_path.c_str();
            NFD_GetNativeWindowFromGLFWWindow(self.m_control_info.m_owner, &args.parentWindow);
            self.m_result = NFD_PickFolderU8_With(&self.m_selected_path, &args);
        } else {
            int num_filters                = 0;
            nfdu8filteritem_t *nfd_filters = nullptr;
            if (self.m_control_info.m_opt_filters) {
                FileDialogFilter filters = std::move(self.m_control_info.m_opt_filters.value());
                num_filters              = filters.numFilters();
                filters.copyFiltersOutU8(self.m_filters);
                nfd_filters = new nfdu8filteritem_t[num_filters];
                for (int i = 0; i < num_filters; ++i) {
                    nfd_filters[i] = {self.m_filters[i].first.c_str(),
                                      self.m_filters[i].second.c_str()};
                }
            }
            nfdopendialogu8args_t args;
            args.filterList  = const_cast<const nfdu8filteritem_t *>(nfd_filters);
            args.filterCount = num_filters;
            args.defaultPath = self.m_control_info.m_starting_path.c_str();
            NFD_GetNativeWindowFromGLFWWindow(self.m_control_info.m_owner, &args.parentWindow);
            self.m_result = NFD_OpenDialogU8_With(&self.m_selected_path, &args);
            if (self.m_control_info.m_opt_filters) {
                delete[] nfd_filters;
            }
        }

        NFD_Quit();
        self.m_thread_running = false;
        return self.m_result.value_or(NFD_ERROR);
    }

    nfdresult_t FileDialog::NFD_SaveDialogRoutine(FileDialog &self) {
        if (self.m_control_info.m_is_directory) {
            nfdpickfolderu8args_t args;
            args.defaultPath = self.m_control_info.m_starting_path.c_str();
            NFD_GetNativeWindowFromGLFWWindow(self.m_control_info.m_owner, &args.parentWindow);
            self.m_result = NFD_PickFolderU8_With(&self.m_selected_path, &args);
        } else {
            int num_filters                = 0;
            nfdu8filteritem_t *nfd_filters = nullptr;
            if (self.m_control_info.m_opt_filters) {
                FileDialogFilter filters = std::move(self.m_control_info.m_opt_filters.value());
                num_filters              = filters.numFilters();
                filters.copyFiltersOutU8(self.m_filters);
                nfd_filters = new nfdu8filteritem_t[num_filters];
                for (int i = 0; i < num_filters; ++i) {
                    nfd_filters[i] = {self.m_filters[i].first.c_str(),
                                      self.m_filters[i].second.c_str()};
                }
            }
            nfdsavedialogu8args_t args;
            args.filterList  = const_cast<const nfdu8filteritem_t *>(nfd_filters);
            args.filterCount = num_filters;
            args.defaultPath = self.m_control_info.m_starting_path.c_str();
            args.defaultName = self.m_control_info.m_default_name.c_str();
            NFD_GetNativeWindowFromGLFWWindow(self.m_control_info.m_owner, &args.parentWindow);
            self.m_result = NFD_SaveDialogU8_With(&self.m_selected_path, &args);
            if (self.m_control_info.m_opt_filters) {
                delete[] nfd_filters;
            }
        }
        self.m_thread_running = false;
        return self.m_result.value_or(NFD_ERROR);
    }

    void FileDialogFilter::addFilter(const std::string &label, const std::string &csv_filters) {
        m_filters.push_back({std::string(label), std::string(csv_filters)});
    }

    bool FileDialogFilter::hasFilter(const std::string &label) const {
        return std::find_if(m_filters.begin(), m_filters.end(),
                            [label](std::pair<std::string, std::string> p) {
                                return p.first == label;
                            }) != m_filters.end();
    }

    void FileDialogFilter::copyFiltersOutU8(
        std::vector<std::pair<std::string, std::string>> &out_filters) const {
        out_filters = m_filters;
    }
}  // namespace Toolbox

// void ImGuiSetupTheme(bool bStyleDark_, float alpha_) {
//     ImGuiStyle &style = ImGui::GetStyle();
//
// #define HEADER_COLOR         0.96f, 0.18f, 0.12f
// #define HEADER_ACTIVE_COLOR  0.96f, 0.18f, 0.12f
// #define HEADER_HOVERED_COLOR 0.96f, 0.18f, 0.12f
// #define ACCENT_COLOR         0.88f, 0.82f, 0.34f
// #define ACCENT_HOVERED_COLOR  0.96f, 0.90f, 0.38f
// #define ACCENT_ACTIVE_COLOR         0.92f, 0.86f, 0.35f
//
//     // light style from Pacôme Danhiez (user itamago)
//     // https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
//     style.Alpha                                  = 1.0f;
//     style.FrameRounding                          = 3.0f;
//     style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
//     style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
//     style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.24f, 0.58f, 0.90f, 0.94f);
//     style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
//     style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.26f, 0.60f, 0.92f, 0.94f);
//     style.Colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
//     style.Colors[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
//     style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
//     style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(ACCENT_COLOR, 0.78f);
//     style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(ACCENT_COLOR, 0.88f);
//     style.Colors[ImGuiCol_TitleBg]               = ImVec4(HEADER_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(HEADER_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(HEADER_COLOR, 0.70f);
//     style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(HEADER_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
//     style.Colors[ImGuiCol_CheckMark]             = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_SliderGrab]            = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Button]                = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ButtonActive]          = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Header]                = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_HeaderActive]          = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Separator]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
//     style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(ACCENT_HOVERED_COLOR, 0.78f);
//     style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Tab]                   = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabHovered]            = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabActive]             = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_DockingPreview]        = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
//     style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
//     style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
//     style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
//     style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
//     style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(ACCENT_COLOR, 0.78f);
//     style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(ACCENT_COLOR, 0.78f);
//     style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
//     style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_NavHighlight]          = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
//     style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
//
//     if (bStyleDark_) {
//         for (int i = 0; i <= ImGuiCol_COUNT; i++) {
//             ImVec4 &col = style.Colors[i];
//             float H, S, V;
//             ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);
//
//             if (S < 0.1f) {
//                 V = 1.0f - V;
//             }
//             ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
//             if (col.w < 1.00f) {
//                 col.w *= alpha_;
//             }
//         }
//     } else {
//         for (int i = 0; i <= ImGuiCol_COUNT; i++) {
//             ImVec4 &col = style.Colors[i];
//             if (col.w < 1.00f) {
//                 col.x *= alpha_;
//                 col.y *= alpha_;
//                 col.z *= alpha_;
//                 col.w *= alpha_;
//             }
//         }
//     }
// }
