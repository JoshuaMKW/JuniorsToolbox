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

    void DolphinCommunicator::tRun(void *param) {
        while (!tIsSignalKill()) {
            AppSettings &settings = SettingsManager::instance().getCurrentProfile();

            if (!m_hook_flag.load()) {
#if 0
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
#endif
            }

            {
                std::unique_lock<std::mutex> lk(m_mutex);
                DolphinHookManager::instance().refresh();
                m_hook_flag.store(false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
        }
    }

}  // namespace Toolbox::Dolphin