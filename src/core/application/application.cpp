#include "core/application/application.hpp"

#include <algorithm>
#include <execution>
#include <iostream>
#include <string>
#include <thread>

#include "core/core.hpp"
#include "core/input/input.hpp"

// void ImGuiSetupTheme(bool, float);

namespace Toolbox {

    static std::thread::id s_main_thread_id;

    std::thread::id CoreApplication::GetMainThreadId() { return s_main_thread_id; }

    CoreApplication::CoreApplication() : CoreApplication("") {}

    CoreApplication::CoreApplication(const std::string &app_name) : m_app_name(app_name) {
        s_main_thread_id  = std::this_thread::get_id();
        m_delta_time      = TimeStep();
        m_last_frame_time = GetTime();
    }

    void CoreApplication::run(int argc, const char **argv) {
        setup(argc, argv);

        while (m_is_running) {
            TimePoint this_frame_time = GetTime();
            m_delta_time              = TimeStep(m_last_frame_time, this_frame_time);

            onUpdate(m_delta_time);

            // Process delayed events
            m_event_mutex.lock();
            std::for_each(std::execution::par, m_events.begin(), m_events.end(),
                          [&](auto &ev) { onEvent(ev); });
            m_events.clear();
            m_event_mutex.unlock();

            m_frame_counter += 1;
            m_last_frame_time = this_frame_time;

            if (m_exit_code != 0) {
                stop();
            }
        }

        teardown();
    }

    void CoreApplication::stop() { m_is_running = false; }

    void CoreApplication::setup(int argc, const char **argv) {
        m_is_running = true;
        onInit(argc, argv);
    }

    void CoreApplication::teardown() {
        for (auto &layer : m_layers) {
            layer->onDetach();
        }
        onExit();
    }

    void CoreApplication::onEvent(RefPtr<BaseEvent> ev) {
        for (auto &layer : m_layers) {
            layer->onEvent(ev);
        }
        if (!ev->isHandled()) {
            TOOLBOX_DEBUG_LOG_V("[EVENT] Unhandled event of TypeID {}", ev->getType());
        }
    }

    void CoreApplication::addLayer(RefPtr<ProcessLayer> layer) {
        m_layers.push_back(layer);
        layer->onAttach();
    }

    void CoreApplication::removeLayer(RefPtr<ProcessLayer> layer) {
        auto it = std::find(m_layers.begin(), m_layers.end(), layer);
        if (it != m_layers.end()) {
            m_layers.erase(it);
            layer->onDetach();
        }
    }

}  // namespace Toolbox
