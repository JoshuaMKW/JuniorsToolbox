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

        auto profile_path = cwd_result.value() / "Profiles";
        auto result       = loadProfiles(profile_path);
        if (!result) {
            UI::logSerialError(result.error());
            return false;
        }

        return true;
    }

    std::expected<void, SerialError>
    SettingsManager::loadProfiles(const std::filesystem::path &path) {
        auto path_result = Toolbox::is_directory(path);
        if (!path_result || !path_result.value()) {
            return {};
        }

        ImGuiIO &io = ImGui::GetIO();

        for (auto &child_path : std::filesystem::directory_iterator{path}) {
            AppSettings settings;

            std::ifstream in_stream = std::ifstream(child_path.path(), std::ios::in);
            Deserializer in(in_stream.rdbuf());

            json_t profile_json;
            auto result = tryJSON(profile_json, [&](json_t &j) {
                in.stream() >> j;

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
                                               in.filepath());
            }

            addProfile(child_path.path().stem().string(), settings);
        }

        return {};
    }

    std::expected<void, SerialError>
    SettingsManager::saveProfiles(const std::filesystem::path &path) {
        return {};
    }

}  // namespace Toolbox