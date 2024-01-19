#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "smart_resource.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"

#include "gui/clipboard.hpp"
#include "gui/logging/logger.hpp"
#include "gui/property/property.hpp"
#include "gui/window.hpp"

#include <gui/context_menu.hpp>
#include <imgui.h>

namespace Toolbox::UI {

    class LoggingWindow final : public SimpleWindow {
    public:
        LoggingWindow() {
            Log::AppLogger::instance().setLogCallback(appendMessageToPool);
            Log::AppLogger::instance().debugLog("Logger successfully started!");
        }
        ~LoggingWindow() = default;

        ImGuiWindowFlags flags() const override { return SimpleWindow::flags() | ImGuiWindowFlags_MenuBar; }

    protected:
        static void appendMessageToPool(const Log::AppLogger::LogMessage &message);

        void renderMenuBar() override;
        void renderBody(f32 delta_time) override;

    public:
        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow              = ImGui::GetCurrentWindow();
            m_window_class.ClassId                  = 0;
            m_window_class.ParentViewportId         = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed    = false;
            m_window_class.DockingAlwaysTabBar      = false;
            m_window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
            return &m_window_class;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {600, 500}
            };
        }
        std::optional<ImVec2> maxSize() const override { return std::nullopt; }

        [[nodiscard]] std::string context() const override { return ""; }
        [[nodiscard]] std::string name() const override { return "Application Log"; }
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool loadData(const std::filesystem::path &path) override { return false; }
        [[nodiscard]] bool saveData(std::optional<std::filesystem::path> path) override { return false; }

        bool update(f32 delta_time) override;
        bool postUpdate(f32 delta_time) override { return true; }

    private:
        Log::ReportLevel m_logging_level = Log::ReportLevel::INFO;
        uint32_t m_dock_space_id = 0;
    };
}  // namespace Toolbox::UI