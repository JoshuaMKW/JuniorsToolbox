#pragma once

#include "fsystem.hpp"
#include "json.hpp"
#include "serial.hpp"
#include "types.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>

namespace Toolbox {

    struct AppSettings {
        // General
        bool m_is_better_obj_allowed  = false;
        bool m_is_file_backup_allowed = false;

        // UI
        // TODO: Make family properly considered
        // where bold italic etc are all in the same
        // family for markdown etc (NotoSansJP)
        float m_font_size         = 16;
        std::string m_font_family = "NotoSansJP-Bold";
        std::string m_gui_theme   = "Default";

        // Preview
        bool m_is_rendering_simple  = false;
        bool m_is_show_origin_point = true;
        bool m_is_unique_rail_color = true;
        float m_camera_fov          = 70.0f;
        float m_camera_speed        = 1.0f;
        float m_camera_sensitivity  = 1.0f;
        float m_near_plane          = 1000.0f;
        float m_far_plane           = 500000.0f;

        // Control
        std::vector<int> m_gizmo_translate_mode_keybind = {GLFW_KEY_1};
        std::vector<int> m_gizmo_rotate_mode_keybind    = {GLFW_KEY_2};
        std::vector<int> m_gizmo_scale_mode_keybind     = {GLFW_KEY_3};

        // Advanced
        bool m_is_template_cache_allowed = true;
        bool m_log_to_cout_cerr          = false;
    };

    class SettingsManager {
    public:
        using json_t = nlohmann::json;

        SettingsManager() = default;

        static SettingsManager &instance() {
            static SettingsManager _inst;
            return _inst;
        }

        bool initialize();

        AppSettings &getProfile(std::string_view name) {
            return m_settings_profiles[std::string(name)];
        }

        AppSettings &getCurrentProfile() { return m_settings_profiles[m_current_profile]; }

        std::vector<std::string> getProfileNames() const {
            std::vector<std::string> names;
            for (const auto &pair : m_settings_profiles) {
                names.push_back(pair.first);
            }
            return names;
        }

        void addProfile(std::string_view name, const AppSettings &profile) {
            m_settings_profiles[std::string(name)] = profile;
        }

        std::expected<void, SerialError> loadProfiles(const std::filesystem::path &path);
        std::expected<void, SerialError> saveProfiles(const std::filesystem::path &path);

    private:
        std::string m_current_profile;
        std::unordered_map<std::string, AppSettings> m_settings_profiles;
    };

}  // namespace Toolbox