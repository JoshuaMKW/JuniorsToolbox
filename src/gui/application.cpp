#include "IconsForkAwesome.h"
#include "core/input/input.hpp"
#include "gui/font.hpp"
#include "gui/settings/window.hpp"
#include "gui/themes.hpp"
#include "gui/util.hpp"
#include "platform/service.hpp"

#include <J3D/Material/J3DUniformBufferObject.hpp>

#include <iostream>
#include <string>
#include <thread>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <ImGuiFileDialog.h>
#include <J3D/J3DModelLoader.hpp>
#include <bstream.h>
#include <gui/logging/errors.hpp>
#include <gui/logging/window.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

#include "core/core.hpp"
#include "dolphin/hook.hpp"
#include "gui/application.hpp"
#include "gui/scene/ImGuizmo.h"

// void ImGuiSetupTheme(bool, float);

namespace Toolbox {

    int GUIApplication::run() {
        Clock::time_point lastFrameTime, thisFrameTime;

        while (true) {
            lastFrameTime = thisFrameTime;
            thisFrameTime = Util::GetTime();

            f32 delta_time = Util::GetDeltaTime(lastFrameTime, thisFrameTime);

            if (!execute(delta_time))
                break;

            render(delta_time);

            postRender(delta_time);
        }

        return 0;
    }

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

    void GUIApplication::onInit() {
        // Initialize GLFW
        if (!glfwInit()) {
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

        // TODO: Load application settings
        //
        // ----

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif
        glfwWindowHint(GLFW_DEPTH_BITS, 32);
        glfwWindowHint(GLFW_SAMPLES, 4);

        m_render_window = glfwCreateWindow(1280, 720, "Junior's Toolbox", nullptr, nullptr);
        if (m_render_window == nullptr) {
            glfwTerminate();
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

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

        auto &settings_manager = SettingsManager::instance();
        settings_manager.initialize();

        auto &font_manager = FontManager::instance();
        font_manager.initialize();
        font_manager.setCurrentFont("NotoSansJP-Regular", 16.0f);

        // glEnable(GL_MULTISAMPLE);

        {
            auto result = Object::TemplateFactory::initialize();
            if (!result) {
                LogError(result.error());
            }
        }

        {
            auto result = ThemeManager::instance().initialize();
            if (!result) {
                LogError(result.error());
            }
        }

        auto settings_window = make_referable<SettingsWindow>();
        m_windows.push_back(settings_window);

        auto logging_window = make_referable<LoggingWindow>();
        logging_window->open();
        m_windows.push_back(logging_window);

        determineEnvironmentConflicts();

        m_dolphin_communicator.start();
    }

    void GUIApplication::onUpdate(TimeStep delta_time) {}

    void GUIApplication::onExit() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_render_window);
        glfwTerminate();

        J3DUniformBufferObject::DestroyUBO();

        m_windows.clear();

        m_dolphin_communicator.kill();
    }

    bool GUIApplication::execute(f32 delta_time) {
        // Try to make sure we return an error if anything's fucky
        if (m_render_window == nullptr || glfwWindowShouldClose(m_render_window))
            return false;

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

        glfwPollEvents();
        Input::UpdateInputState();

        // Update viewer context
        for (auto &window : m_windows) {
            if (!window->update(delta_time)) {
                return false;
            }
        }

        return true;
    }

