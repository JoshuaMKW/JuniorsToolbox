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
#include "gui/event/event.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/window.hpp"
#include "gui/project/asset.hpp"

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

        [[nodiscard]] std::string context() const override {
            return m_project_root.string();
        }

        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override {
            return {};
        }

        [[nodiscard]] bool onLoadData(const std::filesystem::path& path) override {
            m_project_root = path;
            m_file_system_model = make_referable<FileSystemModel>();
            m_file_system_model->initialize();
            m_file_system_model->setRoot(m_project_root);
            m_tree_proxy.setSourceModel(m_file_system_model);
            m_tree_proxy.setDirsOnly(true);
            m_view_proxy.setSourceModel(m_file_system_model);
            m_view_proxy.setSortRole(FileSystemModelSortRole::SORT_ROLE_NAME);
            m_view_index = m_view_proxy.getIndex(0, 0);
            return true;
        }
        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override {
            return true;
        }

        void onAttach() override;
        void onDetach() override;
        void onImGuiUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;

    private:
        fs_path m_project_root;

        // TODO: Have filesystem model.
        FileSystemModelSortFilterProxy m_tree_proxy;
        FileSystemModelSortFilterProxy m_view_proxy;
        RefPtr<FileSystemModel> m_file_system_model;

        std::vector<ModelIndex> m_selected_indices;
        std::vector<ProjectAsset> m_view_assets;
        ModelIndex m_view_index;

        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;
    };

}  // namespace Toolbox::UI