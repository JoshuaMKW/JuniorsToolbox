#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/event/event.hpp"
#include "gui/window.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class DolphinOverlay final : public ImWindow {
    private:
        class DolphinOverlayZUpdater : public Threaded<void> {
        public:
            DolphinOverlayZUpdater() = default;

            void setWindow(Platform::LowWindow window) { m_window = window; }
            void setTarget(Platform::LowWindow target) { m_target = target; }

        protected:
            void tRun(void *param) override;

        private:
            std::atomic<Platform::LowWindow> m_window = nullptr;
            std::atomic<Platform::LowWindow> m_target = nullptr;
        };

    public:
        using render_layer_cb = std::function<void(TimeStep delta_time, std::string_view layer_name,
                                                   int width, int height, UUID64 window_uuid)>;

        DolphinOverlay();
        ~DolphinOverlay();

        void registerRenderLayer(const std::string &layer_name, render_layer_cb cb) {
            m_render_layers[layer_name] = cb;
        }

        void deregisterRenderLayer(const std::string &layer_name) {
            m_render_layers.erase(layer_name);
        }

    protected:
        void onRenderBody(TimeStep delta_time) override;

    public:
        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav;
        }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow           = ImGui::GetCurrentWindow();
            m_window_class.ClassId               = (ImGuiID)getUUID();
            m_window_class.ParentViewportId      = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed = false;
            m_window_class.DockingAlwaysTabBar   = false;
            return nullptr;
        }

        [[nodiscard]] std::string context() const override { return ""; }

        void onAttach() override;
        void onDetach() override;
        void onImGuiUpdate(TimeStep delta_time) override;

    private:
        bool m_is_dolphin_attached           = false;
        Platform::LowWindow m_dolphin_window = nullptr;
        std::map<std::string, render_layer_cb> m_render_layers;

        DolphinOverlayZUpdater m_z_updater;
    };

}  // namespace Toolbox::UI