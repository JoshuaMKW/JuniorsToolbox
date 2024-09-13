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

#include "scene/layout.hpp"
#include <nfd.h>

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

        std::filesystem::path getProjectRoot() const { return m_project_root; }

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

        std::filesystem::path m_project_root = std::filesystem::current_path();
        std::filesystem::path m_load_path    = std::filesystem::current_path();
        std::filesystem::path m_save_path    = std::filesystem::current_path();

        ScopePtr<Scene::SceneLayoutManager> m_scene_layout_manager;

        GLFWwindow *m_render_window;
        std::vector<RefPtr<ImWindow>> m_windows;

        std::unordered_map<UUID64, bool> m_docked_map;
        ImGuiID m_dockspace_id;
        bool m_dockspace_built;

        bool m_opening_options_window = false;
        bool m_is_file_dialog_open    = false;
        bool m_is_dir_dialog_open     = false;

        std::thread m_thread_templates_init;
        DolphinCommunicator m_dolphin_communicator;
        Game::TaskCommunicator m_task_communicator;
    };

    class FileDialogFilter {
    public:
        FileDialogFilter() = default;
        ~FileDialogFilter() = default;

        void addFilter(const std::string &label, const std::string &csv_filters);
        bool hasFilter(const std::string &label) const;
        int numFilters() const { return m_filters.size(); };
        void writeFiltersU8(nfdu8filteritem_t *&out) const;

    private:
        std::vector<std::pair<std::string, std::string>> m_filters;
    };

    class FileDialog {
    public:
        FileDialog() = default;
        ~FileDialog() {
            if (m_thread_initialized) {
                m_thread.join();
            }
        }

        static FileDialog *instance() {
            static FileDialog _instance;
            return &_instance;
        }
        void openDialog(std::filesystem::path starting_path, GLFWwindow *parent_window,
                        bool is_directory = false,
                        std::optional<FileDialogFilter>
                            maybe_filters = std::nullopt);
        bool isAlreadyOpen() { return m_thread_running; }
        bool isDone() { return !m_thread_running && !m_closed && m_thread_initialized; }
        bool isOk() { return m_result == NFD_OKAY; }
        std::filesystem::path getFilenameResult() { return m_selected_path; }
        void close() { m_closed = true; }

    private:
        std::string m_starting_path;
        std::vector<std::pair<std::string, std::string>> m_filters;

        // The result of the last dialog box.
        nfdu8char_t *m_selected_path;
        nfdresult_t m_result;
        // The thread that we run the dialog in. If
        // m_thread_initialized is true, this should be an initialized
        // thread object.
        std::thread m_thread;
        // For tracking whether we've launched any threads so far. If
        // so, we'll join them before launching another.
        bool m_thread_initialized = false;
        // For tracking whether the thread is still running a dialog
        // box.
        bool m_thread_running = false;
        // For checking whether the user has called Close. If so, stop
        // returning true for isDone().
        bool m_closed = false;
    };

}  // namespace Toolbox
