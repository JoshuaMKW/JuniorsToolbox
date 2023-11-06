#include "gui/font.hpp"

namespace Toolbox::UI {

    bool FontManager::initialize() {
        auto cwd_result = Toolbox::current_path();
        if (!cwd_result) {
            return false;
        }

        auto font_path   = cwd_result.value() / "Fonts";
        auto path_result = Toolbox::is_directory(font_path);
        if (!path_result || !path_result.value()) {
            return false;
        }

        ImGuiIO &io = ImGui::GetIO();

        for (auto &child_path : std::filesystem::directory_iterator{font_path}) {
            if (child_path.path().stem().string() == "forkawesome") {
                continue;
            }
            addFont(child_path.path(), nullptr, io.Fonts->GetGlyphRangesJapanese());
        }
    }

    bool FontManager::addFontOTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                                 const ImWchar *glyph_ranges) {
        auto cwd_result = Toolbox::current_path();
        if (!cwd_result) {
            return false;
        }

        auto font_path = cwd_result.value() / "Fonts" / std::format("{}.otf", name);
        return addFont(font_path, font_cfg_template, glyph_ranges);
    }

    bool FontManager::addFontTTF(const std::string_view name, const ImFontConfig *font_cfg_template,
                                 const ImWchar *glyph_ranges) {
        auto cwd_result = Toolbox::current_path();
        if (!cwd_result) {
            return false;
        }

        auto font_path = cwd_result.value() / "Fonts" / std::format("{}.ttf", name);
        return addFont(font_path, font_cfg_template, glyph_ranges);
    }

    void FontManager::finalize() { ImGui::GetIO().Fonts->Build(); }

    ImFont *FontManager::getFont(std::string_view name, float size) {
        for (auto font = m_loaded_fonts.find(std::string(name)); font != m_loaded_fonts.end();
             font++) {
            if (font->second->FontSize == size) {
                return font->second;
            }
        }
    }

    bool FontManager::addFont(const std::filesystem::path &font_path,
                              const ImFontConfig *font_cfg_template, const ImWchar *glyph_ranges) {
        auto cwd_result = Toolbox::current_path();
        if (!cwd_result) {
            return false;
        }

        auto fork_awesome_path = cwd_result.value() / "Fonts" / "forkawesome.ttf";

        ImFontConfig font_cfg = font_cfg_template ? *font_cfg_template : ImFontConfig();
        ImFontConfig fork_awesome_cfg;
        fork_awesome_cfg.MergeMode  = true;
        fork_awesome_cfg.PixelSnapH = true;

        static const ImWchar icons_ranges[] = {ICON_MIN_FK, ICON_MAX_16_FK, 0};

        for (auto &size : fontSizes()) {
            ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.string().c_str(),
                                                                    size, &font_cfg, glyph_ranges);
            if (!font) {
                return false;
            }

            ImGui::GetIO().Fonts->AddFontFromFileTTF(fork_awesome_path.string().c_str(), size,
                                                     &fork_awesome_cfg, icons_ranges);

            m_loaded_fonts.insert({font_path.stem().string(), font});
        }
        return true;
    }
}  // namespace Toolbox::UI