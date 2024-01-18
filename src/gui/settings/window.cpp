#include "gui/settings/window.hpp"
#include "gui/font.hpp"
#include "gui/settings.hpp"
#include "gui/themes.hpp"
#include "gui/imgui_ext.hpp"

namespace Toolbox::UI {

    void SettingsWindow::renderBody(f32 delta_time) {

        /*ImGuiWindowClass panelOverride;
        panelOverride.ClassId                  = getID();
        panelOverride.ParentViewportId         = ImGui::GetCurrentWindow()->ViewportId;
        panelOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
        panelOverride.DockingAllowUnclassed    = false;*/

        if (ImGui::BeginTabBar("##settings_tabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {

            if (ImGui::BeginTabItem("General")) {
                renderSettingsGeneral(delta_time);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Control")) {
                renderSettingsControl(delta_time);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("UI")) {
                renderSettingsUI(delta_time);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Preview")) {
                renderSettingsPreview(delta_time);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Advanced")) {
                renderSettingsAdvanced(delta_time);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    void SettingsWindow::renderSettingsGeneral(f32 delta_time) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();
        ImGui::Checkbox("Include BetterSMS Objects", &settings.m_is_better_obj_allowed);
        ImGui::Checkbox("Enable File Backup on Save", &settings.m_is_file_backup_allowed);
    }

    void SettingsWindow::renderSettingsControl(f32 delta_time) {}

    void SettingsWindow::renderSettingsUI(f32 delta_time) {
        auto &manager = ThemeManager::instance();

        auto themes = manager.themes();

        size_t selected_index                  = manager.getActiveThemeIndex();
        std::shared_ptr<ITheme> selected_theme = themes.at(manager.getActiveThemeIndex());

        if (ImGui::BeginCombo("Theme", selected_theme->name().data())) {
            for (size_t i = 0; i < themes.size(); ++i) {
                auto theme    = themes.at(i);
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

        float current_font_size         = font_manager.getCurrentFontSize();
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
                if (ImGui::Selectable(std::format("{}", size).c_str(), selected)) {
                    font_manager.setCurrentFontSize(size);
                }
            }
            ImGui::EndCombo();
        }
    }

    void SettingsWindow::renderSettingsPreview(f32 delta_time) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();

        ImGui::Checkbox("Use Simple Rendering", &settings.m_is_rendering_simple);
        ImGui::Checkbox("Show Origin Point", &settings.m_is_show_origin_point);
        ImGui::Checkbox("Assign Unique Rail Colors", &settings.m_is_unique_rail_color);

        if (ImGui::BeginGroupPanel("Camera", nullptr, {})) {
            ImGui::SliderFloat("FOV", &settings.m_camera_fov, 30.0f, 110.0f, "%.0f",
                               ImGuiSliderFlags_AlwaysClamp);

            ImGui::SliderFloat("Near Plane", &settings.m_near_plane, 10.0f, 1000.0f, "%.0f",
                               ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);

            ImGui::SliderFloat("Far Plane", &settings.m_far_plane, 100000.0f, 10000000.0f, "%.0f",
                               ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);

            ImGui::SliderFloat("Speed", &settings.m_camera_speed, 0.5f, 5.0f, "%.1f",
                               ImGuiSliderFlags_AlwaysClamp);

            ImGui::SliderFloat("Sensitivity", &settings.m_camera_sensitivity, 0.5f, 5.0f, "%.1f",
                               ImGuiSliderFlags_AlwaysClamp);
        }
        ImGui::EndGroupPanel();
    }

    void SettingsWindow::renderSettingsAdvanced(f32 delta_time) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();
        ImGui::Checkbox("Cache Object Templates", &settings.m_is_template_cache_allowed);
        ImGui::Checkbox("Pipe Logs To Terminal", &settings.m_log_to_cout_cerr);
    }

}  // namespace Toolbox::UI