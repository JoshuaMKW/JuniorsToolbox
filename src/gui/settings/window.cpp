#include "gui/settings/window.hpp"
#include "core/keybind/keybind.hpp"
#include "gui/font.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/settings.hpp"
#include "gui/themes.hpp"
#include <ImGuiFileDialog.h>
#include <gui/logging/errors.hpp>

namespace Toolbox::UI {

    bool KeyBindInput(std::string_view text, bool *is_reading, const KeyBind &keybind,
                      KeyBind &new_binding) {
        if (!is_reading)
            return false;

        ImGui::PushID(text.data());

        std::string keybind_name = *is_reading ? new_binding.toString() + " ..."
                                               : keybind.toString();
        ImGui::InputText("##binding_text", keybind_name.data(), keybind_name.size(),
                         ImGuiInputTextFlags_ReadOnly);

        ImGui::SameLine();

        ImVec2 padding     = ImGui::GetStyle().FramePadding;
        ImVec2 text_size   = ImGui::CalcTextSize(ICON_FK_KEYBOARD_O);
        ImVec2 button_size = {
            text_size.x + padding.x * 2,
            text_size.y + padding.y * 2,
        };

        if (ImGui::Button((*is_reading) ? ICON_FK_UNDO : ICON_FK_KEYBOARD_O, button_size)) {
            *is_reading ^= true;
        }

        ImGui::SameLine();

        ImGui::Text(text.data());

        bool is_binding_finalized = false;

        if (!(*is_reading)) {
            new_binding = KeyBind();
        } else {
            is_binding_finalized = new_binding.scanNextInputKey();
            if (is_binding_finalized) {
                *is_reading = false;
            }
        }

        ImGui::PopID();

        return is_binding_finalized;
    }

