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
        int run();
        bool teardown();

    protected:
        bool execute(f32 delta_time);
        void render(f32 delta_time);
        void renderMenuBar();
        void renderWindows(f32 delta_time);

    private:
        GLFWwindow *m_render_window;
        std::vector<std::shared_ptr<IWindow>> m_windows;

        std::unordered_map<std::string, bool> m_docked_map;
        ImGuiID m_dockspace_id;
        bool m_dockspace_built;

        bool m_options_open = false;
        bool m_is_file_dialog_open = false;
        bool m_is_dir_dialog_open = false;
    };

}  // namespace Toolbox::UI