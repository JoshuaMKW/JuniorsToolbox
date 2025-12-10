#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "gui/window.hpp"

namespace Toolbox::UI {

    class ImModal : public ImWindow {
    protected:
        ImGuiID onBuildDockspace() override final { return std::numeric_limits<ImGuiID>::max(); }
        void onRenderMenuBar() override final {}

        bool onBeginWindow(const std::string &window_name, bool *is_open,
                           ImGuiWindowFlags flags) override final;
        void onEndWindow(bool did_render) override final;

    public:
        explicit ImModal(const std::string &name) : ImWindow(name) {}
        ImModal(const std::string &name, std::optional<ImVec2> default_size)
            : ImWindow(name, default_size) {}
        ImModal(const std::string &name, std::optional<ImVec2> min_size,
                std::optional<ImVec2> max_size)
            : ImWindow(name, min_size, max_size) {}
        ImModal(const std::string &name, std::optional<ImVec2> default_size,
                std::optional<ImVec2> min_size, std::optional<ImVec2> max_size)
            : ImWindow(name, default_size, min_size, max_size) {}
        ImModal(const std::string &name, std::optional<ImVec2> default_size,
                std::optional<ImVec2> min_size, std::optional<ImVec2> max_size,
                ImGuiWindowClass window_class)
            : ImWindow(name, default_size, min_size, max_size, window_class) {}

        ~ImModal() override = default;

        const ImGuiWindowClass *windowClass() const override final {
            return m_parent ? m_parent->windowClass() : &m_window_class;
        }

        ImGuiWindowFlags flags() const override final {
            return ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse;
        }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override final {
            return false;
        }
        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override final {
            return false;
        }
    };

    inline std::string ImModalComponentTitle(const ImModal &window_layer,
                                             const std::string &component_name) {
        return std::format("{}##{}-modal", component_name, window_layer.getUUID());
    }

}  // namespace Toolbox::UI