    void GUIApplication::render(f32 delta_time) {  // Begin actual rendering
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
        m_dockspace_id = ImGui::DockSpaceOverViewport(viewport);

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
        renderWindows(delta_time);

        // Render imgui
        ImGui::Render();

        // Update buffer size
        int width, height;
        glfwGetFramebufferSize(m_render_window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiStyle &style = ImGui::GetStyle();
        ImGuiIO &io       = ImGui::GetIO();

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

        // Swap buffers
        glfwSwapBuffers(m_render_window);
    }

    void GUIApplication::renderMenuBar() {
        m_options_open = false;
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
                    (void)window->saveData(std::nullopt);
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
                m_options_open = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(ICON_FK_QUESTION_CIRCLE)) {
            if (ImGui::MenuItem("About")) {
                m_options_open = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        if (m_is_dir_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog("OpenDirDialog", "Choose Project Path", nullptr,
                                                    m_load_path.string(), "");
        }

        if (ImGuiFileDialog::Instance()->Display("OpenDirDialog")) {
            ImGuiFileDialog::Instance()->Close();
            m_is_dir_dialog_open = false;

            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                auto dir_result = Toolbox::is_directory(path);
                if (!dir_result) {
                    return;
                }

                if (!dir_result.value()) {
                    return;
                }

                m_load_path = path;

                if (path.filename() == "scene") {
                    auto scene_window = make_referable<SceneWindow>();

                    for (auto window : m_windows) {
                        if (window->name() == scene_window->name() &&
                            window->context() == path.string()) {
                            window->open();
                            ImGui::SetWindowFocus(window->title().c_str());
                            return;
                        }
                    }

                    if (!scene_window->loadData(path)) {
                        return;
                    }
                    scene_window->open();
                    m_windows.push_back(scene_window);
                } else {
                    auto sys_path   = std::filesystem::path(path) / "sys";
                    auto files_path = std::filesystem::path(path) / "files";

                    auto sys_result   = Toolbox::is_directory(sys_path);
                    auto files_result = Toolbox::is_directory(files_path);

                    if ((sys_result && sys_result.value()) &&
                        (files_result && files_result.value())) {
                        // TODO: Open project folder view
                        m_project_root = path;
                        TOOLBOX_INFO_V("Project root: {}", m_project_root.string());
                    }
                }
            }
        }

        if (m_is_file_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog("OpenFileDialog", "Choose File", nullptr, "",
                                                    "");
        }

        if (ImGuiFileDialog::Instance()->Display("OpenFileDialog")) {
            m_is_file_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                auto file_result = Toolbox::is_regular_file(path);
                if (!file_result) {
                    return;
                }

                if (!file_result.value()) {
                    return;
                }
                m_load_path = path.parent_path();

                if (path.extension() == ".szs" || path.extension() == ".arc") {
                    auto scene_window = make_referable<SceneWindow>();
                    if (!scene_window->loadData(path)) {
                        return;
                    }
                    scene_window->open();
                    m_windows.push_back(scene_window);
                }
            }

            ImGuiFileDialog::Instance()->Close();
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

        if (m_options_open) {
            auto settings_window_it =
                std::find_if(m_windows.begin(), m_windows.end(),
                             [](auto window) { return window->name() == "Application Settings"; });
            if (settings_window_it != m_windows.end()) {
                (*settings_window_it)->open();
            }
        }
    }

    void GUIApplication::renderWindows(f32 delta_time) {
        // Render viewer context
        for (auto &window : m_windows) {
            if (!m_dockspace_built && !m_docked_map[window->getUUID()]) {
                std::string window_name =
                    std::format("{}###{}", window->title(), window->getUUID());
                ImGui::DockBuilderDockWindow(window_name.c_str(), m_dockspace_id);
                m_docked_map[window->getUUID()] = true;
            }
        }

        for (auto &window : m_windows) {
            window->render(delta_time);
        }
    }

    bool GUIApplication::postRender(f32 delta_time) {
        // Try to make sure we return an error if anything's fucky
        if (m_render_window == nullptr || glfwWindowShouldClose(m_render_window))
            return false;

        // Update viewer context
        for (auto &window : m_windows) {
            if (!window->postUpdate(delta_time)) {
                return false;
            }
        }

        Input::PostUpdateInputState();

        return true;
    }

    bool GUIApplication::determineEnvironmentConflicts() {
        auto service_result = Platform::IsServiceRunning("NahimicService");
        if (!service_result) {
            LogError(service_result.error());
            return true;
        }

        if (service_result.value()) {
            TOOLBOX_WARN("Found Nahimic service running on this system, this could cause "
                         "crashes to occur on window resize!");
            return false;
        }

        return true;
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
