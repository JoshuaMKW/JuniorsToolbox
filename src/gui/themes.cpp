#include <fstream>

#include "fsystem.hpp"
#include "gui/application.hpp"
#include "gui/logging/errors.hpp"
#include "gui/themes.hpp"
#include "jsonlib.hpp"

namespace Toolbox::UI {
    ConfigTheme::ConfigTheme(std::string_view name, const ImGuiStyle &theme)
        : m_name(name), m_style(theme), m_load_ok(true) {}

    ConfigTheme::ConfigTheme(std::string_view name) : m_name(name) {}

    bool ConfigTheme::apply() {
        if (!m_load_ok)
            return false;
        auto &style = ImGui::GetStyle();
        for (size_t i = 0; i < ImGuiCol_COUNT; ++i) {
            style.Colors[i] = m_style.Colors[i];
        }
        return true;
    }

#define THEME_SAVE_COLOR(style, theme_color, j)                                                    \
    saveColor(j, TOOLBOX_STRINGIFY_MACRO(theme_color), style.Colors[theme_color])

    Result<void, SerialError> ConfigTheme::serialize(Serializer &out) const {
        json_t theme_json;

        auto result = tryJSON(theme_json, [this, &out](json_t &j) {
            THEME_SAVE_COLOR(m_style, ImGuiCol_Text, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TextDisabled, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_WindowBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ChildBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_PopupBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_Border, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_BorderShadow, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_FrameBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_FrameBgHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_FrameBgActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TitleBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TitleBgActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TitleBgCollapsed, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_MenuBarBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ScrollbarBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ScrollbarGrab, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ScrollbarGrabHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ScrollbarGrabActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_CheckMark, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_SliderGrab, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_SliderGrabActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_Button, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ButtonHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ButtonActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_Header, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_HeaderHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_HeaderActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_Separator, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_SeparatorHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_SeparatorActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ResizeGrip, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ResizeGripHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ResizeGripActive, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TabHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_Tab, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TabSelected, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TabSelectedOverline, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TabDimmed, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TabDimmedSelected, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TabDimmedSelectedOverline, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_DockingPreview, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_DockingEmptyBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_PlotLines, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_PlotLinesHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_PlotHistogram, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_PlotHistogramHovered, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TableHeaderBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TableBorderStrong, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TableBorderLight, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TableRowBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TableRowBgAlt, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TextLink, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_TextSelectedBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_DragDropTarget, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_NavHighlight, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_NavWindowingHighlight, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_NavWindowingDimBg, j);
            THEME_SAVE_COLOR(m_style, ImGuiCol_ModalWindowDimBg, j);

            out.stream() << j;
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           out.filepath());
        }

        return Result<void, SerialError>();
    }

#undef THEME_SAVE_COLOR

#define THEME_LOAD_COLOR(style, theme_color, j)                                                    \
    loadColor(j, TOOLBOX_STRINGIFY_MACRO(theme_color), style.Colors[theme_color])

    Result<void, SerialError> ConfigTheme::deserialize(Deserializer &in) {
        json_t theme_json;

        auto result = tryJSON(theme_json, [this, &in](json_t &j) {
            in.stream() >> j;

            THEME_LOAD_COLOR(m_style, ImGuiCol_Text, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TextDisabled, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_WindowBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ChildBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_PopupBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_Border, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_BorderShadow, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_FrameBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_FrameBgHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_FrameBgActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TitleBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TitleBgActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TitleBgCollapsed, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_MenuBarBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ScrollbarBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ScrollbarGrab, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ScrollbarGrabHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ScrollbarGrabActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_CheckMark, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_SliderGrab, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_SliderGrabActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_Button, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ButtonHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ButtonActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_Header, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_HeaderHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_HeaderActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_Separator, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_SeparatorHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_SeparatorActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ResizeGrip, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ResizeGripHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ResizeGripActive, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TabHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_Tab, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TabSelected, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TabSelectedOverline, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TabDimmed, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TabDimmedSelected, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TabDimmedSelectedOverline, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_DockingPreview, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_DockingEmptyBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_PlotLines, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_PlotLinesHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_PlotHistogram, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_PlotHistogramHovered, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TableHeaderBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TableBorderStrong, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TableBorderLight, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TableRowBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TableRowBgAlt, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TextLink, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_TextSelectedBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_DragDropTarget, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_NavHighlight, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_NavWindowingHighlight, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_NavWindowingDimBg, j);
            THEME_LOAD_COLOR(m_style, ImGuiCol_ModalWindowDimBg, j);
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           in.filepath());
        }

        m_load_ok = true;
        return Result<void, SerialError>();
    }

#undef THEME_LOAD_COLOR

    void ConfigTheme::loadColor(const json_t &j, const std::string &name, ImVec4 &color) {
        const json_t &color_entry = j[name];
        if (color_entry.is_array() && color_entry.size() == 4) {
            if (color_entry[0].is_number() && color_entry[1].is_number() &&
                color_entry[2].is_number() && color_entry[3].is_number()) {
                color.x = color_entry[0];
                color.y = color_entry[1];
                color.z = color_entry[2];
                color.w = color_entry[3];
            } else {
                TOOLBOX_ERROR_V("[THEME] JSON entry for theme {} has corrupted entry {}", m_name,
                                name);
            }
        } else {
            TOOLBOX_ERROR_V("[THEME] JSON entry for theme {} has corrupted entry {}", m_name, name);
        }
    }

    void ConfigTheme::saveColor(json_t &j, const std::string &name, const ImVec4 &color) const {
        std::array<float, 4> color_rgba = {color.x, color.y, color.z, color.w};
        j[name]                         = color_rgba;
    }

    Result<void, FSError> ThemeManager::initialize() {
        const ResourceManager &manager = GUIApplication::instance().getResourceManager();
        UUID64 themes_uuid             = ResourceManager::getResourcePathUUID("Themes");
        if (!manager.hasResourcePath(themes_uuid)) {
            return make_fs_error<void>(std::error_code(), {"Themes not found"});
        }

        for (auto &subpath : manager.walkIterator(themes_uuid)) {
            try {
                std::ifstream in_str;
                manager.getSerialData(in_str, subpath, themes_uuid).and_then([&]() {
                    Deserializer in(in_str.rdbuf());

                    RefPtr<ConfigTheme> theme =
                        make_referable<ConfigTheme>(subpath.path().stem().string());

                    theme->deserialize(in).or_else([](const SerialError &error) {
                        LogError(error);
                        return Result<void, SerialError>();
                    });

                    if (theme->name() == "Default") {
                        theme->apply();
                        m_active_theme = m_themes.size();
                    }
                    m_themes.emplace_back(theme);

                    return Result<std::ifstream, FSError>();
                });
            } catch (std::runtime_error &e) {
                return make_fs_error<void>(std::error_code(), {e.what()});
            }
        }

        return {};
    }

}  // namespace Toolbox::UI