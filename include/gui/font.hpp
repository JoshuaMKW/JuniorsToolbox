#pragma once

#include <filesystem>
#include <imgui.h>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "IconsForkAwesome.h"
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

        std::set<float> fontSizes() {
            static std::set<float> s_font_sizes = {10.0f, 12.0f, 14.0f, 16.0f,
                                                      18.0f, 24.0f, 32.0f};
            return s_font_sizes;
        }

        std::set<std::string> fontFamilies() {
            std::set<std::string> unique_families;
            for (const auto &elem : m_loaded_fonts) {
                unique_families.insert(elem.first);
            }
            return unique_families;
        }

        bool initialize();
        bool addFontOTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                        const ImWchar *glyph_ranges);
        bool addFontTTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                        const ImWchar *glyph_ranges);
        void finalize();

        ImFont *getFont(std::string_view name, float size);

        ImFont *getCurrentFont() { return getFont(m_current_name, m_current_size); }
        std::string getCurrentFontFamily() { return m_current_name; }
        float getCurrentFontSize() { return m_current_size; }

        void setCurrentFont(std::string_view name, float size) {
            m_current_name = name;
            m_current_size = size;
        }
        void setCurrentFontFamily(std::string_view name) { m_current_name = name; }
        void setCurrentFontSize(float size) { m_current_size = size; }

    private:
        bool addFont(const std::filesystem::path &font_path, const ImFontConfig *font_cfg_template,
                     const ImWchar *glyph_ranges);
    };

}  // namespace Toolbox::UI