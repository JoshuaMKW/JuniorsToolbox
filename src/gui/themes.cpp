#include <fstream>

#include "fsystem.hpp"
#include "gui/themes.hpp"

namespace Toolbox::UI {
    ConfigTheme::ConfigTheme(std::string_view name) : m_name(name) {
        auto cwd_result = Toolbox::Filesystem::current_path();
        if (!cwd_result) {
            return;
        }
        loadFromFile(cwd_result.value() / "Themes" / std::format("{}.theme", name));
    }

    ConfigTheme::ConfigTheme(std::string_view name, const ImGuiStyle &theme)
        : m_name(name), m_style(theme), m_load_ok(true) {}

    bool ConfigTheme::apply() {
        if (!m_load_ok || true)
            return false;
        auto &style = ImGui::GetStyle();
        for (size_t i = 0; i < ImGuiCol_COUNT; ++i) {
            style.Colors[i] = m_style.Colors[i];
        }
        return true;
    }

    void ConfigTheme::loadFromFile(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary | std::ios::in);

        in.read(reinterpret_cast<char *>(&m_style), sizeof(m_style));

        m_load_ok = true;
    }

    void ConfigTheme::saveToFile(const std::filesystem::path &path) {
        std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::ate);

        out.write(reinterpret_cast<const char *>(&m_style), sizeof(m_style));
    }

    ThemeManager &ThemeManager::instance() {
        static ThemeManager instance_;
        return instance_;
    }

    Result<void, FSError> ThemeManager::initialize() {
        return Toolbox::Filesystem::current_path().and_then([this](std::filesystem::path &&cwd) {
            for (auto &subpath : Toolbox::Filesystem::directory_iterator{cwd / "Themes"}) {
                try {
                    auto theme = make_referable<ConfigTheme>(subpath.path().stem().string());
                    if (theme->name() == "Default") {
                        theme->apply();
                        m_active_theme = m_themes.size();
                    }
                    m_themes.emplace_back(theme);
                } catch (std::runtime_error &e) {
                    return make_fs_error<void>(std::error_code(), {e.what()});
                }
            }
            return Result<void, FSError>();
        });
    }

}  // namespace Toolbox::UI