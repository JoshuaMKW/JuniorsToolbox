#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <imgui.h>

#include "core/core.hpp"
#include "core/types.hpp"

#include "dolphin/process.hpp"

#include "gui/settings.hpp"

using namespace Toolbox;
using namespace Toolbox::UI;

namespace Toolbox::Dolphin {

    void DolphinCommunicator::run() {
        while (!m_kill_flag.load()) {
            AppSettings &settings = SettingsManager::instance().getCurrentProfile();

            if (!m_hook_flag.load()) {
#if 0
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
#endif
            }

            {
                std::unique_lock<std::mutex> lk(m_mutex);
                DolphinHookManager &manager = DolphinHookManager::instance();
                auto result                 = manager.refresh();
                if (!result) {
                    logError(result.error());
                }
                m_hook_flag.store(false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
        }

        m_kill_condition.notify_all();
    }

}  // namespace Toolbox::Dolphin