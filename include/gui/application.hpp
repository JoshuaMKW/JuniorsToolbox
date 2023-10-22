#pragma once

#include <deps/glad/gl.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "gui/scene/window.hpp"
#include "gui/window.hpp"

namespace Toolbox::UI {

    class MainApplication {
    public:
        MainApplication();
        virtual ~MainApplication() {}

        bool setup();
        void run();
        bool teardown();

    protected:
        bool execute(float delta_time);

    private:
        GLFWwindow *m_render_window;
        std::vector<std::shared_ptr<IWindow>> m_windows;
        std::unordered_map<std::string, bool> m_docked_map;
        ImGuiID m_dockspace_id;
    };

}  // namespace Toolbox::UI