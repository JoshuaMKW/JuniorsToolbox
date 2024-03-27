#pragma once

#include "core/application/application.hpp"

#include <glad/glad.h>

// Included first to let definitions take over
#include "gui/imgui_ext.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDE_NONE

#include "gui/scene/window.hpp"
#include "gui/window.hpp"

#include "core/clipboard.hpp"
#include "dolphin/process.hpp"

using namespace Toolbox::Dolphin;

namespace Toolbox {

    class GUIApplication : public CoreApplication {
    protected:
        GUIApplication();

    public:
        virtual ~GUIApplication() {}

        static GUIApplication &instance() {
            static GUIApplication _inst;
            return _inst;
        }

        virtual void onInit(int argc, const char **argv) override;
        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onExit() override;

        void addWindow(RefPtr<ImWindow> window) { 
          addLayer(window);
          m_windows.push_back(window);
        }

        void removeWindow(RefPtr<ImWindow> window) {
          removeLayer(window);
          std::erase(m_windows, window);
        }

        TypedDataClipboard<SelectionNodeInfo<Object::ISceneObject>> &getSceneObjectClipboard() {
            return m_hierarchy_clipboard;
        }

        TypedDataClipboard<SelectionNodeInfo<Rail::Rail>> &getSceneRailClipboard() {
            return m_rail_clipboard;
        }

        TypedDataClipboard<SelectionNodeInfo<Rail::RailNode>> &getSceneRailNodeClipboard() {
            return m_rail_node_clipboard;
        }

        DolphinCommunicator &getDolphinCommunicator() { return m_dolphin_communicator; }

        std::filesystem::path getProjectRoot() const {
            return m_project_root;
        }

        ImVec2 windowScreenPos() {
            int x = 0, y = 0;
            if (m_render_window) {
                glfwGetWindowPos(m_render_window, &x, &y);
            }
            return {static_cast<f32>(x), static_cast<f32>(y)};
        }

        ImVec2 windowSize() {
            int x = 0, y = 0;
            if (m_render_window) {
                glfwGetWindowSize(m_render_window, &x, &y);
            }
            return {static_cast<f32>(x), static_cast<f32>(y)};
        }

    protected:
        void render(TimeStep delta_time);
        void renderMenuBar();

        bool determineEnvironmentConflicts();

    private:
        TypedDataClipboard<SelectionNodeInfo<Object::ISceneObject>> m_hierarchy_clipboard;
        TypedDataClipboard<SelectionNodeInfo<Rail::Rail>> m_rail_clipboard;
        TypedDataClipboard<SelectionNodeInfo<Rail::RailNode>> m_rail_node_clipboard;

        std::filesystem::path m_project_root = std::filesystem::current_path();
        std::filesystem::path m_load_path = std::filesystem::current_path();
        std::filesystem::path m_save_path = std::filesystem::current_path();

        GLFWwindow *m_render_window;
        std::vector<RefPtr<ImWindow>> m_windows;

        std::unordered_map<UUID64, bool> m_docked_map;
        ImGuiID m_dockspace_id;
        bool m_dockspace_built;

        bool m_options_open        = false;
        bool m_is_file_dialog_open = false;
        bool m_is_dir_dialog_open  = false;

        std::thread m_thread_templates_init;
        DolphinCommunicator m_dolphin_communicator;
    };

}  // namespace Toolbox