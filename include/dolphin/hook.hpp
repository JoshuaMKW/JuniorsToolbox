#pragma once

#include <expected>
#include <optional>
#include <span>

#include "core/core.hpp"
#include "core/memory.hpp"
#include "error.hpp"
#include "unique.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elif TOOLBOX_PLATFORM_LINUX
#endif

using namespace Toolbox;

namespace Toolbox::Dolphin {

    class DolphinHookManager {
    public:
#ifdef TOOLBOX_PLATFORM_WINDOWS
        typedef HANDLE LowHandle;
        typedef DWORD ProcessID;
#elif TOOLBOX_PLATFORM_LINUX
        typedef void *MemoryHandle;
        typedef u64 ProcessID;
#endif

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

        bool isHooked() const { return m_mem_view; }

        std::expected<void, BaseError> hook();
        std::expected<void, BaseError> unhook();
        std::expected<void, BaseError> refresh() {
            auto unhook_result = unhook();
            if (!unhook_result) {
                return std::unexpected(unhook_result.error());
            }

            return hook();
        }

        std::expected<void, BaseError> readBytes(char *buf, u32 address, size_t size);
        std::expected<void, BaseError> writeBytes(const char *buf, u32 address, size_t size);

    private:
        ProcessID m_proc_id;
        std::string m_proc_name;

        LowHandle m_mem_handle;
        void *m_mem_view;

        std::optional<UUID64> m_owner;
    };

}  // namespace Toolbox::Dolphin