    void SettingsWindow::onRenderBody(TimeStep delta_time) {
        renderProfileBar(delta_time);

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

    void SettingsWindow::renderProfileBar(TimeStep delta_time) {
        SettingsManager &manager = SettingsManager::instance();

        ImGui::Text("Current Profile");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (m_is_making_profile) {
            if (ImGui::InputTextWithHint(
                    "##profile_input", "Enter a unique name...", m_profile_create_input.data(),
                    m_profile_create_input.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string_view profile_name(m_profile_create_input.data());
                if (profile_name != "") {
                    auto result = manager.addProfile(profile_name, AppSettings());
                    if (!result) {
                        LogError(result.error());
                    } else {
                        manager.setCurrentProfile(profile_name);
                        TOOLBOX_INFO_V("(Settings) Created profile \"{}\" successfully!",
                                       profile_name);
                    }
                }
                m_is_making_profile = false;
            }
            if (!m_is_profile_focused_yet) {
                ImGui::FocusItem();
                m_is_profile_focused_yet = true;
            } else if (!ImGui::IsItemFocused()) {
                std::string_view profile_name(m_profile_create_input.data());
                if (profile_name != "") {
                    manager.addProfile(profile_name, AppSettings());
                    manager.setCurrentProfile(profile_name);
                }
                m_is_making_profile = false;
            }
        } else {
            if (ImGui::BeginCombo("##profile_combo", manager.getCurrentProfileName().data(),
                                  ImGuiComboFlags_PopupAlignLeft)) {
                for (auto &profile : manager.getProfileNames()) {
                    bool throwaway = false;
                    if (ImGui::Selectable(profile.c_str(), &throwaway)) {
                        manager.setCurrentProfile(profile);
                        std::string dolphin_path_str =
                            manager.getCurrentProfile().m_dolphin_path.string();
                        for (size_t i = 0;
                             i < std::min(dolphin_path_str.size(), m_dolphin_path_input.size());
                             ++i) {
                            m_dolphin_path_input[i] = dolphin_path_str[i];
                        }
                        break;
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("New")) {
            // Clear the input state
            for (size_t i = 0; i < m_profile_create_input.size(); ++i) {
                m_profile_create_input[i] = '\0';
            }
            m_is_making_profile      = true;
            m_is_profile_focused_yet = false;
        }

        ImGui::SameLine();

        const bool can_use_io =
            !m_is_making_profile && manager.getCurrentProfileName() != "Default";
        if (!can_use_io) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Delete")) {
            std::string profile_name = std::string(manager.getCurrentProfileName());
            auto result              = manager.removeProfile(profile_name);
            if (!result) {
                LogError(result.error());
            } else {
                TOOLBOX_INFO_V("(Settings) Deleted profile \"{}\" successfully!", profile_name);
            }
        }

        ImGui::SameLine();

        ImVec2 text_size      = ImGui::CalcTextSize("Save Profile");
        ImVec2 item_padding   = ImGui::GetStyle().FramePadding;
        ImVec2 window_padding = ImGui::GetStyle().WindowPadding;
        ImVec2 window_size    = ImGui::GetWindowSize();

        ImGui::SetCursorPosX(window_size.x - window_padding.x - text_size.x - item_padding.x * 2);
        if (ImGui::Button("Save Profile")) {
            manager.saveProfile(manager.getCurrentProfileName());
        }

        if (!can_use_io) {
            ImGui::EndDisabled();
        }
    }

    void SettingsWindow::renderSettingsGeneral(TimeStep delta_time) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();
        ImGui::Checkbox("Include BetterSMS Objects", &settings.m_is_better_obj_allowed);
        ImGui::Checkbox("Enable File Backup on Save", &settings.m_is_file_backup_allowed);
    }

    void SettingsWindow::renderSettingsControl(TimeStep delta_time) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();

        static KeyBind s_gizmo_translate_keybind = {};
        static KeyBind s_gizmo_rotate_keybind    = {};
        static KeyBind s_gizmo_scale_keybind     = {};

        static bool s_gizmo_translate_keybind_active = false;
        static bool s_gizmo_rotate_keybind_active    = false;
        static bool s_gizmo_scale_keybind_active     = false;

        if (ImGui::BeginGroupPanel("Camera", nullptr, {})) {
            {
                bool should_clear_others_on_use = !s_gizmo_translate_keybind_active;
                if (KeyBindInput("Translate", &s_gizmo_translate_keybind_active,
                                 settings.m_gizmo_translate_mode_keybind,
                                 s_gizmo_translate_keybind)) {
                    if (!s_gizmo_translate_keybind.empty()) {
                        settings.m_gizmo_translate_mode_keybind = s_gizmo_translate_keybind;
                        s_gizmo_translate_keybind               = KeyBind();
                    }
                }
                if (should_clear_others_on_use && s_gizmo_translate_keybind_active) {
                    s_gizmo_rotate_keybind_active = false;
                    s_gizmo_scale_keybind_active  = false;
                }
            }
            {
                bool should_clear_others_on_use = !s_gizmo_rotate_keybind_active;
                if (KeyBindInput("Rotate", &s_gizmo_rotate_keybind_active,
                                 settings.m_gizmo_rotate_mode_keybind, s_gizmo_rotate_keybind)) {
                    if (!s_gizmo_rotate_keybind.empty()) {
                        settings.m_gizmo_rotate_mode_keybind = s_gizmo_rotate_keybind;
                        s_gizmo_rotate_keybind               = KeyBind();
                    }
                }
                if (should_clear_others_on_use && s_gizmo_rotate_keybind_active) {
                    s_gizmo_translate_keybind_active = false;
                    s_gizmo_scale_keybind_active     = false;
                }
            }
            {
                bool should_clear_others_on_use = !s_gizmo_scale_keybind_active;
                if (KeyBindInput("Scale", &s_gizmo_scale_keybind_active,
                                 settings.m_gizmo_scale_mode_keybind, s_gizmo_scale_keybind)) {
                    if (!s_gizmo_scale_keybind.empty()) {
                        settings.m_gizmo_scale_mode_keybind = s_gizmo_scale_keybind;
                        s_gizmo_scale_keybind               = KeyBind();
                    }
                }
                if (should_clear_others_on_use && s_gizmo_scale_keybind_active) {
                    s_gizmo_translate_keybind_active = false;
                    s_gizmo_rotate_keybind_active    = false;
                }
            }
        }
        ImGui::EndGroupPanel();
    }

    void SettingsWindow::renderSettingsUI(TimeStep delta_time) {
        auto &manager = ThemeManager::instance();

        auto themes = manager.themes();

        size_t selected_index         = manager.getActiveThemeIndex();
        RefPtr<ITheme> selected_theme = themes.at(manager.getActiveThemeIndex());

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

    void SettingsWindow::renderSettingsPreview(TimeStep delta_time) {
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

    void SettingsWindow::renderSettingsAdvanced(TimeStep delta_time) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();

        if (ImGui::BeginGroupPanel("Dolphin Integration", nullptr, {})) {
            s64 min = 1;
            s64 max = 100;
            float button_width =
                ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float label_width = ImGui::CalcTextSize("Dolphin Path").x;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button_width - label_width -
                                    ImGui::GetStyle().ItemSpacing.x * 2);
            if (ImGui::InputTextWithHint("Dolphin Path", "Specify the Dolphin EXE path...",
                                         m_dolphin_path_input.data(), m_dolphin_path_input.size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                // TODO: This copy system is slow and should be replace with a proper resize
                // callback
                settings.m_dolphin_path =
                    std::string(m_dolphin_path_input.data(), m_dolphin_path_input.size());
            }
            ImGui::SameLine();
            if (ImGui::AlignedButton("...", {button_width, 0}) && !m_is_path_dialog_open) {
                m_is_path_dialog_open = true;
            }
            ImGui::SliderScalar("Hook Refresh Rate", ImGuiDataType_S64,
                                &settings.m_dolphin_refresh_rate, &min, &max, nullptr,
                                ImGuiSliderFlags_AlwaysClamp);
        }
        ImGui::EndGroupPanel();

        ImGui::Checkbox("Cache Object Templates", &settings.m_is_template_cache_allowed);
        ImGui::Checkbox("Pipe Logs To Terminal", &settings.m_log_to_cout_cerr);

        if (ImGui::Button("Clear Cache")) {
            auto cwd_result = Toolbox::Filesystem::current_path();
            if (!cwd_result) {
                LogError(cwd_result.error());
                return;
            }

            Toolbox::fs_path cwd = cwd_result.value();

            auto template_cache_result = Toolbox::Filesystem::remove_all(cwd / "Templates/.cache");
            if (!template_cache_result) {
                LogError(template_cache_result.error());
                return;
            }

            TOOLBOX_INFO("(Settings) Cleared Template cache successfully!");

            // Any other caches...
        }

        if (m_is_path_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog(
                "OpenDolphinDialog", "Choose Dolphin EXE", {"Dolphin{.exe}"},
                settings.m_dolphin_path.string(), "Dolphin.exe");
        }

        if (ImGuiFileDialog::Instance()->Display("OpenDolphinDialog")) {
            ImGuiFileDialog::Instance()->Close();
            m_is_path_dialog_open = false;

            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                auto file_result = Toolbox::Filesystem::is_regular_file(path);
                if (!file_result) {
                    return;
                }

                if (!file_result.value()) {
                    return;
                }

                settings.m_dolphin_path = path;
                std::string path_str    = path.string();
                for (size_t i = 0; i < std::min(path_str.size(), m_dolphin_path_input.size());
                     ++i) {
                    m_dolphin_path_input[i] = path_str[i];
                }
            }
        }
    }

}  // namespace Toolbox::UI