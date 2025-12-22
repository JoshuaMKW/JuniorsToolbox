#include "gui/settings/window.hpp"
#include "core/keybind/keybind.hpp"
#include "gui/application.hpp"
#include "gui/font.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/logging/errors.hpp"
#include "gui/settings.hpp"
#include "gui/themes.hpp"

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
        SettingsManager &manager = GUIApplication::instance().getSettingsManager();

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
                        TOOLBOX_INFO_V("[SETTINGS] Created profile \"{}\" successfully!",
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
                TOOLBOX_INFO_V("[SETTINGS] Deleted profile \"{}\" successfully!", profile_name);
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
        AppSettings &settings = GUIApplication::instance().getSettingsManager().getCurrentProfile();
        ImGui::Checkbox("Include Custom Objects", &settings.m_is_custom_obj_allowed);
        ImGui::Checkbox("Enable File Backup on Save", &settings.m_is_file_backup_allowed);

        static std::unordered_map<UpdateFrequency, std::string> s_values_map = {
            {UpdateFrequency::NEVER, "Never Update" },
            {UpdateFrequency::MAJOR, "Major Updates"},
            {UpdateFrequency::MINOR, "Minor Updates"},
            {UpdateFrequency::PATCH, "Patch Updates"},
        };

        ImGui::SetNextItemWidth(200.0f);

        if (ImGui::BeginCombo("Check For Updates",
                              s_values_map[settings.m_update_frequency].c_str())) {
            for (const auto &[freq, val] : s_values_map) {
                bool selected = freq == settings.m_update_frequency;
                if (ImGui::Selectable(val.c_str(), selected,
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    settings.m_update_frequency = freq;
                }
            }
            ImGui::EndCombo();
        }
    }

    void SettingsWindow::renderSettingsControl(TimeStep delta_time) {
        AppSettings &settings = GUIApplication::instance().getSettingsManager().getCurrentProfile();

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
        FontManager &font_manager         = FontManager::instance();
        ThemeManager &themes_manager      = GUIApplication::instance().getThemeManager();
        SettingsManager &settings_manager = GUIApplication::instance().getSettingsManager();
        AppSettings &settings             = settings_manager.getCurrentProfile();

        auto themes = themes_manager.themes();

        size_t selected_index                = themes_manager.getActiveThemeIndex();
        std::string_view selected_theme_name = themes.size() > 0 ? themes[selected_index]->name()
                                                                 : "";

        if (ImGui::BeginCombo("Theme", selected_theme_name.data())) {
            for (size_t i = 0; i < themes.size(); ++i) {
                auto theme    = themes.at(i);
                bool selected = i == selected_index;
                if (ImGui::Selectable(theme->name().data(), selected,
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected_index = i;
                    themes_manager.applyTheme(theme->name());
                    settings.m_gui_theme = theme->name();
                }
            }
            ImGui::EndCombo();
        }

        float current_font_size         = font_manager.getCurrentFontSize();
        std::string current_font_family = font_manager.getCurrentFontFamily();

        if (ImGui::BeginCombo("Font Type", std::format("{}", current_font_family).c_str())) {
            for (auto &family : font_manager.fontFamilies()) {
                bool selected = family == current_font_family;
                if (ImGui::Selectable(std::format("{}", family).c_str(), selected)) {
                    font_manager.setCurrentFontFamily(family);
                    settings.m_font_family = family;
                }
            }
            ImGui::EndCombo();
        }

#if 0
        if (ImGui::BeginCombo("Font Size", std::format("{}", current_font_size).c_str())) {
            for (auto& size : font_manager.fontSizes()) {
                bool selected = size == current_font_size;
                if (ImGui::Selectable(std::format("{}", size).c_str(), selected)) {
                    font_manager.setCurrentFontSize(size);
                    settings.m_font_size = size;
                }
            }
            ImGui::EndCombo();
        }
#else
        if (ImGui::SliderFloat("Font Size", &settings.m_font_size, font_manager.getFontSizeMin(),
                               font_manager.getFontSizeMax(), "%.0f",
                               ImGuiSliderFlags_AlwaysClamp)) {
            font_manager.setCurrentFontSize(settings.m_font_size);
        }
#endif
    }

    void SettingsWindow::renderSettingsPreview(TimeStep delta_time) {
        AppSettings &settings = GUIApplication::instance().getSettingsManager().getCurrentProfile();

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
        AppSettings &settings = GUIApplication::instance().getSettingsManager().getCurrentProfile();

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

        if (ImGui::BeginGroupPanel("Game Scene", nullptr, {})) {
            ImGui::Checkbox("Repack Scenes on Save", &settings.m_repack_scenes_on_save);
            ImGui::Checkbox("Cache Object Templates", &settings.m_is_template_cache_allowed);
        }
        ImGui::EndGroupPanel();

        ImGui::Checkbox("Pipe Logs To Terminal", &settings.m_log_to_cout_cerr);

        if (ImGui::Button("Clear Cache")) {
            fs_path cache_path = GUIApplication::instance().getAppDataPath() / ".cache";
            Filesystem::remove_all(cache_path).and_then([](uintmax_t) {
                TOOLBOX_INFO("[SETTINGS] Cleared Template cache successfully!");
                return Result<uintmax_t, FSError>();
            });

            // Any other caches...
        }
        ImGuiWindow *window = ImGui::GetCurrentWindow();

#ifdef TOOLBOX_PLATFORM_WINDOWS
        const std::string desired_ext = ".exe";
        const std::string filter_ext  = "exe";
#elif defined(TOOLBOX_PLATFORM_LINUX)
        const std::string desired_ext = "";
        const std::string filter_ext  = "";
#else
        const std::string desired_ext = ".exe";
        const std::string filter_ext  = "exe";
#endif

        if (m_is_path_dialog_open) {
            if (!FileDialog::instance()->isAlreadyOpen()) {
                FileDialogFilter filter;
                filter.addFilter("Dolphin Emulator", filter_ext);
                FileDialog::instance()->openDialog(window, settings.m_dolphin_path.string(), false,
                                                   filter);
            }
            m_is_path_dialog_open = false;
        }

        if (FileDialog::instance()->isDone(window)) {
            FileDialog::instance()->close();
            if (FileDialog::instance()->isOk()) {
                switch (FileDialog::instance()->getFilenameMode()) {
                case FileDialog::FileNameMode::MODE_OPEN: {
                    std::filesystem::path selected_path =
                        FileDialog::instance()->getFilenameResult();

                    std::string extension = selected_path.extension().string();
                    if (extension == desired_ext) {
                        settings.m_dolphin_path = selected_path;
                        std::string path_str    = selected_path.string();
                        for (size_t i = 0;
                             i < std::min(path_str.size(), m_dolphin_path_input.size()); ++i) {
                            m_dolphin_path_input[i] = path_str[i];
                        }
                    } else {
                        GUIApplication::instance().showErrorModal(
                            this, name(),
                            "The selected path does not have a valid extension! (look for an "
                            "executable file)");
                    }
                    break;
                }
                default:
                    GUIApplication::instance().showErrorModal(
                        this, name(),
                        "Invalid file dialog state detected! (Create an issue on github with "
                        "context please)");
                    break;
                }
            }
        }
    }

}  // namespace Toolbox::UI