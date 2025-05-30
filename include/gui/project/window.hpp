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
#include "gui/project/asset.hpp"
#include "gui/window.hpp"

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

        void buildContextMenu();
        MimeData buildFolderViewMimeData() const;

        // Selection actions
        void actionDeleteIndexes(std::vector<ModelIndex> &indices);
        void actionOpenIndexes(const std::vector<ModelIndex> &indices);
        void actionRenameIndex(const ModelIndex &index);
        void actionPasteIntoIndex(const ModelIndex &index, const std::vector<fs_path> &data);
        void actionCopyIndexes(const std::vector<ModelIndex> &indices);

        void actionSelectIndex(const ModelIndex &view_index, const ModelIndex &child_index,
                                  bool is_selected);
        void actionClearRequestExcIndex(const ModelIndex &view_index,
                                        const ModelIndex &child_index, bool is_left_button);

        bool actionOpenScene(const ModelIndex &index);
        bool actionOpenPad(const ModelIndex &index);

        bool isPathForScene(const ModelIndex &index) const;

    private:

        bool isValidName(const std::string &name, const std::vector<ModelIndex> &selected_indices) const;
        fs_path m_project_root;

        // TODO: Have filesystem model.
        FileSystemModelSortFilterProxy m_tree_proxy;
        FileSystemModelSortFilterProxy m_view_proxy;
        RefPtr<FileSystemModel> m_file_system_model;

        ModelIndex m_last_selected_index;
        std::vector<ModelIndex> m_selected_indices;
        std::vector<ProjectAsset> m_view_assets;
        ModelIndex m_view_index;

        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;

        ContextMenu<ModelIndex> m_folder_view_context_menu;
        ContextMenu<ModelIndex> m_tree_view_context_menu;
        std::vector<ModelIndex> m_selected_indices_ctx;

        bool m_is_renaming = false;
        char m_rename_buffer[128];
        bool m_is_valid_name = true;

        bool m_delete_without_request = false;
        bool m_delete_requested       = false;

        bool m_did_drag_drop = false;

        ImVec2 m_last_reg_mouse_pos;
    };

}  // namespace Toolbox::UI