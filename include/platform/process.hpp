#pragma once

#include <filesystem>

#include <core/core.hpp>
#include <core/error.hpp>

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elif TOOLBOX_PLATFORM_LINUX
#endif

namespace Toolbox::Platform {

#ifdef TOOLBOX_PLATFORM_WINDOWS
    typedef HANDLE LowHandle;
    typedef DWORD ProcessID;
#elif TOOLBOX_PLATFORM_LINUX
    typedef void *MemoryHandle;
    typedef u64 ProcessID;
#endif

    struct ProcessInformation {
        std::string m_process_name = "";
        LowHandle m_process        = nullptr;
        ProcessID m_process_id     = std::numeric_limits<ProcessID>::max();
        LowHandle m_thread         = nullptr;
        ProcessID m_thread_id      = std::numeric_limits<ProcessID>::max();
    };

    Result<ProcessInformation> CreateExProcess(const std::filesystem::path &program_path,
                                               std::string_view cmdargs);
    Result<void> KillExProcess(const ProcessInformation &process,
                               size_t max_wait = std::numeric_limits<ProcessID>::max());
    bool IsExProcessRunning(const ProcessInformation &process);
}  // namespace Toolbox::Platform