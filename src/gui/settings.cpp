#include "gui/settings.hpp"
#include "fsystem.hpp"
#include "gui/keybind.hpp"
#include "gui/logging/errors.hpp"
#include "jsonlib.hpp"

#include <filesystem>
#include <fstream>
#include <imgui.h>

namespace Toolbox {

    bool SettingsManager::initialize() {
        auto cwd_result = Toolbox::current_path();
        if (!cwd_result) {
            return false;
        }

        m_profile_path = cwd_result.value() / "Profiles";
        auto result    = loadProfiles();
        if (!result) {
            UI::logSerialError(result.error());
            return false;
        }

        return true;
    }

    bool SettingsManager::save() {
        auto result       = saveProfiles();
        if (!result) {
            UI::logSerialError(result.error());
            return false;
        }

        return true;
    }

    bool SettingsManager::saveProfile(std::string_view name) {
        auto result = saveProfile(name, getProfile(name));
        if (!result) {
            UI::logSerialError(result.error());
            return false;
        }

        return true;
    }

    std::expected<void, SerialError>
    SettingsManager::loadProfiles() {
        auto path_result = Toolbox::is_directory(m_profile_path);
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
                settings.m_is_better_obj_allowed  = j["Include Better Objects"];
                settings.m_is_file_backup_allowed = j["Backup File On Save"];

                // Control
                settings.m_gizmo_translate_mode_keybind =
                    UI::KeyBindFromString(j["Gizmo Translate Mode"]);
                settings.m_gizmo_rotate_mode_keybind =
                    UI::KeyBindFromString(j["Gizmo Rotate Mode"]);
                settings.m_gizmo_scale_mode_keybind = UI::KeyBindFromString(j["Gizmo Scale Mode"]);

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
                settings.m_is_template_cache_allowed = j["Cache Object Templates"];
                settings.m_log_to_cout_cerr          = j["Log To Terminal"];
            });

            if (!result) {
                JSONError &err = result.error();
                return make_serial_error<void>(err.m_message, err.m_reason, err.m_byte,
                                               child_path.path().string());
            }

            addProfile(child_path.path().stem().string(), settings);
        }

        std::ifstream in_stream = std::ifstream(m_profile_path / "profile.txt", std::ios::in);
        if (!in_stream) {
            TOOLBOX_WARN("Profile config not found! (profile.txt is missing, it will be "
                         "generated on settings save)");
            return {};
        }

        in_stream >> m_current_profile;

        return {};
    }

    std::expected<void, SerialError>
    SettingsManager::saveProfiles() {
        auto path_result = Toolbox::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            TOOLBOX_WARN("Folder \"Profiles\" not found, generating the folder now.");
            auto result = Toolbox::create_directory(m_profile_path);
            if (!result) {
                TOOLBOX_ERROR("Folder \"Profiles\" failed to create!");
                return {};
            }
        }

        for (auto &[key, value] : m_settings_profiles) {
            std::filesystem::path child_path = m_profile_path / (key + ".json");
            std::ofstream out_stream(child_path, std::ios::out);

            json_t profile_json;
            auto result = tryJSON(profile_json, [&](json_t &j) {
                // General
                j["Include Better Objects"] = value.m_is_better_obj_allowed;
                j["Backup File On Save"]    = value.m_is_file_backup_allowed;

                // Control
                j["Gizmo Translate Mode"] =
                    UI::KeyBindToString(value.m_gizmo_translate_mode_keybind);
                j["Gizmo Rotate Mode"] = UI::KeyBindToString(value.m_gizmo_rotate_mode_keybind);
                j["Gizmo Scale Mode"]  = UI::KeyBindToString(value.m_gizmo_scale_mode_keybind);

                // UI
                j["App Theme"]   = value.m_gui_theme;
                j["Font Family"] = value.m_font_family;
                j["Font Size"]   = value.m_font_size;

                // Preview
                j["Simple Rendering"]   = value.m_is_rendering_simple;
                j["Show Origin Point"]  = value.m_is_show_origin_point;
                j["Unique Rail Colors"] = value.m_is_unique_rail_color;
                j["Camera FOV"]         = value.m_camera_fov;
                j["Camera Speed"]       = value.m_camera_speed;
                j["Camera Sensitivity"] = value.m_camera_sensitivity;
                j["Camera Near Plane"]  = value.m_near_plane;
                j["Camera Far Plane"]   = value.m_far_plane;

                // Advanced
                j["Cache Object Templates"] = value.m_is_template_cache_allowed;
                j["Log To Terminal"]        = value.m_log_to_cout_cerr;
            });

            if (!result) {
                JSONError &err = result.error();
                return make_serial_error<void>(err.m_message, err.m_reason, err.m_byte,
                                               child_path.string());
            }

            out_stream << profile_json;
        }

        std::filesystem::path child_path = m_profile_path / "profile.txt";
        std::ofstream out_stream(child_path, std::ios::out);
        out_stream << m_current_profile;

        return {};
    }

    std::expected<void, SerialError> SettingsManager::addProfile(std::string_view name,
                                                                 const AppSettings &profile) {
        m_settings_profiles[std::string(name)] = profile;
        return saveProfile(name, profile);
    }

    std::expected<void, SerialError> SettingsManager::removeProfile(std::string_view name) {
        m_settings_profiles.erase(std::string(name));

        auto path_result = Toolbox::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            TOOLBOX_WARN("Folder \"Profiles\" not found, generating the folder now.");
            auto result = Toolbox::create_directory(m_profile_path);
            if (!result) {
                TOOLBOX_ERROR("Folder \"Profiles\" failed to create!");
                return {};
            }
        }

        std::filesystem::path child_path = m_profile_path / (std::string(name) + ".json");
        auto result = Toolbox::remove(child_path);
        if (!result) {
            TOOLBOX_ERROR("Failed to remove deleted profile from Folder \"Profiles\"");
        }

        m_current_profile = "Default";

        return {};
    }

    std::expected<void, SerialError> SettingsManager::saveProfile(std::string_view name, const AppSettings &profile) {
        auto path_result = Toolbox::is_directory(m_profile_path);
        if (!path_result || !path_result.value()) {
            TOOLBOX_WARN("Folder \"Profiles\" not found, generating the folder now.");
            auto result = Toolbox::create_directory(m_profile_path);
            if (!result) {
                TOOLBOX_ERROR("Folder \"Profiles\" failed to create!");
                return {};
            }
        }

        std::filesystem::path child_path = m_profile_path / (std::string(name) + ".json");
        std::ofstream out_stream(child_path, std::ios::out);

        json_t profile_json;
        auto result = tryJSON(profile_json, [&](json_t &j) {
            // General
            j["Include Better Objects"] = profile.m_is_better_obj_allowed;
            j["Backup File On Save"]    = profile.m_is_file_backup_allowed;

            // Control
            j["Gizmo Translate Mode"] = UI::KeyBindToString(profile.m_gizmo_translate_mode_keybind);
            j["Gizmo Rotate Mode"]    = UI::KeyBindToString(profile.m_gizmo_rotate_mode_keybind);
            j["Gizmo Scale Mode"]     = UI::KeyBindToString(profile.m_gizmo_scale_mode_keybind);

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
            j["Cache Object Templates"] = profile.m_is_template_cache_allowed;
            j["Log To Terminal"]        = profile.m_log_to_cout_cerr;
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message, err.m_reason, err.m_byte,
                                           child_path.string());
        }

        out_stream << profile_json;
        return {};
    }

}  // namespace Toolbox