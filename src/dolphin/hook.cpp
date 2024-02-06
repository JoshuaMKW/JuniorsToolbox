#include <string>
#include <string_view>

#define NOMINMAX

#include "dolphin/hook.hpp"

namespace Toolbox::Dolphin {

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <tlhelp32.h>

    static std::expected<DolphinHookManager::ProcessID, BaseError>
    FindProcessPID(std::string_view process_name) {
        std::string process_file = std::string(process_name) + ".exe";

        DolphinHookManager::LowHandle hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return make_error<DolphinHookManager::ProcessID>(std::format(
                "(PROCESS) Failed to create snapshot to find process \"{}\".", process_file));
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            CloseHandle(hSnapshot);
            return make_error<DolphinHookManager::ProcessID>(
                "(PROCESS) Failed to retrieve first process entry!");
        }

        DolphinHookManager::ProcessID pid =
            std::numeric_limits<DolphinHookManager::ProcessID>::max();
        do {
            if (strcmp(pe32.szExeFile, process_file.data()) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);

        return pid;
    }

    static std::expected<DolphinHookManager::LowHandle, BaseError>
    OpenProcessMemory(std::string_view memory_name) {
        DolphinHookManager::LowHandle memory_handle =
            OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, memory_name.data());
        if (!memory_handle) {
            return make_error<DolphinHookManager::LowHandle>(std::format(
                "(SHARED_MEM) Failed to find shared process memory handle \"{}\".", memory_name));
        }

        return memory_handle;
    }

    static std::expected<void *, BaseError>
    OpenMemoryView(DolphinHookManager::LowHandle memory_handle) {
        void *mem_buf = MapViewOfFile(memory_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!mem_buf) {
            return make_error<void *>(
                "(SHARED_MEM) Failed to get view of shared process memory handle.");
        }
    }

    static std::expected<void, BaseError>
    CloseProcessMemory(DolphinHookManager::LowHandle memory_handle) {
        CloseHandle(memory_handle);
        return {};
    }

    static std::expected<void, BaseError> CloseMemoryView(void *memory_view) {
        UnmapViewOfFile(memory_view);
        return {};
    }

#elif TOOLBOX_PLATFORM_LINUX
    static std::expected<DolphinHookManager::ProcessID, BaseError>
    FindProcessPID(std::string_view process_name) {
        return make_error<DolphinHookManager::ProcessID>("Linux support unimplemented!");
    }

    static std::expected<DolphinHookManager::MemoryHandle, BaseError>
    OpenProcessMemory(std::string_view memory_name) {
        return nullptr;
    }

    static std::expected<void *, BaseError>
    OpenMemoryView(DolphinHookManager::MemoryHandle memory_handle) {
        return nullptr;
    }

    static std::expected<void, BaseError>
    CloseProcessMemory(DolphinHookManager::MemoryHandle memory_handle) {
        return {};
    }

    static std::expected<void, BaseError> CloseMemoryView(void *memory_view) { return {}; }
#endif

    std::expected<void, BaseError> DolphinHookManager::hook() {
        if (m_mem_view) {
            return {};
        }

        const ProcessID sentinel                  = std::numeric_limits<ProcessID>::max();
        ProcessID pid                             = sentinel;
        std::vector<std::string> target_processes = {"Dolphin", "DolphinQt2",
                                                     "DolphinWx"};

        size_t i = 0;
        for (auto &proc_name : target_processes) {
            auto pid_result = FindProcessPID(proc_name);
            if (!pid_result) {
                return std::unexpected(pid_result.error());
            }
            pid = pid_result.value();
            if (pid != sentinel) {
                break;
            }
            i += 1;
        };

        if (pid == sentinel) {
            return make_error<void>(
                "(PROCESS) Failed to find valid Dolphin Emulator instance running!");
        }

        m_proc_id = pid;
        m_proc_name = target_processes[i];

        std::string dolphin_memory_name = std::format("dolphin-emu.{}", pid);

        auto handle_result = OpenProcessMemory(dolphin_memory_name);
        if (!handle_result || handle_result.value() == nullptr) {
            return std::unexpected(handle_result.error());
        }

        m_mem_handle = handle_result.value();

        auto view_result = OpenMemoryView(m_mem_handle);
        if (!view_result || view_result.value() == nullptr) {
            CloseProcessMemory(m_mem_handle);
            m_mem_handle = nullptr;
            return std::unexpected(view_result.error());
        }

        m_mem_view = view_result.value();

        TOOLBOX_INFO_V("(DOLPHIN) Successfully hooked to process! (Name={}, PID={}, View={})",
                       m_proc_name, m_proc_id, m_mem_view);

        return {};
    }

    std::expected<void, BaseError> DolphinHookManager::unhook() {
        if (!m_mem_view) {
            return {};
        }

        auto view_result = CloseMemoryView(m_mem_view);
        if (!view_result) {
            return std::unexpected(view_result.error());
        }
        m_mem_view = nullptr;

        auto handle_result = CloseProcessMemory(m_mem_handle);
        if (!handle_result) {
            return std::unexpected(view_result.error());
        }
        m_mem_handle = nullptr;
        return {};
    }

    std::expected<void, BaseError> DolphinHookManager::readBytes(char *buf, u32 address,
                                                                 size_t size) {
        const char *true_address = static_cast<const char *>(m_mem_view) + (address & 0x7FFFFFFF);
        memcpy(buf, true_address, size);
        return {};
    }

    std::expected<void, BaseError> DolphinHookManager::writeBytes(const char *buf, u32 address,
                                                                  size_t size) {
        char *true_address = static_cast<char *>(m_mem_view) + (address & 0x7FFFFFFF);
        memcpy(true_address, buf, size);
        return {};
    }

}  // namespace Toolbox::Dolphin