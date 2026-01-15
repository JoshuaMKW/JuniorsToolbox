#include "gui/appmain/dolphin/overlay.hpp"
#include "gui/appmain/application.hpp"

namespace Toolbox::UI {

    DolphinOverlay::DolphinOverlay() : ImWindow("Dolphin Overlay") {}

    DolphinOverlay::~DolphinOverlay() {}

    void DolphinOverlay::onRenderBody(TimeStep delta_time) {
        if (!m_is_dolphin_attached) {
            return;
        }

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        Platform::LowWindow window_handle =
            reinterpret_cast<Platform::LowWindow>(window->Viewport->PlatformHandleRaw);

        m_z_updater.setWindow(window_handle);
        m_z_updater.setTarget(m_dolphin_window);

        ImVec2 pos = ImGui::GetCursorPos();
        for (const auto &[layer_name, render_layer] : m_render_layers) {
            render_layer(delta_time, std::string_view(layer_name), ImGui::GetWindowWidth(),
                         ImGui::GetWindowHeight(), getUUID());
            ImGui::SetCursorPos(pos);
        }
    }

    void DolphinOverlay::onAttach() {
        m_z_updater.tStart(false, nullptr);
    }

    void DolphinOverlay::onDetach() { m_z_updater.tKill(true); }

    void DolphinOverlay::onImGuiUpdate(TimeStep delta_time) {
        m_is_dolphin_attached = false;

        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            return;
        }

        const Platform::ProcessInformation &proc_info = communicator.manager().getProcess();
        if (proc_info.m_process_id == std::numeric_limits<Platform::ProcessID>::max()) {
            return;
        }

        std::vector<Platform::LowWindow> dolphin_windows =
            Platform::FindWindowsOfProcess(proc_info);
        auto window_it = std::find_if(
            dolphin_windows.begin(), dolphin_windows.end(), [](const Platform::LowWindow &window) {
                std::string title = Platform::GetWindowTitle(window);
                return title.starts_with("Dolphin") && title.find("GMS") != std::string::npos;
            });

        if (window_it == dolphin_windows.end()) {
            return;
        }

        m_dolphin_window = *window_it;
        int x, y, width, height;
        if (!Platform::GetWindowClientRect(m_dolphin_window, x, y, width, height)) {
            return;
        }

        setPos(ImVec2(static_cast<float>(x), static_cast<float>(y)));
        setSize(ImVec2(static_cast<float>(width), static_cast<float>(height)));

        m_is_dolphin_attached = true;
    }

    void DolphinOverlay::DolphinOverlayZUpdater::tRun(void *param) {
        while (!tIsSignalKill()) {
            if (m_window && m_target)
                Platform::ForceWindowToFront(m_window, m_target);
            else if (m_window)
                Platform::ForceWindowToFront(m_window);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

}  // namespace Toolbox::UI
