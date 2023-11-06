#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include <imgui.h>

#include "fsystem.hpp"

namespace Toolbox::UI {

    class ITheme {
    public:
        virtual ~ITheme() = default;

        virtual std::string_view name() const = 0;
        virtual bool apply()                  = 0;
    };

    class ConfigTheme : public ITheme {
    public:
        ConfigTheme(std::string_view name);
        ConfigTheme(std::string_view name, const ImGuiStyle &theme);
        ~ConfigTheme() override = default;

        std::string_view name() const override { return m_name; }
        bool apply() override;

        void saveToFile(const std::filesystem::path &path);
    protected:
        void loadFromFile(const std::filesystem::path &path);

    private:
        bool m_load_ok = false;
        std::string m_name;
        ImGuiStyle m_style;
    };

    class ThemeManager {
    public:
        ThemeManager() = default;
        ~ThemeManager() = default;

        static ThemeManager &instance() {
            static ThemeManager instance_;
            return instance_;
        }

        void addTheme(std::shared_ptr<ITheme> theme) { m_themes.push_back(theme); }
        std::vector<std::shared_ptr<ITheme>> themes() const { return m_themes; }

        void applyTheme(std::string_view name) {
            for (size_t i = 0; i < m_themes.size(); ++i) {
                auto theme = m_themes.at(i);
                if (theme->name() == name) {
                    theme->apply();
                    m_active_theme = i;
                    break;
                }
            }
        }

        size_t getActiveThemeIndex() const { return m_active_theme; }

        std::expected<void, FSError> initialize();

    private:
        size_t m_active_theme = 0;
        std::vector<std::shared_ptr<ITheme>> m_themes;
    };

}  // namespace Toolbox::UI