#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/core.hpp"
#include "core/error.hpp"
#include "core/threaded.hpp"
#include "core/types.hpp"

#include "dolphin/hook.hpp"

using namespace Toolbox;
using namespace Toolbox::UI;

namespace Toolbox::Dolphin {

    class DolphinCommunicator : public Threaded<void> {
    public:
        DolphinCommunicator() = default;

        DolphinHookManager &manager() const { return DolphinHookManager::instance(); }

        void signalHook() { m_hook_flag.store(true); }

        s64 getRefreshRate() const { return m_refresh_rate; }
        void setRefreshRate(s64 milliseconds) { m_refresh_rate = milliseconds; }

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

        Result<void> readBytes(char *buf, u32 address, size_t size) {
            return DolphinHookManager::instance().readBytes(buf, address, size);
        }

        Result<void> writeBytes(const char *buf, u32 address, size_t size) {
            return DolphinHookManager::instance().writeBytes(buf, address, size);
        }

    protected:
        void tRun(void *param) override;

    private:
        bool m_started = false;

        std::mutex m_mutex;
        std::thread m_thread;

        std::atomic<bool> m_hook_flag;

        s64 m_refresh_rate = 100;
    };

}  // namespace Toolbox::Dolphin