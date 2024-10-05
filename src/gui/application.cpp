#include <IconsForkAwesome.h>

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
#include <stb/stb_image.h>

#include "core/core.hpp"
#include "core/input/input.hpp"
#include "dolphin/hook.hpp"
#include "gui/application.hpp"
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
#include "gui/themes.hpp"
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
            TOOLBOX_WARN_V("(OpenGL) {}", message);
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            TOOLBOX_ERROR_V("(OpenGL) {}", message);
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

        NFD_Init();

        // TODO: Load application settings

        // Initialize the resource manager
        {
            fs_path cwd = Filesystem::current_path().value();

            m_resource_manager.includeResourcePath(cwd / "Fonts", true);
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

        m_render_window = glfwCreateWindow(1280, 720, "Junior's Toolbox", nullptr, nullptr);
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

        if (!m_settings_manager.initialize()) {
            TOOLBOX_ERROR("[INIT] Failed to initialize settings manager!");
        }

        const AppSettings &settings = m_settings_manager.getCurrentProfile();

        if (!m_font_manager.initialize()) {
            TOOLBOX_ERROR("[INIT] Failed to initialize font manager!");
        } else {
            m_font_manager.setCurrentFont(settings.m_font_family, settings.m_font_size);
        }

        TRY(TemplateFactory::initialize()).error([](const FSError &error) { LogError(error); });
        TRY(m_theme_manager.initialize()).error([](const FSError &error) { LogError(error); });

        DolphinHookManager::instance().setDolphinPath(settings.m_dolphin_path);

        m_dolphin_communicator.setRefreshRate(settings.m_dolphin_refresh_rate);
        m_dolphin_communicator.tStart(false, nullptr);

        m_task_communicator.tStart(false, nullptr);

        createWindow<LoggingWindow>("Application Log");

        determineEnvironmentConflicts();
        hookClipboardIntoGLFW();
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
            m_font_manager.setCurrentFont(settings.m_font_family, settings.m_font_size);

            TemplateFactory::setCacheMode(settings.m_is_template_cache_allowed);
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

        NFD_Quit();
    }

    void GUIApplication::onEvent(RefPtr<BaseEvent> ev) {
        CoreApplication::onEvent(ev);
        if (ev->getType() == EVENT_DROP) {
            DragDropManager::instance().destroyDragAction(
                DragDropManager::instance().getCurrentDragAction());
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

            for (size_t i = 0; i < m_windows_to_add.size(); ++i) {
                RefPtr<ImWindow> window = m_windows_to_add.front();
                m_windows_to_add.pop();

                addLayer(window);
                m_windows.push_back(window);
            }

            for (size_t i = 0; i < m_windows_to_gc.size(); ++i) {
                RefPtr<ImWindow> window = m_windows_to_gc.front();
                m_windows_to_gc.pop();

                removeLayer(window);
                std::erase(m_windows, window);
            }

            gcClosedWindows();
        }

        // Render imgui frame
        {
            ImGui::Render();
            finalizeFrame();
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
            if (ImGui::MenuItem("BMG")) {
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
                FileDialog::instance()->openDialog(m_load_path, m_render_window, true);
            }
            m_is_dir_dialog_open = false;
        }
        if (FileDialog::instance()->isDone()) {
            FileDialog::instance()->close();
            if (FileDialog::instance()->isOk()) {
                std::filesystem::path selected_path = FileDialog::instance()->getFilenameResult();
                if (m_project_manager.loadProjectFolder(selected_path)) {
                    TOOLBOX_INFO_V("Loaded project folder: {}", selected_path.string());
                    RefPtr<ProjectViewWindow> project_window =
                        createWindow<ProjectViewWindow>("Project View");
                    if (!project_window->onLoadData(selected_path)) {
                        TOOLBOX_ERROR("Failed to open project folder view!");
                        project_window->close();
                    }
                }
            }
        }

        if (m_is_file_dialog_open) {
            if (!FileDialog::instance()->isAlreadyOpen()) {
                FileDialogFilter filter;
                filter.addFilter("Nintendo Scene Archive", "szs,arc");
                FileDialog::instance()->openDialog(m_load_path, m_render_window, false, filter);
            }
            m_is_file_dialog_open = false;
        }
        if (FileDialog::instance()->isDone()) {
            FileDialog::instance()->close();
            if (FileDialog::instance()->isOk()) {
                std::filesystem::path selected_path = FileDialog::instance()->getFilenameResult();
                if (selected_path.extension() == ".szs" || selected_path.extension() == ".arc") {
                    RefPtr<SceneWindow> window = createWindow<SceneWindow>("Scene Editor");
                    if (!window->onLoadData(selected_path)) {
                        window->close();
                    }
                }
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
        glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    void FileDialog::openDialog(std::filesystem::path starting_path, GLFWwindow *parent_window,
                                bool is_directory, std::optional<FileDialogFilter> maybe_filters) {
        if (m_thread_initialized) {
            m_thread.join();
        } else {
            m_thread_initialized = true;
        }
        m_thread_running = true;
        m_closed         = false;
        auto fn          = [this, starting_path, parent_window, is_directory, maybe_filters]() {
            m_starting_path = starting_path.string();
            if (is_directory) {
                nfdpickfolderu8args_t args;
                args.defaultPath = m_starting_path.c_str();
                NFD_GetNativeWindowFromGLFWWindow(parent_window, &args.parentWindow);
                m_result = NFD_PickFolderU8_With(&m_selected_path, &args);
            } else {
                int num_filters                = 0;
                nfdu8filteritem_t *nfd_filters = nullptr;
                if (maybe_filters) {
                    FileDialogFilter filters = std::move(maybe_filters.value());
                    num_filters              = filters.numFilters();
                    filters.copyFiltersOutU8(m_filters);
                    nfd_filters = new nfdu8filteritem_t[num_filters];
                    for (int i = 0; i < num_filters; ++i) {
                        nfd_filters[i] = {m_filters[i].first.c_str(), m_filters[i].second.c_str()};
                    }
                }
                nfdopendialogu8args_t args;
                args.filterList  = const_cast<const nfdu8filteritem_t *>(nfd_filters);
                args.filterCount = num_filters;
                args.defaultPath = m_starting_path.c_str();
                NFD_GetNativeWindowFromGLFWWindow(parent_window, &args.parentWindow);
                m_result = NFD_OpenDialogU8_With(&m_selected_path, &args);
                if (maybe_filters) {
                    delete[] nfd_filters;
                }
            }
            m_thread_running = false;
        };
        m_thread = std::thread(fn);
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
