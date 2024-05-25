#pragma once

#include <filesystem>

#include "core/core.hpp"
#include "core/error.hpp"

#include "platform/process.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <TlHelp32.h>
#include <Windows.h>
#elif TOOLBOX_PLATFORM_LINUX
#include <limits>
#include <process.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Toolbox::Platform {

    struct EnumWindowsData {
        ProcessID m_proc_id;
        std::vector<LowWindow> m_window_handles;
    };

#ifdef TOOLBOX_PLATFORM_WINDOWS
    static std::string GetLastErrorMessage() {
        DWORD error = GetLastError();

        // Buffer that will hold the error message
        LPSTR errorMessageBuffer = nullptr;

        DWORD size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   (LPSTR)&errorMessageBuffer, 0, NULL);

        std::string error_msg(errorMessageBuffer);
        LocalFree(errorMessageBuffer);

        if (size > 0) {
            return error_msg;
        } else {
            return "Failed to spin new process for unknown reasons.";
        }
    }

    Result<ProcessInformation> CreateExProcess(const std::filesystem::path &program_path,
                                               std::string_view cmdargs) {
        std::string true_cmdargs =
            std::format("\"{}\" {}", program_path.string().c_str(), cmdargs.data());

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcess(program_path.string().data(), true_cmdargs.data(), NULL, NULL, FALSE, 0,
                           NULL, NULL, &si, &pi)) {
            return make_error<ProcessInformation>("PROCESS", GetLastErrorMessage());
        }

        return {
            {program_path.stem().string(), pi.hProcess, pi.dwProcessId, pi.hThread, pi.dwThreadId}
        };
    }

    Result<void> KillExProcess(const ProcessInformation &process, size_t max_wait) {
        if (process.m_process) {
            if (!TerminateProcess(process.m_process, 0)) {
                return make_error<void>("PROCESS", GetLastErrorMessage());
            }
            CloseHandle(process.m_process);
        }

        if (process.m_thread) {
            DWORD result = WaitForSingleObject(process.m_thread, (DWORD)max_wait);
            if (result == WAIT_TIMEOUT) {
                // Force thread to close
                if (!TerminateThread(process.m_thread, 0)) {
                    return make_error<void>("PROCESS", GetLastErrorMessage());
                }
            }
            CloseHandle(process.m_thread);
        }

        return {};
    }

    bool IsExProcessRunning(const ProcessInformation &process) {
        if (process.m_process_id == std::numeric_limits<Platform::ProcessID>::max()) {
            return false;
        }

        HANDLE proc_handle =
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.m_process_id);
        if (!proc_handle) {
            return false;
        }

        DWORD exit_code = 0;
        if (GetExitCodeProcess(proc_handle, &exit_code)) {
            CloseHandle(proc_handle);
            return exit_code == STILL_ACTIVE;
        }

        TOOLBOX_DEBUG_LOG(GetLastErrorMessage());

        CloseHandle(proc_handle);
        return false;
    }

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM param) {
        EnumWindowsData *data = reinterpret_cast<EnumWindowsData *>(param);
        DWORD win_proc_id     = 0;
        GetWindowThreadProcessId(hwnd, &win_proc_id);
        if (win_proc_id == data->m_proc_id) {
            data->m_window_handles.push_back(hwnd);
        }
        return TRUE;
    }

    std::vector<LowWindow> FindWindowsOfProcess(const ProcessInformation &process) {
        EnumWindowsData data = {process.m_process_id, {}};
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
        return data.m_window_handles;
    }

    std::string GetWindowTitle(LowWindow window) {
        WINDOWINFO win_info{};
        if (!GetWindowInfo(window, &win_info)) {
            return "";
        }

        char title[256];
        int title_len = GetWindowText(window, title, sizeof(title));

        std::string win_title = std::string(title, title_len);

        TOOLBOX_DEBUG_LOG_V("Window ({}) - Type: {}, Status: {}", win_title,
                            win_info.atomWindowType, win_info.dwWindowStatus);

        return win_title;
    }

    bool GetWindowClientRect(LowWindow window, int &x, int &y, int &width, int &height) {
        RECT win_rect;
        if (!GetClientRect(window, &win_rect)) {
            return false;
        }

        POINT top_left = {win_rect.left, win_rect.top};
        ClientToScreen(window, &top_left);

        x      = top_left.x;
        y      = top_left.y;
        width  = win_rect.right - win_rect.left;
        height = win_rect.bottom - win_rect.top;

        return true;
    }

    bool ForceWindowToFront(LowWindow window) {
        return SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    bool ForceWindowToFront(LowWindow window, LowWindow target) {
        return SetWindowPos(window, GetNextWindow(target, GW_HWNDPREV), 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    bool SetWindowTransparency(LowWindow window, uint8_t alpha) {
        SetWindowLongPtr(window, GWL_STYLE, WS_EX_LAYERED);
        SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
        if (!ShowWindow(window, SW_SHOW)) {
            return false;
        }
        return UpdateWindow(window);
    }

#elif TOOLBOX_PLATFORM_LINUX
    std::string GetLastErrorMessage() { return std::strerror(errno); }

    Result<ProcessInformation> CreateExProcess(const std::filesystem::path &program_path,
                                               const std::string &cmdargs) {
        std::string true_cmdargs =
            std::format("\"{}\" {}", program_path.string().c_str(), cmdargs.data());

        ProcessID pid = fork();  // Fork the current process
        if (pid == -1) {
            // Handle error
            return make_error<ProcessInformation>("PROCESS", GetLastErrorMessage());
        } else if (pid > 0) {
            // Parent process
            return ProcessInformation{program_path.stem().string(), pid};
        } else {
            // Child process
            execl(program_path.c_str(), true_cmdargs.c_str(), (char *)NULL);
            // execl only returns on error
            _exit(EXIT_FAILURE);
        }
    }

    Result<void> KillExProcess(const ProcessInformation &process, size_t max_wait) {
        if (kill(process.m_process_id, SIGTERM) == -1) {
            return make_error<void>("PROCESS", GetLastErrorMessage());
        }

        int status;
        ProcessID result = waitpid(process.m_process_id, &status, WNOHANG);
        if (result == 0) {
            // Process is still running, try to wait for max_wait time
            sleep(max_wait);
            result = waitpid(process.m_process_id, &status, WNOHANG);
            if (result == 0) {
                // Process is still running, force kill it
                if (kill(process.m_process_id, SIGKILL) == -1) {
                    return make_error<void>("PROCESS", GetLastErrorMessage());
                }
            }
        }

        return {};
    }

    bool IsExProcessRunning(const ProcessInformation &process) {
        if (process.m_process_id <= 0 ||
            process.m_process_id == std::numeric_limits<ProcessID>::max()) {
            return false;
        }

        if (kill(process.m_process_id, 0) == -1) {
            if (errno == ESRCH) {
                // The process does not exist.
                return false;
            }
            // Other errors indicate that the process exists and may be a zombie.
        }

        return true;
    }
#endif
}  // namespace Toolbox::Platform