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

    class NewItemWindow final : public ImWindow {
    public:
        NewItemWindow(const std::string &name);
        ~NewItemWindow() = default;

        using window_constructor = std::function<RefPtr<ImWindow>()>;

        struct ItemInfo {
            std::string m_name;
            std::string m_extension;
            std::string m_description;
            RefPtr<const ImageHandle> m_icon;
            window_constructor m_win_factory;
        };

    protected:
        void onRenderBody(TimeStep delta_time) override;

        void renderItemRow(const ItemInfo &info, bool selected);
        void renderItemDescription(const std::string &description);
        void renderControlPanel();

    public:
        ImGuiWindowFlags flags() const override { return ImWindow::flags(); }

        const ImGuiWindowClass *windowClass() const override {
            ImGuiWindow *currentWindow           = ImGui::GetCurrentWindow();
            m_window_class.ClassId               = (ImGuiID)getUUID();
            m_window_class.ParentViewportId      = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed = false;
            m_window_class.DockingAlwaysTabBar   = false;
            return nullptr;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {600, 700}
            };
        }

        [[nodiscard]] std::string context() const override { return ""; }

        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override { return false; }

        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override {
            return false;
        }

        void onAttach() override;
        void onDetach() override;
        void onImGuiUpdate(TimeStep delta_time) override;

    private:
        std::vector<ItemInfo> m_item_infos;
        std::string m_search_buffer;
        int m_selected_item_index;
    };

}  // namespace Toolbox::UI