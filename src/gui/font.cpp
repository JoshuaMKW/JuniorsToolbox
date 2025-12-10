#include "gui/font.hpp"

namespace Toolbox::UI {

    bool FontManager::initialize() {
        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return false;
        }

        auto font_path   = cwd_result.value() / "Fonts";
        auto path_result = Toolbox::Filesystem::is_directory(font_path);
        if (!path_result || !path_result.value()) {
            return false;
        }

        ImGuiIO &io = ImGui::GetIO();

        for (auto &child_path : std::filesystem::directory_iterator{font_path}) {
            if (!Filesystem::is_regular_file(child_path).value_or(false)) {
                continue;
            }
            if (child_path.path().stem().string() == "forkawesome") {
                continue;
            }
            addFont(child_path.path(), nullptr, io.Fonts->GetGlyphRangesJapanese());
        }
        return true;
    }

    bool FontManager::addFontOTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                                 const ImWchar *glyph_ranges) {
        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return false;
        }

        auto font_path = cwd_result.value() / "Fonts" / std::format("{}.otf", name);
        return addFont(font_path, font_cfg_template, glyph_ranges);
    }

    bool FontManager::addFontTTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                                 const ImWchar *glyph_ranges) {
        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return false;
        }

        auto font_path = cwd_result.value() / "Fonts" / std::format("{}.ttf", name);
        return addFont(font_path, font_cfg_template, glyph_ranges);
    }

    void FontManager::finalize() { ImGui::GetIO().Fonts->Build(); }

#if USE_LEGACY_FONT_API
    ImFont *FontManager::getFont(std::string_view name, float size) const {
        for (auto font = m_loaded_fonts.find(std::string(name)); font != m_loaded_fonts.end();
             font++) {
            if (font->second->FontSize == size) {
                return font->second;
            }
        }
        return nullptr;
    }
#else
    ImFont *FontManager::getFont(std::string_view name) const {
        for (auto font = m_loaded_fonts.find(std::string(name)); font != m_loaded_fonts.end();
             font++) {
            return font->second;
        }
        return nullptr;
    }
#endif

    bool FontManager::addFont(const std::filesystem::path &font_path,
                              const ImFontConfig *font_cfg_template, const ImWchar *glyph_ranges) {
        if (!Filesystem::is_regular_file(font_path).value_or(false)) {
            return false;
        }

        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return false;
        }

        auto fork_awesome_path = cwd_result.value() / "Fonts" / "forkawesome.ttf";

        ImFontConfig font_cfg = font_cfg_template ? *font_cfg_template : ImFontConfig();
        ImFontConfig fork_awesome_cfg;
        fork_awesome_cfg.MergeMode  = true;
        fork_awesome_cfg.PixelSnapH = true;

        static const ImWchar icons_ranges[] = {ICON_MIN_FK, ICON_MAX_16_FK, 0};

#if USE_LEGACY_FONT_API
        for (auto &size : fontSizes()) {
            ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.string().c_str(),
                                                                    size, &font_cfg, glyph_ranges);
            if (!font) {
                return false;
            }

            font->FontSize = size;

            ImGui::GetIO().Fonts->AddFontFromFileTTF(fork_awesome_path.string().c_str(), size,
                                                     &fork_awesome_cfg, icons_ranges);

            m_loaded_fonts.insert({font_path.stem().string(), font});
        }
#else
        font_cfg.GlyphExcludeRanges = icons_ranges;
        ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.string().c_str(), 0.0f,
                                                                &font_cfg, glyph_ranges);
        if (!font) {
            return false;
        }

        ImGui::GetIO().Fonts->AddFontFromFileTTF(fork_awesome_path.string().c_str(), 0.0f,
                                                 &fork_awesome_cfg, icons_ranges);

        m_loaded_fonts.insert({font_path.stem().string(), font});
#endif
        return true;
    }

#if USE_LEGACY_FONT_API
    void FontManager::setCurrentFont(std::string_view name, float size) {
        m_current_font_family      = name;
        m_current_font_size        = size;
        ImGui::GetIO().FontDefault = getCurrentFont();
    }

    void FontManager::setCurrentFontFamily(std::string_view name) {
        m_current_font_family      = name;
        ImGui::GetIO().FontDefault = getCurrentFont();
    }

    void FontManager::setCurrentFontSize(float size) {
        m_current_font_size        = size;
        ImGui::GetIO().FontDefault = getCurrentFont();
    }
#else
    void FontManager::setCurrentFont(std::string_view name, float size) {
        m_current_font_family = name;
        m_current_font_size   = size;
    }

    void FontManager::setCurrentFontFamily(std::string_view name) {
        m_current_font_family = name;
    }

    void FontManager::setCurrentFontSize(float size) {
        m_current_font_size = size;
    }
#endif
}  // namespace Toolbox::UI