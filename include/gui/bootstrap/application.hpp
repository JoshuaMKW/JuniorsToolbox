#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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

#include "gui/appmain/application.hpp"
#include "gui/bootstrap/project/model.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/window.hpp"

#include "core/clipboard.hpp"
#include "dolphin/process.hpp"

#include "gui/font.hpp"
#include "project/project.hpp"
#include "resource/resource.hpp"

#include "scene/layout.hpp"
#include <nfd.h>

namespace Toolbox::UI {

    enum class ProjectAgeCategory {
        AGE_NONE,
        AGE_DAY,
        AGE_WEEK,
        AGE_MONTH,
        AGE_YEAR,
        AGE_LONG_AGO,
    };

    class BootStrapApplication final : public CoreApplication {
    public:
        struct BootStrapArguments {
            struct WindowArguments {
                std::string m_window_type;
                std::optional<fs_path> m_load_path;
                std::vector<std::string> m_vargs;
            };

            std::optional<std::string> m_settings_profile;
            std::vector<WindowArguments> m_windows;
        };

        static BootStrapApplication &instance();
        bool hasResults() const { return m_results_ready; }
        BootStrapArguments getResults() const { return m_results; }

    protected:
        BootStrapApplication();

    public:
        BootStrapApplication(const BootStrapApplication &)            = delete;
        BootStrapApplication(BootStrapApplication &&)                 = delete;
        BootStrapApplication &operator=(const BootStrapApplication &) = delete;
        BootStrapApplication &operator=(BootStrapApplication &&)      = delete;

        virtual ~BootStrapApplication() {}

        void onInit(int argc, const char **argv) override;
        void onUpdate(TimeStep delta_time) override;
        void onExit() override;

        void onEvent(RefPtr<BaseEvent> ev) override;

        const fs_path &getAppDataPath() const;

        void showSuccessModal(ImWindow *parent, const std::string &title,
                              const std::string &message);
        void showErrorModal(ImWindow *parent, const std::string &title, const std::string &message);

        // bool startDragAction(Platform::LowWindow source, RefPtr<DragAction> action);

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
        void renderBody();
        void renderLandingPage();
        void renderLocalConfigPage();
        void renderRepositoryConfigPage();
        bool renderProjectAgeGroup(ProjectAgeCategory category);
        void renderProjectRow(const ModelIndex &index);
        void finalizeFrame();

        bool determineEnvironmentConflicts();

    private:
        UUID64 m_uuid;

        std::mutex m_event_mutex;

        std::filesystem::path m_load_path = std::filesystem::current_path();
        std::filesystem::path m_save_path = std::filesystem::current_path();

        GLFWwindow *m_render_window;

        ResourceManager m_resource_manager;

        // ScopePtr<IDragDropTargetDelegate> m_drag_drop_target_delegate;
        // ScopePtr<IDragDropSourceDelegate> m_drag_drop_source_delegate;
        // ImGuiViewport *m_drag_drop_viewport = nullptr;
        // bool m_await_drag_drop_destroy      = false;

        // RefPtr<DragAction> m_pending_drag_action;
        // Platform::LowWindow m_pending_drag_window;

        bool m_is_file_dialog_open = false;
        bool m_is_dir_dialog_open  = false;

        std::thread m_thread_templates_init;

        RefPtr<ProjectModel> m_project_model;
        RefPtr<ProjectModelSortFilterProxy> m_sort_filter_proxy;
        std::array<char, 256> m_search_str;
        std::array<char, 256> m_repo_input_str;
        std::array<char, 256> m_repo_path_str;
        std::array<char, 256> m_github_username_str;
        std::array<char, 256> m_github_passkey_str;

        bool m_is_git_busy = false;
        std::string m_git_clone_msg;
        std::string m_git_clone_job;
        float m_git_clone_progress = 0.0f;
        int m_git_clone_job_index  = 0;
        int m_git_clone_job_max    = 0;
        std::string m_project_error_msg;

        enum class RenderPage {
            LANDING,
            LOCAL_CONFIG,
            REPO_CONFIG,
        } m_render_page = RenderPage::LANDING;

        std::optional<fs_path> m_project_path;
        std::optional<fs_path> m_scene_path;
        BootStrapArguments m_results;
        bool m_results_ready = false;

        std::unordered_map<ProjectAgeCategory, bool> m_category_closed_map;

        // std::vector<SuccessModal> m_success_modal_queue;
        // std::vector<FailureModal> m_error_modal_queue;
    };

}  // namespace Toolbox::UI
