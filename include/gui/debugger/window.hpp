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

#include <imgui.h>

namespace Toolbox::UI {

    class DebuggerWindow final : public ImWindow {
    public:
        DebuggerWindow(const std::string &name);
        ~DebuggerWindow() = default;

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderMemoryView();
        void renderMemoryWatchList();

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
            if (m_attached_scene_uuid == 0) {
                return "MRAM";
            }

            RefPtr<ImWindow> scene_window =
                GUIApplication::instance().findWindow(m_attached_scene_uuid);
            if (!scene_window) {
                return "MRAM";
            }

            return std::format("MRAM ({})", scene_window->context());
        }

        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override { return true; }

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
        UUID64 m_attached_scene_uuid = 0;

        u32 m_base_address = 0;
        std::vector<u32> m_address_search_history = {};

        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;

        ContextMenu<ModelIndex> m_memory_view_context_menu;
        ContextMenu<ModelIndex> m_watch_view_context_menu;
    };

}  // namespace Toolbox::UI