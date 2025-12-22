#include "gui/settings.hpp"
#include "core/keybind/keybind.hpp"
#include "fsystem.hpp"
#include "gui/logging/errors.hpp"
#include "jsonlib.hpp"

#include <filesystem>
#include <fstream>
#include <imgui.h>

namespace Toolbox {

    bool SettingsManager::initialize(const fs_path &profile_path) {
        m_profile_path = profile_path;

        bool result = true;
        TRY(loadProfiles()).error([&result](const SerialError &error) {
            UI::LogError(error);
            result = false;
        });
        return result;
    }

    void SettingsManager::setCurrentProfile(std::string_view name) {
        m_current_profile = std::string(name);

        std::filesystem::path child_path = m_profile_path / "profile.txt";
        std::ofstream out_stream(child_path, std::ios::out);
        out_stream << m_current_profile;
    }

    bool SettingsManager::save() {
        bool result = true;
        TRY(saveProfiles()).error([&result](const SerialError &error) {
            UI::LogError(error);
            result = false;
        });
        return result;
    }

    bool SettingsManager::saveProfile(std::string_view name) {
        bool result = true;
        TRY(saveProfile(name, getProfile(name))).error([&result](const SerialError &error) {
            UI::LogError(error);
            result = false;
        });
        return result;
    }

    Result<void, SerialError> SettingsManager::loadProfiles() {
        auto path_result = Toolbox::Filesystem::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            return {};
        }

        for (auto &child_path : std::filesystem::directory_iterator{m_profile_path}) {
            if (!child_path.path().string().ends_with(".json")) {
                continue;
            }

            AppSettings settings;

            std::ifstream in_stream = std::ifstream(child_path.path(), std::ios::in);

            json_t profile_json;
            auto result = tryJSON(profile_json, [&](json_t &j) {
                in_stream >> j;

                // General
                settings.m_is_custom_obj_allowed  = j["Include Custom Objects"];
                settings.m_is_file_backup_allowed = j["Backup File On Save"];
                settings.m_update_frequency       = j["Update Frequency"];

                // Control
                settings.m_gizmo_translate_mode_keybind =
                    KeyBind::FromString(j["Gizmo Translate Mode"]);
                settings.m_gizmo_rotate_mode_keybind = KeyBind::FromString(j["Gizmo Rotate Mode"]);
                settings.m_gizmo_scale_mode_keybind  = KeyBind::FromString(j["Gizmo Scale Mode"]);

                // UI
                settings.m_gui_theme   = j["App Theme"];
                settings.m_font_family = j["Font Family"];
                settings.m_font_size   = j["Font Size"];

                // Preview
                settings.m_is_rendering_simple  = j["Simple Rendering"];
                settings.m_is_show_origin_point = j["Show Origin Point"];
                settings.m_is_unique_rail_color = j["Unique Rail Colors"];
                settings.m_camera_fov           = j["Camera FOV"];
                settings.m_camera_speed         = j["Camera Speed"];
                settings.m_camera_sensitivity   = j["Camera Sensitivity"];
                settings.m_near_plane           = j["Camera Near Plane"];
                settings.m_far_plane            = j["Camera Far Plane"];

                // Advanced
                settings.m_dolphin_path = std::filesystem::path(std::string(j["Dolphin Path"]));
                settings.m_dolphin_refresh_rate      = j["Dolphin Refresh Rate"];
                settings.m_is_template_cache_allowed = j["Cache Object Templates"];
                settings.m_log_to_cout_cerr          = j["Log To Terminal"];
                settings.m_repack_scenes_on_save     = j["Repack Scenes on Save"];
            });

            if (!result) {
                JSONError &err = result.error();
                return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                               child_path.path().string());
            }

