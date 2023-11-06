#include "gui/settings/window.hpp"
#include "gui/font.hpp"
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
            auto &font_manager = FontManager::instance();

            float current_font_size = font_manager.getCurrentFontSize();
            std::string current_font_family = font_manager.getCurrentFontFamily();

            if (ImGui::BeginCombo("Font Type", std::format("{}", current_font_family).c_str())) {
                for (auto &family : font_manager.fontFamilies()) {
                    bool selected = family == current_font_family;
                    if (ImGui::Selectable(std::format("{}", family).c_str(), selected)) {
                        font_manager.setCurrentFontFamily(family);
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::BeginCombo("Font Size", std::format("{}", current_font_size).c_str())) {
                for (auto &size : font_manager.fontSizes()) {
                    bool selected = size == current_font_size;
                    if (ImGui::Selectable(std::format("{}", size).c_str(),
                                          selected)) {
                        font_manager.setCurrentFontSize(size);
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