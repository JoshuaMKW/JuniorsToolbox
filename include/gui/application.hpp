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

#include "gui/dolphin/overlay.hpp"
#include "gui/scene/window.hpp"
#include "gui/window.hpp"

#include "core/clipboard.hpp"
#include "dolphin/process.hpp"

#include "project/project.hpp"

#include "scene/layout.hpp"

using namespace Toolbox::Dolphin;

namespace Toolbox {

    class GUIApplication final : public CoreApplication {
    public:
        static GUIApplication &instance();

    protected:
        GUIApplication();

    public:
        GUIApplication(const GUIApplication &)            = delete;
        GUIApplication(GUIApplication &&)                 = delete;
        GUIApplication &operator=(const GUIApplication &) = delete;
        GUIApplication &operator=(GUIApplication &&)      = delete;

        virtual ~GUIApplication() {}

        virtual void onInit(int argc, const char **argv) override;
        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onExit() override;

        Toolbox::fs_path getResourcePath(const Toolbox::fs_path &path) const &;
        Toolbox::fs_path getResourcePath(Toolbox::fs_path &&path) const &&;

        void addWindow(RefPtr<ImWindow> window) {
            addLayer(window);
            m_windows.push_back(window);
        }

        void removeWindow(RefPtr<ImWindow> window) {
            removeLayer(window);
            std::erase(m_windows, window);
        }

        const std::vector<RefPtr<ImWindow>> &getWindows() const & { return m_windows; }

        // Find a window by its UUID
        RefPtr<ImWindow> findWindow(UUID64 uuid);
        RefPtr<ImWindow> findWindow(const std::string &title, const std::string &context);

        std::vector<RefPtr<ImWindow>> findWindows(const std::string &title);

        template <typename T, bool strict = false, typename... Args>
        RefPtr<T> createWindow(const std::string &name, Args &&...args) {
            std::vector<RefPtr<ImWindow>> reuse_candidates = findWindows(name);

            RefPtr<T> window = make_referable<T>(name, std::forward<Args>(args)...);

            // Reuse window if it is closed and not set to destroy on close
            for (RefPtr<ImWindow> candidate : reuse_candidates) {
                if (candidate->isClosed() && !candidate->destroyOnClose()) {
                    candidate->onAttach();
                    candidate->open();
                    return ref_cast<T>(candidate);
                }

                if constexpr (!strict) {
                    if (candidate->isHidden()) {
                        candidate->show();
                        candidate->focus();
                    }

                    if (candidate->isOpen()) {
                        candidate->focus();
                    }

                    return ref_cast<T>(candidate);
                }
            }

            window->open();
            addWindow(window);
            return window;
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
        Game::TaskCommunicator &getTaskCommunicator() { return m_task_communicator; }

        ProjectManager &getProjectManager() { return m_project_manager; }
        const ProjectManager &getProjectManager() const { return m_project_manager; }
        

        void registerDolphinOverlay(UUID64 scene_uuid, const std::string &name,
                                    SceneWindow::render_layer_cb cb);
        void deregisterDolphinOverlay(UUID64 scene_uuid, const std::string &name);

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
        void initializeIcon();

        void render(TimeStep delta_time);
        void renderMenuBar();
        void finalizeFrame();

        bool determineEnvironmentConflicts();

        void gcClosedWindows();

    private:
        TypedDataClipboard<SelectionNodeInfo<Object::ISceneObject>> m_hierarchy_clipboard;
        TypedDataClipboard<SelectionNodeInfo<Rail::Rail>> m_rail_clipboard;
        TypedDataClipboard<SelectionNodeInfo<Rail::RailNode>> m_rail_node_clipboard;

        ProjectManager m_project_manager;

        std::filesystem::path m_load_path    = std::filesystem::current_path();
        std::filesystem::path m_save_path    = std::filesystem::current_path();

        GLFWwindow *m_render_window;
        std::vector<RefPtr<ImWindow>> m_windows;

        std::unordered_map<UUID64, bool> m_docked_map;
        ImGuiID m_dockspace_id;
        bool m_dockspace_built;

        bool m_opening_options_window        = false;
        bool m_is_file_dialog_open = false;
        bool m_is_dir_dialog_open  = false;

        std::thread m_thread_templates_init;
        DolphinCommunicator m_dolphin_communicator;
        Game::TaskCommunicator m_task_communicator;
    };

}  // namespace Toolbox