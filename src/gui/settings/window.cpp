#include "gui/settings/window.hpp"
#include "gui/themes.hpp"

namespace Toolbox::UI {

    void SettingsWindow::buildDockspace(ImGuiID dockspace_id) {
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "General").c_str(), dockspace_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "UI").c_str(), dockspace_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Advanced").c_str(), dockspace_id);
    }

    void SettingsWindow::renderBody(f32 delta_time) {
        if (ImGui::Begin(getWindowChildUID(*this, "General").c_str())) {
            // ...
        }
        ImGui::End();

        if (ImGui::Begin(getWindowChildUID(*this, "UI").c_str())) {
            auto &manager     = ThemeManager::instance();

            auto themes      = manager.themes();

            size_t selected_index = manager.getActiveThemeIndex();
            std::shared_ptr<ITheme> selected_theme = themes.at(manager.getActiveThemeIndex());

            if (ImGui::BeginCombo("Theme", selected_theme->name().data())) {
                for (size_t i = 0; i < themes.size(); ++i) {
                    auto theme = themes.at(i);
                    bool selected = i == selected_index;
                    if (ImGui::Selectable(theme->name().data(), selected,
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        selected_index = i;
                        manager.applyTheme(theme->name());
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::End();

        if (ImGui::Begin(getWindowChildUID(*this, "Advanced").c_str())) {
            // ...
        }
        ImGui::End();
    }

}  // namespace Toolbox::UI