#pragma once

#include <filesystem>

#include "core/core.hpp"
#include "core/error.hpp"

#include "platform/process.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elif TOOLBOX_PLATFORM_LINUX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <limits>
#include <process.h>
#endif

namespace Toolbox::Platform {

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