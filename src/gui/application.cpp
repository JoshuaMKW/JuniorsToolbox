#include "gui/application.hpp"
#include "gui/input.hpp"
#include "gui/util.hpp"

#include <J3D/J3DUniformBufferObject.hpp>

#include <iostream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <ImGuiFileDialog.h>
#include <J3D/J3DModelLoader.hpp>
#include <bstream.h>
#include <gui/IconsForkAwesome.h>
#include <imgui.h>
#include <imgui_internal.h>

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
        //std::cout << "GL CALLBACK: " << message << std::endl;
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
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(m_render_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        if (std::filesystem::exists(
                (std::filesystem::current_path() / "res" / "NotoSansJP-Regular.otf"))) {
            io.Fonts->AddFontFromFileTTF(
                (std::filesystem::current_path() / "res" / "NotoSansJP-Regular.otf")
                    .string()
                    .c_str(),
                16.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
        }

        if (std::filesystem::exists(
                (std::filesystem::current_path() / "res" / "forkawesome.ttf"))) {
            static const ImWchar icons_ranges[] = {ICON_MIN_FK, ICON_MAX_16_FK, 0};
            ImFontConfig icons_config;
            icons_config.MergeMode        = true;
            icons_config.PixelSnapH       = true;
            icons_config.GlyphMinAdvanceX = 16.0f;
            io.Fonts->AddFontFromFileTTF(
                (std::filesystem::current_path() / "res" / "forkawesome.ttf").string().c_str(),
                icons_config.GlyphMinAdvanceX, &icons_config, icons_ranges);
        }

        // glEnable(GL_MULTISAMPLE);

        auto result = Object::TemplateFactory::initialize();
        if (!result) {
            std::cout << result.error().m_message;
            std::cout << result.error().m_stacktrace;
            return false;
        }

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
        }

        renderMenuBar();
        renderWindows(delta_time);

        if (!m_dockspace_built)
            ImGui::DockBuilderFinish(m_dockspace_id);

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
            ImGui::OpenPopup("Options");
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