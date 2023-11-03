#pragma once

#include <filesystem>
#include <imgui.h>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "fsystem.hpp"

namespace Toolbox::UI {

    class FontManager {
        std::multimap<std::string, ImFont *> m_loaded_fonts;
        std::string m_current_name;
        float m_current_size;

    public:
        static FontManager &instance() {
            static FontManager _inst;
            return _inst;
        }

        std::vector<float> fontSizes() {
            static std::vector<float> s_font_sizes = {10.0f,  12.0f, 14.0f, 16.0f,
                                                      18.0f, 24.0f, 32.0f};
            return s_font_sizes;
        }

        bool addFontOTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                        const ImWchar *glyph_ranges) {
            auto cwd_result = Toolbox::current_path();
            if (!cwd_result) {
                return false;
            }

            auto font_path   = cwd_result.value() / "res" / std::format("{}.otf", name);
            ImFontConfig cfg = font_cfg_template ? *font_cfg_template : ImFontConfig();

            for (auto &size : fontSizes()) {
                ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.string().c_str(),
                                                                        size, &cfg, glyph_ranges);
                if (!font) {
                    return false;
                }

                m_loaded_fonts.insert({std::string(name), font});
            }
            return true;
        }

        bool addFontTTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                        const ImWchar *glyph_ranges) {
            auto cwd_result = Toolbox::current_path();
            if (!cwd_result) {
                return false;
            }

            auto font_path   = cwd_result.value() / "res" / std::format("{}.ttf", name);
            ImFontConfig cfg = font_cfg_template ? *font_cfg_template : ImFontConfig();

            for (auto &size : fontSizes()) {
                cfg.GlyphMinAdvanceX = size;
                ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.string().c_str(),
                                                                        size, &cfg, glyph_ranges);
                if (!font) {
                    return false;
                }

                m_loaded_fonts.insert({std::string(name), font});
            }
            return true;
        }

        void build() { ImGui::GetIO().Fonts->Build(); }

        ImFont *getFont(std::string_view name, float size) {
            for (auto font = m_loaded_fonts.find(std::string(name));
                 font != m_loaded_fonts.end(); font++) {
                if (font->second->FontSize == size) {
                    return font->second;
                }
            }
        }
        ImFont *getCurrentFont() { return getFont(m_current_name, m_current_size); }

        void setCurrentFontSize(float size) { m_current_size = size; }
        void setCurrentFont(std::string_view name, float size) {
            m_current_name = name;
            m_current_size = size;
        }
    };

}  // namespace Toolbox::UI