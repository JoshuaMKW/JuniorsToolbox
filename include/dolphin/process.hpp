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
        ~DolphinCommunicator() { kill(); }

        DolphinHookManager &manager() const { return DolphinHookManager::instance(); }

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

        template <typename T> Result<T, BaseError> read(u32 address) {
            T data;

            auto result = DolphinHookManager::instance().readBytes(reinterpret_cast<char *>(&data),
                                                                   address, sizeof(T));
            if (!result) {
                return std::unexpected(result.error());
            }

            return *endian_swapped_t<T>(data);
        }
        template <typename T> Result<void> write(u32 address, const T &value) {
            T swapped_v = *endian_swapped_t<T>(value);
            auto result = DolphinHookManager::instance().writeBytes(
                reinterpret_cast<const char *>(std::addressof(swapped_v)), address, sizeof(T));
            if (!result) {
                return std::unexpected(result.error());
            }
            return {};
        }

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