            addProfile(child_path.path().stem().string(), settings);
        }

        m_settings_profiles["Default"] = AppSettings();

        std::ifstream in_stream = std::ifstream(m_profile_path / "profile.txt", std::ios::in);
        if (!in_stream) {
            TOOLBOX_WARN("[SETTINGS] Profile config not found! Initializing it now.");
            {
                std::filesystem::path child_path = m_profile_path / "profile.txt";
                std::ofstream out_stream(child_path, std::ios::out);
                out_stream << "Default";
            }
            return {};
        }

        std::getline(in_stream, m_current_profile);

        return {};
    }

    Result<void, SerialError> SettingsManager::saveProfiles() {
        auto path_result = Toolbox::Filesystem::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            TOOLBOX_WARN("[SETTINGS] Folder \"Profiles\" not found, generating the folder now.");
            auto result = Toolbox::Filesystem::create_directory(m_profile_path);
            if (!result) {
                TOOLBOX_ERROR("[SETTINGS] Folder \"Profiles\" failed to create!");
                return {};
            }
        }

        for (auto &[key, value] : m_settings_profiles) {
            auto result = saveProfile(key, value);
            if (!result) {
                return result;
            }
        }

        std::filesystem::path child_path = m_profile_path / "profile.txt";
        std::ofstream out_stream(child_path, std::ios::out);
        out_stream << m_current_profile;

        return {};
    }

    Result<void, SerialError> SettingsManager::addProfile(std::string_view name,
                                                          const AppSettings &profile) {
        m_settings_profiles[std::string(name)] = profile;
        return saveProfile(name, profile);
    }

    Result<void, SerialError> SettingsManager::removeProfile(std::string_view name) {
        m_settings_profiles.erase(std::string(name));

        auto path_result = Toolbox::Filesystem::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            TOOLBOX_WARN("[SETTINGS] Folder \"Profiles\" not found, generating the folder now.");
            auto result = Toolbox::Filesystem::create_directory(m_profile_path);
            if (!result) {
                TOOLBOX_ERROR("[SETTINGS] Folder \"Profiles\" failed to create!");
                return {};
            }
        }

        std::filesystem::path child_path = m_profile_path / (std::string(name) + ".json");
        auto result                      = Toolbox::Filesystem::remove(child_path);
        if (!result) {
            TOOLBOX_ERROR("[SETTINGS] Failed to remove deleted profile from Folder \"Profiles\"");
        }

        m_current_profile = "Default";

        return {};
    }

    Result<void, SerialError> SettingsManager::saveProfile(std::string_view name,
                                                           const AppSettings &profile) {
        auto path_result = Toolbox::Filesystem::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            TOOLBOX_WARN("[SETTINGS] Folder \"Profiles\" not found, generating the folder now.");
            auto result = Toolbox::Filesystem::create_directory(m_profile_path);
            if (!result) {
                TOOLBOX_ERROR("[SETTINGS] Folder \"Profiles\" failed to create!");
                return {};
            }
        }

        std::filesystem::path child_path = m_profile_path / (std::string(name) + ".json");
        std::ofstream out_stream(child_path, std::ios::out);

        json_t profile_json;
        auto result = tryJSON(profile_json, [&](json_t &j) {
            // General
            j["Include Custom Objects"] = profile.m_is_custom_obj_allowed;
            j["Backup File On Save"]    = profile.m_is_file_backup_allowed;
            j["Update Frequency"]       = profile.m_update_frequency;

            // Control
            j["Gizmo Translate Mode"] = profile.m_gizmo_translate_mode_keybind.toString();
            j["Gizmo Rotate Mode"]    = profile.m_gizmo_rotate_mode_keybind.toString();
            j["Gizmo Scale Mode"]     = profile.m_gizmo_scale_mode_keybind.toString();

            // UI
            j["App Theme"]   = profile.m_gui_theme;
            j["Font Family"] = profile.m_font_family;
            j["Font Size"]   = profile.m_font_size;

            // Preview
            j["Simple Rendering"]   = profile.m_is_rendering_simple;
            j["Show Origin Point"]  = profile.m_is_show_origin_point;
            j["Unique Rail Colors"] = profile.m_is_unique_rail_color;
            j["Camera FOV"]         = profile.m_camera_fov;
            j["Camera Speed"]       = profile.m_camera_speed;
            j["Camera Sensitivity"] = profile.m_camera_sensitivity;
            j["Camera Near Plane"]  = profile.m_near_plane;
            j["Camera Far Plane"]   = profile.m_far_plane;

            // Advanced
            j["Dolphin Path"]           = profile.m_dolphin_path.string();
            j["Dolphin Refresh Rate"]   = profile.m_dolphin_refresh_rate;
            j["Cache Object Templates"] = profile.m_is_template_cache_allowed;
            j["Log To Terminal"]        = profile.m_log_to_cout_cerr;
            j["Repack Scenes on Save"] = profile.m_repack_scenes_on_save;
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           child_path.string());
        }

        out_stream << profile_json;
        return {};
    }

}  // namespace Toolbox