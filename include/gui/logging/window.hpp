#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "core/log.hpp"
#include "core/types.hpp"
#include "gui/context_menu.hpp"
#include "gui/property/property.hpp"
#include "gui/window.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class LoggingWindow final : public ImWindow {
    protected:
        void appendMessageToPool(const Log::AppLogger::LogMessage &message);

    public:
        LoggingWindow(const std::string &name) : ImWindow(name) {
            TOOLBOX_LOG_CALLBACK(TOOLBOX_BIND_EVENT_FN(LoggingWindow::appendMessageToPool));
            TOOLBOX_INFO("Logger successfully started!");
        }
        ~LoggingWindow() = default;

        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_MenuBar;
        }

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
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override { return false; }
        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override {
            return false;
        }

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

    private:
        Log::ReportLevel m_logging_level = Log::ReportLevel::REPORT_INFO;
        uint32_t m_dock_space_id         = 0;
        bool m_scroll_requested          = false;
    };
}  // namespace Toolbox::UI