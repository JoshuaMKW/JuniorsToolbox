#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/context_menu.hpp"
#include "gui/event/event.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/appmain/project/asset.hpp"
#include "gui/appmain/project/rarc_processor.hpp"
#include "gui/selection.hpp"
#include "gui/window.hpp"
#include "project/config.hpp"

#include "model/fsmodel.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class ProjectViewWindow final : public ImWindow {
    public:
        ProjectViewWindow(const std::string &name);
        ~ProjectViewWindow() = default;

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderProjectTreeView();
        void renderProjectFolderView();
        void renderProjectFolderButton();
        void renderProjectFileButton();

        bool isViewedAncestor(const ModelIndex &index);
        void renderFolderTree(const ModelIndex &index);
        void initFolderAssets(const ModelIndex &index);

        //void startCopyProc(const ModelIndex &from, const ModelIndex &to);
        void evInsertProc_(MimeData data);
        void optionFolderViewDeleteProc_();
        void optionTreeViewDeleteProc_();
        void optionPinnedViewDeleteProc_();
        void optionFolderViewPasteProc_();

        void setViewIndex(const ModelIndex &index, bool replace_present_history);
        bool redoViewHistory();
        bool undoViewHistory();

    public:
        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_MenuBar;
        }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow           = ImGui::GetCurrentWindow();
            m_window_class.ClassId               = (ImGuiID)getUUID();
            m_window_class.ParentViewportId      = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed = true;
            m_window_class.DockingAlwaysTabBar   = false;
            return nullptr;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {600, 400}
            };
        }

        [[nodiscard]] std::string context() const override { return m_project_root.string(); }

        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override;

        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override {
            return true;
        }

        void onAttach() override;
        void onDetach() override;
        void onImGuiUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;
        void onEvent(RefPtr<BaseEvent> ev) override;

        void buildContextMenu();

        // Selection actions
        void actionOpenIndexes(const std::vector<ModelIndex> &indices);
        void actionCutIndexes(const std::vector<ModelIndex> &indices);
        void actionRenameIndex(const ModelIndex &index);

        bool actionOpenScene(const ModelIndex &index);
        bool actionOpenPad(const ModelIndex &index);

        bool isPathForScene(const ModelIndex &index) const;

    private:
        bool isValidName(const std::string &name,
                         const std::vector<ModelIndex> &selected_indices) const;

        fs_path m_project_root;

        RefPtr<FileSystemModelSortFilterProxy> m_tree_proxy;
        RefPtr<FileSystemModelSortFilterProxy> m_view_proxy;
        RefPtr<FileSystemModel> m_file_system_model;

        ModelIndex m_last_selected_index;
        ModelSelectionManager m_folder_selection_mgr;
        ModelSelectionManager m_tree_selection_mgr;
        ModelSelectionManager m_pinned_selection_mgr;
        std::vector<ProjectAsset> m_view_assets;
        ModelIndex m_view_index;
        std::vector<ModelIndex> m_pinned_folders;

        RarcProcessor m_rarc_processor;
        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;

        ContextMenu<ModelIndex> m_folder_view_context_menu;
        ContextMenu<ModelIndex> m_tree_view_context_menu;
        ContextMenu<ModelIndex> m_pinned_view_context_menu;

        std::vector<ModelIndex> m_cut_indices;

        bool m_is_renaming = false;
        char m_rename_buffer[128];
        bool m_is_valid_name = true;

        bool m_delete_without_request = false;
        bool m_folder_view_delete_requested = false;
        bool m_tree_view_delete_requested = false;

        bool m_did_drag_drop = false;

        ImVec2 m_last_reg_mouse_pos;

        std::vector<ModelIndex> m_view_history_stack;
        size_t m_view_history_index = 0;

        std::array<char, 128> m_search_buf;
        std::string m_search_str;

        std::mutex m_async_io_mutex;

        ProjectConfig m_project_config;
    };

}  // namespace Toolbox::UI