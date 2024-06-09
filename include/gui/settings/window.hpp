#pragma once

#include "gui/window.hpp"
#include <string>
#include <string_view>

namespace Toolbox::UI {

    class SettingsWindow : public ImWindow {
    public:
        SettingsWindow(const std::string &name) : ImWindow(name) {
            m_dolphin_path_input.fill('\0');
            m_profile_create_input.fill('\0');
        }
        ~SettingsWindow() override = default;

        [[nodiscard]] bool destroyOnClose() const noexcept override { return false; }

        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_NoResize;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {500, 500}
            };
        }
        std::optional<ImVec2> maxSize() const override { return minSize(); }

        [[nodiscard]] std::string context() const override { return ""; }

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

        void onRenderBody(TimeStep delta_time) override;

    protected:
        void renderProfileBar(TimeStep delta_time);
        void renderSettingsGeneral(TimeStep delta_time);
        void renderSettingsControl(TimeStep delta_time);
        void renderSettingsUI(TimeStep delta_time);
        void renderSettingsPreview(TimeStep delta_time);
        void renderSettingsAdvanced(TimeStep delta_time);

    private:
        bool m_is_making_profile = false;
        bool m_is_profile_focused_yet = false;
        bool m_is_path_dialog_opening = false;
        bool m_is_path_dialog_open    = false;
        std::array<char, 128> m_profile_create_input;
        std::array<char, 512> m_dolphin_path_input;
    };

}  // namespace Toolbox::UI