#pragma once

#include <filesystem>

#include <core/core.hpp>
#include <core/error.hpp>

#include "fsystem.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Toolbox::Platform {

#ifdef TOOLBOX_PLATFORM_WINDOWS
    typedef HANDLE LowHandle;
    typedef DWORD ProcessID;
    typedef HWND LowWindow;
    typedef void* MemHandle;
#define NULL_MEMHANDLE nullptr
#elif defined(TOOLBOX_PLATFORM_LINUX)
    typedef void *LowHandle;
    typedef pid_t ProcessID;
    typedef void *LowWindow;
    typedef int MemHandle;
#define NULL_MEMHANDLE -1
#endif

    struct ProcessInformation {
        std::string m_process_name = "";
        LowHandle m_process        = nullptr;
        ProcessID m_process_id     = std::numeric_limits<ProcessID>::max();
        LowHandle m_thread         = nullptr;
        ProcessID m_thread_id      = std::numeric_limits<ProcessID>::max();
    };

    bool IsExProcessRunning(const ProcessInformation &process);
    Result<ProcessInformation> CreateExProcess(const fs_path &program_path,
                                               std::string_view cmdargs, bool background_proc = false);
    Result<ProcessInformation> GetExProcess(const std::string &process_name);
    Result<void> KillExProcess(const ProcessInformation &process,
                               size_t max_wait = std::numeric_limits<ProcessID>::max());


    std::vector<LowWindow> FindWindowsOfProcess(const ProcessInformation &process);

    std::string GetWindowTitle(LowWindow window);
    bool GetWindowClientRect(LowWindow window, int &x, int &y, int &width, int &height);
    bool GetWindowZOrder(LowWindow window, int &zorder);

    bool ForceWindowToFront(LowWindow window);
    bool ForceWindowToFront(LowWindow window, LowWindow target);

    bool SetWindowTransparency(LowWindow window, uint8_t alpha);
    bool SetWindowClickThrough(LowWindow window, bool click_through);

    bool HideWindow(LowWindow window);
    bool ShowWindow(LowWindow window);

    bool OpenFileExplorer(const fs_path &path);

}  // namespace Toolbox::Platform