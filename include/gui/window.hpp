#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "clone.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "serial.hpp"
#include <imgui.h>

namespace Toolbox::UI {

    class IWindow {
    protected:
        virtual void renderMenuBar()            = 0;
        virtual void renderBody(f32 delta_time) = 0;

    public:
        [[nodiscard]] virtual std::string context() const = 0;
        [[nodiscard]] virtual std::string name() const    = 0;
        [[nodiscard]] virtual bool unsaved() const        = 0;

        // Returns the supported file type, or empty if designed for a folder.
        [[nodiscard]] virtual std::vector<std::string> extensions() const = 0;

        [[nodiscard]] virtual bool loadData(const std::filesystem::path &path) = 0;
        [[nodiscard]] virtual bool saveData(const std::filesystem::path &path) = 0;

        virtual bool update(f32 delta_time) = 0;

        [[nodiscard]] std::string title() const {
            std::string t = std::format("{} - {}", name(), context());
            if (unsaved())
                t += " (*)";
            return t;
        }

        void render(f32 delta_time) {
            ImGui::Begin(title().c_str(), &m_is_open, m_flags);
            {
                renderMenuBar();
                renderBody(delta_time);
            }
            ImGui::End();
        }

    private:
        bool m_is_open = false;
        ImGuiWindowFlags m_flags = 0;
    };

}  // namespace Toolbox::UI