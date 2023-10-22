#include "gui/application.hpp"
#include "gui/input.hpp"
#include "gui/util.hpp"

#include <J3D/J3DUniformBufferObject.hpp>

#include <iostream>
#include <string>

namespace Toolbox::UI {

    void MainApplication::run() {
        Clock::time_point lastFrameTime, thisFrameTime;

        while (true) {
            lastFrameTime = thisFrameTime;
            thisFrameTime = Util::GetTime();

            if (!execute(Util::GetDeltaTime(lastFrameTime, thisFrameTime)))
                break;
        }
    }

    void DealWithGLErrors(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
        std::cout << "GL CALLBACK: " << message << std::endl;
    }

    MainApplication::MainApplication() {
        m_dockspace_id  = ImGuiID();
        m_render_window = nullptr;
        m_windows       = {};
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
        gladLoadGL(glfwGetProcAddress);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glfwSwapInterval(1);

        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(DealWithGLErrors, nullptr);

        // Initialize imgui
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(m_render_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        // glEnable(GL_MULTISAMPLE);

        // Create viewer context
        m_windows.push_back(std::make_shared<SceneWindow>());
        /*m_windows.push_back(std::make_shared<SceneWindow>());
        m_windows.push_back(std::make_shared<SceneWindow>());
        m_windows.push_back(std::make_shared<SceneWindow>());*/

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

    bool MainApplication::execute(float delta_time) {
        // Try to make sure we return an error if anything's fucky
        if (m_windows.empty() || m_render_window == nullptr ||
            glfwWindowShouldClose(m_render_window))
            return false;

        // Update viewer context
        for (auto &window : m_windows) {
            window->update(delta_time);
        }

        // Begin actual rendering
        glfwMakeContextCurrent(m_render_window);
        glfwPollEvents();

        Input::UpdateInputState();

        // The context renders both the ImGui elements and the background elements.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update buffer size
        int width, height;
        glfwGetFramebufferSize(m_render_window, &width, &height);
        glViewport(0, 0, width, height);

        // Clear buffers
        glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

        ImGui::Begin("MainDockSpace", nullptr, window_flags);
        {
            m_dockspace_id = ImGui::GetID("MainDockSpace");

            bool dockspace_built = ImGui::DockBuilderGetNode(m_dockspace_id);
            if (!dockspace_built) {
                ImGui::DockBuilderRemoveNode(m_dockspace_id);
                ImGui::DockBuilderAddNode(m_dockspace_id, ImGuiDockNodeFlags_None);
            }

            // Render viewer context
            for (auto &window : m_windows) {
                if (!dockspace_built && !m_docked_map[window->title()]) {
                    ImGui::DockBuilderDockWindow(window->title().c_str(), m_dockspace_id);
                    m_docked_map[window->title()] = true;
                }
            }

            for (auto &window : m_windows) {
                window->render(delta_time);
            }

            if (!dockspace_built)
                ImGui::DockBuilderFinish(m_dockspace_id);

            ImGui::DockSpace(m_dockspace_id, {}, ImGuiDockNodeFlags_PassthruCentralNode);

        }
        ImGui::End();

        // Render imgui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(m_render_window);

        return true;
    }

};  // namespace Toolbox::UI