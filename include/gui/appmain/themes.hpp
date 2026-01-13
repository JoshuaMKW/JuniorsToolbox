#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include <imgui.h>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "serial.hpp"

namespace Toolbox::UI {

    class ITheme : public ISerializable {
    public:
        virtual std::string_view name() const = 0;
        virtual bool apply()                  = 0;
    };

    class ConfigTheme final : public ITheme {
    public:
        using json_t = nlohmann::json;

        ConfigTheme() = delete;
        ConfigTheme(std::string_view name);
        ConfigTheme(std::string_view name, const ImGuiStyle &theme);

        ~ConfigTheme() = default;

        std::string_view name() const override { return m_name; }
        bool apply() override;

        Result<void, SerialError> serialize(Serializer &out) const;
        Result<void, SerialError> deserialize(Deserializer &in);

    protected:
        void loadColor(const json_t &j, const std::string &name, ImVec4 &color);
        void saveColor(json_t &j, const std::string &name, const ImVec4 &color) const;

    private:
        bool m_load_ok = false;
        std::string m_name;
        ImGuiStyle m_style;
    };

    class ThemeManager {
    public:
        ThemeManager() = default;
        ~ThemeManager() = default;

        void addTheme(RefPtr<ITheme> theme) { m_themes.push_back(theme); }
        std::vector<RefPtr<ITheme>> themes() const { return m_themes; }

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

        Result<void, FSError> initialize();

    private:
        size_t m_active_theme = 0;
        std::vector<RefPtr<ITheme>> m_themes;
    };

}  // namespace Toolbox::UI