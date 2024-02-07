#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/application.hpp"
#include "core/core.hpp"
#include "core/types.hpp"

#include "game/task.hpp"

#include "gui/settings.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    void TaskCommunicator::run() {
        while (!m_kill_flag.load()) {
            AppSettings &settings = SettingsManager::instance().getCurrentProfile();

            DolphinCommunicator &communicator =
                MainApplication::instance().getDolphinCommunicator();

            while (!m_task_queue.empty()) {
                std::unique_lock<std::mutex> lk(m_mutex);
                std::function<bool(DolphinCommunicator &)> task = m_task_queue.front();
                if (task(communicator)) {
                    m_task_queue.pop();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
        }

        m_kill_condition.notify_all();
    }

}  // namespace Toolbox::Game