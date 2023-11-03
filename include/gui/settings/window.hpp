#pragma once

#include "gui/window.hpp"
#include <string>
#include <string_view>

namespace Toolbox::UI {

    class SettingsWindow : public DockWindow {
    public:
        SettingsWindow() : DockWindow() {}
        ~SettingsWindow() override = default;

        ImGuiWindowFlags flags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoDocking;
        }

        [[nodiscard]] std::string context() const override { return ""; }
        [[nodiscard]] std::string name() const override { return "Application Settings"; }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow              = ImGui::GetCurrentWindow();
            m_window_class.ClassId                  = ImGui::GetID(title().c_str());
            m_window_class.ParentViewportId         = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed    = false;
            m_window_class.DockingAlwaysTabBar      = false;
            m_window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
            return &m_window_class;
        }

        void buildDockspace(ImGuiID dockspace_id) override;
        void renderBody(f32 delta_time) override;
    };

}  // namespace Toolbox::UI