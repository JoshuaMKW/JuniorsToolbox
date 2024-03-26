#include "core/application/application.hpp"

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

            const bool good_exec = update();
            if (!good_exec)
                break;

            onUpdate(m_delta_time);

            m_frame_counter += 1;
            m_last_frame_time = this_frame_time;
        }

        teardown();
    }

    void CoreApplication::stop() { m_is_running = false; }

    bool CoreApplication::update() {
        // TODO: Add logic to pass through logical layers
        // and compare the UUID of the event to the layer UUID
        for (auto &layer : m_layers) {
            layer->onUpdate(m_delta_time);
        }

        // TODO: improve fail response
        return m_exit_code == 0;
    }

    void CoreApplication::setup(int argc, const char **argv) {
        m_is_running = true;
        onInit(argc, argv);
        for (auto &layer : m_layers) {
            layer->onAttach();
        }
    }

    void CoreApplication::teardown() {
        for (auto &layer : m_layers) {
            layer->onDetach();
        }
        onExit();
    }

    void CoreApplication::onEvent(RefPtr<BaseEvent> ev) {
        // TODO: Add logic to pass through logical layers
        // and compare the UUID of the event to the layer UUID
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
