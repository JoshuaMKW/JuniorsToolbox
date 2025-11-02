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
#include "gui/settings.hpp"

#define USE_LEGACY_FONT_API 0

namespace Toolbox::UI {

    class FontManager {
        std::string m_current_font_family;
        float m_current_font_size;
        std::multimap<std::string, ImFont *> m_loaded_fonts;

    public:
#if USE_LEGACY_FONT_API
        std::set<float> fontSizes() {
            static std::set<float> s_font_sizes = {12.0f, 16.0f, 24.0f};
            return s_font_sizes;
        }
#else
        constexpr float getFontSizeMin() const noexcept { return 8.0f; }
        constexpr float getFontSizeMax() const noexcept { return 32.0f; }
#endif

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

#if USE_LEGACY_FONT_API
        ImFont *getFont(std::string_view name, float size) const;

        ImFont *getCurrentFont() const {
            return getFont(m_current_font_family, m_current_font_size);
        }
#else
        ImFont *getFont(std::string_view name) const;

        ImFont *getCurrentFont() const { return getFont(m_current_font_family); }
#endif

        std::string getCurrentFontFamily() const { return m_current_font_family; }

        float getCurrentFontSize() const {
            return std::clamp(m_current_font_size, getFontSizeMin(), getFontSizeMax());
        }

        void setCurrentFont(std::string_view name, float size);
        void setCurrentFontFamily(std::string_view name);
        void setCurrentFontSize(float size);

    private:
        bool addFont(const std::filesystem::path &font_path, const ImFontConfig *font_cfg_template,
                     const ImWchar *glyph_ranges);
    };

}  // namespace Toolbox::UI