#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <queue>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "bmg/bmg.hpp"
#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/context_menu.hpp"
#include "gui/event/event.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/appmain/project/asset.hpp"
#include "gui/appmain/project/rarc_processor.hpp"
#include "gui/selection.hpp"
#include "gui/window.hpp"

#include "model/fsmodel.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class BMGEditorWindow final : public ImWindow {
    public:
        BMGEditorWindow(const std::string &name);
        ~BMGEditorWindow() = default;

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderMessagesList();

        void renderMessageEditorGroup();
        void renderMessageEditorMetadata();
        void renderMessageEditorTextbox();
        void renderMessageEditorPreview();

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
                {800, 600}
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

    private:
        RefPtr<FileSystemModelSortFilterProxy> m_tree_proxy;
        RefPtr<FileSystemModelSortFilterProxy> m_view_proxy;
        RefPtr<FileSystemModel> m_file_system_model;

        ModelSelectionManager m_selection_mgr;

        ImagePainter m_icon_painter;

        ContextMenu<ModelIndex> m_list_context_menu;
        ContextMenu<ModelIndex> m_text_context_menu;

        std::vector<ModelIndex> m_cut_indices;

        bool m_is_renaming = false;
        char m_rename_buffer[128];
        bool m_is_valid_name = true;

        bool m_did_drag_drop = false;

        ImVec2 m_last_reg_mouse_pos;

        std::array<char, 128> m_search_buf;
        std::string m_search_str;
    };

}  // namespace Toolbox::UI