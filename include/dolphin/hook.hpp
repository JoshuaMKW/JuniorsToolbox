#pragma once

#include <expected>
#include <mutex>
#include <optional>
#include <span>

#include "core/core.hpp"
#include "core/error.hpp"
#include "core/memory.hpp"
#include "platform/process.hpp"

#include "unique.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elif TOOLBOX_PLATFORM_LINUX
#endif

using namespace Toolbox;

namespace Toolbox::Dolphin {

    class DolphinHookManager {
    public:
        static DolphinHookManager &instance() {
            static DolphinHookManager _instance;
            return _instance;
        }

    protected:
        DolphinHookManager() = default;

    public:
        // Check if a specific UUID owns the lock
        bool hasLock(const UUID64 &uuid) const {
            return m_owner.has_value() && m_owner.value() == uuid;
        }

        // Locks the usage to a specific UUID. The idea here
        // is no component knows of another's UUID which
        // forces isolated ownership.
        bool lock(const UUID64 &uuid) {
            if (m_owner.has_value()) {
                return false;
            }
            m_owner = uuid;
            return true;
        }

        // Unlock the usage from a specific UUID.
        bool unlock(const UUID64 &uuid) {
            if (!hasLock(uuid)) {
                return false;
            }
            m_owner = std::nullopt;
        }

        //---

        bool isProcessRunning();

        const Platform::ProcessInformation &getProcess() const { return m_proc_info; }
        Result<void> startProcess();
        Result<void> stopProcess();

        bool isHooked() const {
            return m_mem_view && Platform::IsExProcessRunning(m_proc_info);
        }

        Result<bool> hook();
        Result<bool> unhook();
        Result<bool> refresh() {
            if (!Platform::IsExProcessRunning(m_proc_info)) {
                auto unhook_result = unhook();
                if (!unhook_result) {
                    return std::unexpected(unhook_result.error());
                }
            }

            return hook();
        }

        Result<void> readBytes(char *buf, u32 address, size_t size);
        Result<void> writeBytes(const char *buf, u32 address, size_t size);

        u32 captureXFBAsTexture(int width, int height, u32 xfb_start, int xfb_width, int xfb_height);

    private:
        Platform::ProcessInformation m_proc_info;

        Platform::LowHandle m_mem_handle{};
        void *m_mem_view = nullptr;

        std::mutex m_memory_mutex;

        std::optional<UUID64> m_owner;
    };

}  // namespace Toolbox::Dolphin