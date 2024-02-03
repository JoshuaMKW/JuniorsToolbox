#pragma once

#include "gui/window.hpp"
#include <string>
#include <string_view>

namespace Toolbox::UI {

    class SettingsWindow : public SimpleWindow {
    public:
        SettingsWindow() : SimpleWindow() {}
        ~SettingsWindow() override = default;

        ImGuiWindowFlags flags() const override {
            return SimpleWindow::flags() | ImGuiWindowFlags_NoResize;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {600, 500}
            };
        }
        std::optional<ImVec2> maxSize() const override { return minSize(); }

        [[nodiscard]] std::string context() const override { return ""; }
        [[nodiscard]] std::string name() const override { return "Application Settings"; }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow              = ImGui::GetCurrentWindow();
            m_window_class.ClassId                  = static_cast<ImGuiID>(getUUID());
            m_window_class.ParentViewportId         = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed    = false;
            m_window_class.DockingAlwaysTabBar      = false;
            m_window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
            return &m_window_class;
        }

        void renderBody(f32 delta_time) override;

    protected:
        void renderSettingsGeneral(f32 delta_time);
        void renderSettingsControl(f32 delta_time);
        void renderSettingsUI(f32 delta_time);
        void renderSettingsPreview(f32 delta_time);
        void renderSettingsAdvanced(f32 delta_time);
    };

}  // namespace Toolbox::UI