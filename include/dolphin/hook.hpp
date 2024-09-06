#pragma once

#include <expected>
#include <mutex>
#include <optional>
#include <span>

#include "core/core.hpp"
#include "core/error.hpp"
#include "core/memory.hpp"
#include "image/imagehandle.hpp"
#include "platform/process.hpp"

#include "unique.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elifdef TOOLBOX_PLATFORM_LINUX
#endif

using namespace Toolbox;

namespace Toolbox::Dolphin {

    class DolphinHookManager {
    public:
        static DolphinHookManager &instance();

    protected:
        DolphinHookManager()                                      = default;

    public:
        DolphinHookManager(const DolphinHookManager &)            = delete;
        DolphinHookManager(DolphinHookManager &&)                 = delete;
        DolphinHookManager &operator=(const DolphinHookManager &) = delete;
        DolphinHookManager &operator=(DolphinHookManager &&)      = delete;

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

        bool isHooked() const { return m_mem_view && Platform::IsExProcessRunning(m_proc_info); }

        Result<bool> hook();
        Result<bool> unhook();
        Result<bool> refresh();

        void *getMemoryView() const { return isHooked() ? m_mem_view : nullptr; }
        size_t getMemorySize() const { return isHooked() ? 0x1800000 : 0; }

        Result<void> readBytes(char *buf, u32 address, size_t size);
        Result<void> writeBytes(const char *buf, u32 address, size_t size);

        ImageHandle captureXFBAsTexture(int width, int height, u32 xfb_start, int xfb_width,
                                        int xfb_height);

    private:
        Platform::ProcessInformation m_proc_info;

        Platform::MemHandle m_mem_handle{};
        void *m_mem_view = nullptr;

        std::mutex m_memory_mutex;

        std::optional<UUID64> m_owner;
    };

}  // namespace Toolbox::Dolphin