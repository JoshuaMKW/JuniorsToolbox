#include "gui/application.hpp"
#include "gui/IconsForkAwesome.h"
#include "gui/font.hpp"
#include "gui/input.hpp"
#include "gui/settings/window.hpp"
#include "gui/themes.hpp"
#include "gui/util.hpp"

#include <J3D/Material/J3DUniformBufferObject.hpp>

#include <iostream>
#include <string>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <ImGuiFileDialog.h>
#include <J3D/J3DModelLoader.hpp>
#include <bstream.h>
#include <gui/logging/window.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <gui/logging/errors.hpp>

// void ImGuiSetupTheme(bool, float);

namespace Toolbox::UI {

    int MainApplication::run() {
        Clock::time_point lastFrameTime, thisFrameTime;

        while (true) {
            lastFrameTime = thisFrameTime;
            thisFrameTime = Util::GetTime();

            f32 delta_time = Util::GetDeltaTime(lastFrameTime, thisFrameTime);

            if (!execute(delta_time))
                break;

            render(delta_time);
        }

        return 0;
    }

    void DealWithGLErrors(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
        Log::AppLogger &logger = Log::AppLogger::instance();
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        case GL_DEBUG_SEVERITY_LOW:
            // logger.info(std::format("(OpenGL) {}", message));
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            logger.warn(std::format("(OpenGL) {}", message));
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            logger.error(std::format("(OpenGL) {}", message));
            break;
        }
    }

    MainApplication::MainApplication() {
        m_dockspace_built = false;
        m_dockspace_id    = ImGuiID();
        m_render_window   = nullptr;
        m_windows         = {};
    }

    bool MainApplication::setup() {
        // Initialize GLFW
        if (!glfwInit())
            return false;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_DEPTH_BITS, 32);
        glfwWindowHint(GLFW_SAMPLES, 4);

        m_render_window = glfwCreateWindow(1280, 720, "Junior's Toolbox", nullptr, nullptr);
        if (m_render_window == nullptr) {
            glfwTerminate();
            return false;
        }

        glfwSetKeyCallback(m_render_window, Input::GLFWKeyCallback);
        glfwSetCursorPosCallback(m_render_window, Input::GLFWMousePositionCallback);
        glfwSetMouseButtonCallback(m_render_window, Input::GLFWMouseButtonCallback);
        glfwSetScrollCallback(m_render_window, Input::GLFWMouseScrollCallback);

        glfwMakeContextCurrent(m_render_window);
        gladLoadGL();
        // gladLoadGL(glfwGetProcAddress);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glfwSwapInterval(1);

        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(DealWithGLErrors, nullptr);

        // Initialize imgui
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(m_render_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        auto &font_manager = FontManager::instance();
        font_manager.initialize();
        font_manager.setCurrentFont("NotoSansJP-Regular", 16.0f);

        // glEnable(GL_MULTISAMPLE);

        {
            auto result = Object::TemplateFactory::initialize();
            if (!result) {
                logFSError(result.error());
            }
        }

        {
            auto result = ThemeManager::instance().initialize();
            if (!result) {
                logFSError(result.error());
            }
        }

        auto settings_window = std::make_shared<SettingsWindow>();
        m_windows.push_back(settings_window);

        auto logging_window = std::make_shared<LoggingWindow>();
        logging_window->open();
        m_windows.push_back(logging_window);

        return true;
    }

    bool MainApplication::teardown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_render_window);
        glfwTerminate();

        J3DUniformBufferObject::DestroyUBO();

        m_windows.clear();

        return true;
    }

    bool MainApplication::execute(f32 delta_time) {
        // Try to make sure we return an error if anything's fucky
        if (m_render_window == nullptr || glfwWindowShouldClose(m_render_window))
            return false;

        // Update viewer context
        for (auto &window : m_windows) {
            if (!window->update(delta_time)) {
                return false;
            }
        }

        return true;
    }

    void MainApplication::render(f32 delta_time) {  // Begin actual rendering
        glfwMakeContextCurrent(m_render_window);
        glfwPollEvents();

        Input::UpdateInputState();

        // The context renders both the ImGui elements and the background elements.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto &font_manager = FontManager::instance();
        ImFont *main_font  = font_manager.getCurrentFont();

        // ImGui::PushFont(main_font);

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        m_dockspace_id = ImGui::DockSpaceOverViewport(viewport);

        // Update buffer size
        int width, height;
        glfwGetFramebufferSize(m_render_window, &width, &height);
        glViewport(0, 0, width, height);

        // Clear buffers
        glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

        m_dockspace_built = ImGui::DockBuilderGetNode(m_dockspace_id);
        if (!m_dockspace_built) {
            ImGui::DockBuilderRemoveNode(m_dockspace_id);
            ImGui::DockBuilderAddNode(m_dockspace_id, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderFinish(m_dockspace_id);
        }

        renderMenuBar();
        renderWindows(delta_time);

        // ImGui::PopFont();

        // Render imgui
        ImGui::Render();

        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow *backup_window = glfwGetCurrentContext();
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
            glfwMakeContextCurrent(backup_window);
            m_render_window = backup_window;
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(m_render_window);
    }

    void MainApplication::renderMenuBar() {
        m_options_open = false;
        ImGui::BeginMainMenuBar();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_FK_FOLDER_OPEN " Open...")) {
                m_is_file_dialog_open = true;
            }
            if (ImGui::MenuItem(ICON_FK_FOLDER_OPEN " Open Folder...")) {
                m_is_dir_dialog_open = true;
            }
            if (ImGui::MenuItem(ICON_FK_FLOPPY_O " Save...")) {
                // Save Scene
            }

            ImGui::Separator();
            ImGui::MenuItem(ICON_FK_WINDOW_CLOSE " Close");

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
            ImGuiFileDialog::Instance()->OpenDialog("OpenDirDialog", "Choose Directory", nullptr,
                                                    "", "");
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

                if (path.filename() == "scene") {
                    auto scene_window = std::make_shared<SceneWindow>();

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

                if (path.extension() == ".szs" || path.extension() == ".arc") {
                    auto scene_window = std::make_shared<SceneWindow>();
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

    void MainApplication::renderWindows(f32 delta_time) {
        // Render viewer context
        for (auto &window : m_windows) {
            if (!m_dockspace_built && !m_docked_map[window->title()]) {
                ImGui::DockBuilderDockWindow(window->title().c_str(), m_dockspace_id);
                m_docked_map[window->title()] = true;
            }
        }

        for (auto &window : m_windows) {
            window->render(delta_time);
        }
    }
}  // namespace Toolbox::UI

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
