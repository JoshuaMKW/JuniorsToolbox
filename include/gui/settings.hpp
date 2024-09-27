#pragma once

#include "core/assert.hpp"
#include "core/keybind/keybind.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "json.hpp"
#include "serial.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDE_NONE

namespace Toolbox {

    using namespace Input;

    struct AppSettings {
        // General
        bool m_is_better_obj_allowed  = false;
        bool m_is_file_backup_allowed = false;

        // UI
        // TODO: Make family properly considered
        // where bold italic etc are all in the same
        // family for markdown etc (NotoSansJP)
        float m_font_size         = 16;
        std::string m_font_family = "NotoSansJP-Regular";
        std::string m_gui_theme   = "Default";

        // Preview
        bool m_is_rendering_simple  = false;
        bool m_is_show_origin_point = true;
        bool m_is_unique_rail_color = true;
        float m_camera_fov          = 70.0f;
        float m_camera_speed        = 1.0f;
        float m_camera_sensitivity  = 1.0f;
        float m_near_plane          = 50.0f;
        float m_far_plane           = 500000.0f;

        // Control
        KeyBind m_gizmo_translate_mode_keybind = KeyBind({KeyCode::KEY_D1});
        KeyBind m_gizmo_rotate_mode_keybind    = KeyBind({KeyCode::KEY_D2});
        KeyBind m_gizmo_scale_mode_keybind     = KeyBind({KeyCode::KEY_D3});

        // Advanced
        std::filesystem::path m_dolphin_path = "";
        s64 m_dolphin_refresh_rate           = 100;  // In milliseconds
        bool m_is_template_cache_allowed     = true;
        bool m_log_to_cout_cerr              = false;
    };

    class SettingsManager {
    public:
        using json_t = nlohmann::json;

        SettingsManager() = default;

        bool initialize();
        bool save();

        bool saveProfile(std::string_view name);

        std::filesystem::path getPath() const { return m_profile_path; }
        void setPath(const std::filesystem::path &profile_path) { m_profile_path = profile_path; }

        AppSettings &getProfile(std::string_view name) {
            return m_settings_profiles[std::string(name)];
        }

        AppSettings &getCurrentProfile() {
            if (m_current_profile == "") {
                m_current_profile = "Default";
            }
            return m_settings_profiles[m_current_profile];
        }

        std::string_view getCurrentProfileName() const { return m_current_profile; }
        void setCurrentProfile(std::string_view name) { m_current_profile = std::string(name); }

        std::vector<std::string> getProfileNames() const {
            std::vector<std::string> names;
            for (const auto &pair : m_settings_profiles) {
                names.push_back(pair.first);
            }
            return names;
        }

        Result<void, SerialError> addProfile(std::string_view name, const AppSettings &profile);
        Result<void, SerialError> removeProfile(std::string_view name);

    protected:
        Result<void, SerialError> loadProfiles();
        Result<void, SerialError> saveProfiles();

        Result<void, SerialError> saveProfile(std::string_view name, const AppSettings &profile);

    private:
        std::filesystem::path m_profile_path;
        std::string m_current_profile;
        std::unordered_map<std::string, AppSettings> m_settings_profiles;
    };

}  // namespace Toolbox