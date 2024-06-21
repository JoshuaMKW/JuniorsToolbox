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
        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;
    };

}  // namespace Toolbox::UI