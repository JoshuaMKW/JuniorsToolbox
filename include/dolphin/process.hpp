#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/core.hpp"
#include "core/types.hpp"

#include "dolphin/hook.hpp"

#include "gui/logging/errors.hpp"

using namespace Toolbox;
using namespace Toolbox::UI;

namespace Toolbox::Dolphin {

    class DolphinCommunicator {
    public:
        DolphinCommunicator() = default;

        // Starts the detached thread
        void start() {
            if (!m_started) {
                m_thread  = std::thread(&DolphinCommunicator::run, this);
                m_started = true;
            }
        }

        // Waits until the running thread is killed
        void kill() {
            m_kill_flag.store(true);
            m_kill_condition.notify_one();

            std::unique_lock<std::mutex> lk(m_mutex);
            m_kill_condition.wait(lk);
        }

        void signalHook() { m_hook_flag.store(true); }

        template <typename T> T read(u32 address) {}
        template <typename T> void write(u32 address, const T &value) {}

    protected:
        void run();

    private:
        bool m_started = false;

        std::mutex m_mutex;
        std::thread m_thread;

        std::atomic<bool> m_hook_flag;

        std::atomic<bool> m_kill_flag;
        std::condition_variable m_kill_condition;
    };

}  // namespace Toolbox::Dolphin