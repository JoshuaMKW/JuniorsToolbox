#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/core.hpp"
#include "core/types.hpp"
#include "core/threaded.hpp"

#include "dolphin/hook.hpp"

#include "gui/logging/errors.hpp"
#include "gui/settings.hpp"

using namespace Toolbox;
using namespace Toolbox::UI;

namespace Toolbox::Dolphin {

    inline std::chrono::milliseconds GetSleepDuration(f32 framerate) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();
        return std::min(
            std::chrono::milliseconds(settings.m_dolphin_refresh_rate),
            std::chrono::milliseconds(static_cast<u32>(1000 / framerate)));
    }

    class DolphinCommunicator : public Threaded<void> {
    public:
        DolphinCommunicator() = default;

        DolphinHookManager &manager() const { return DolphinHookManager::instance(); }

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
    };

}  // namespace Toolbox::Dolphin