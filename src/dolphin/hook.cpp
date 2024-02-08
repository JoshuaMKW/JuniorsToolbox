#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "core/application.hpp"
#include "gui/settings.hpp"

#include "dolphin/hook.hpp"

namespace Toolbox::Dolphin {

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <tlhelp32.h>

    static Result<Platform::ProcessID, BaseError> FindProcessPID(std::string_view process_name) {
        std::string process_file = std::string(process_name) + ".exe";

        Platform::LowHandle hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return make_error<Platform::ProcessID>(std::format(
                "(PROCESS) Failed to create snapshot to find process \"{}\".", process_file));
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            CloseHandle(hSnapshot);
            return make_error<Platform::ProcessID>(
                "(PROCESS) Failed to retrieve first process entry!");
        }

        Platform::ProcessID pid = std::numeric_limits<Platform::ProcessID>::max();
        do {
            if (strcmp(pe32.szExeFile, process_file.data()) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);

        return pid;
    }

    static Result<Platform::LowHandle, BaseError> OpenProcessMemory(std::string_view memory_name) {
        Platform::LowHandle memory_handle =
            OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, memory_name.data());
        if (!memory_handle) {
            return make_error<Platform::LowHandle>(
                "SHARED_MEMORY",
                std::format("Failed to find shared process memory handle \"{}\".", memory_name));
        }

        return memory_handle;
    }

    static Result<void *, BaseError> OpenMemoryView(Platform::LowHandle memory_handle) {
        void *mem_buf = MapViewOfFile(memory_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!mem_buf) {
            return make_error<void *>("SHARED_MEMORY",
                                      "Failed to get view of shared process memory handle.");
        }
        return mem_buf;
    }

    static Result<void> CloseProcessMemory(Platform::LowHandle memory_handle) {
        CloseHandle(memory_handle);
        return {};
    }

    static Result<void> CloseMemoryView(void *memory_view) {
        UnmapViewOfFile(memory_view);
        return {};
    }

#elif TOOLBOX_PLATFORM_LINUX
    static Result<Platform::ProcessID, BaseError> FindProcessPID(std::string_view process_name) {
        return make_error<Platform::ProcessID>("Linux support unimplemented!");
    }

    static Result<Platform::MemoryHandle, BaseError>
    OpenProcessMemory(std::string_view memory_name) {
        return nullptr;
    }

    static Result<void *, BaseError> OpenMemoryView(Platform::MemoryHandle memory_handle) {
        return nullptr;
    }

    static Result<void> CloseProcessMemory(Platform::MemoryHandle memory_handle) { return {}; }

    static Result<void> CloseMemoryView(void *memory_view) { return {}; }
#endif

    bool DolphinHookManager::isProcessRunning() {
        return Platform::IsExProcessRunning(m_proc_info);
    }

    Result<void> DolphinHookManager::startProcess() {
        AppSettings &settings        = SettingsManager::instance().getCurrentProfile();
        MainApplication &application = MainApplication::instance();

        if (isProcessRunning()) {
            return {};
        }

        auto process_result = Platform::CreateExProcess(
            "C:/Users/Kyler-Josh/3D Objects/Dolphin/Dolphin-x64/Dolphin.exe",
            "-e C:/Users/Kyler-Josh/Dropbox/Master_Builds/Eclipse_Master/sys/main.dol -d -c -a "
            "HLE");
        if (!process_result) {
            return std::unexpected(process_result.error());
        }

        m_proc_info = process_result.value();
        return {};
    }

    Result<void> DolphinHookManager::stopProcess() {
        auto process_result = Platform::KillExProcess(m_proc_info);
        if (!process_result) {
            return std::unexpected(process_result.error());
        }

        m_proc_info  = Platform::ProcessInformation{};
        m_mem_handle = nullptr;
        m_mem_view   = nullptr;
        return {};
    }

    Result<bool> DolphinHookManager::hook() {
        if (m_mem_view) {
            return {};
        }

        constexpr Platform::ProcessID sentinel = std::numeric_limits<Platform::ProcessID>::max();

        if (!IsExProcessRunning(m_proc_info)) {
            Platform::ProcessID pid                   = sentinel;
            std::vector<std::string> target_processes = {"Dolphin", "DolphinQt2", "DolphinWx"};

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
                return false;
            }

            m_proc_info.m_process_name = target_processes[i];
            m_proc_info.m_process_id   = pid;
        }

        std::string dolphin_memory_name = std::format("dolphin-emu.{}", m_proc_info.m_process_id);

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

        TOOLBOX_INFO_V("DOLPHIN: Successfully hooked to process! (Name={}, PID={}, View={})",
                       m_proc_info.m_process_name, m_proc_info.m_process_id, m_mem_view);

        char magic[4];
        readBytes(magic, 0x80000000, 4);
        if ((magic[0] != 'G' || magic[1] != 'M' || magic[2] != 'S') &&
            (magic[0] != '\0' || magic[1] != '\0' || magic[2] != '\0')) {
            TOOLBOX_WARN("DOLPHIN: Game found is not Super Mario Sunshine (GMS)!");
        }

        return {};
    }

    Result<bool> DolphinHookManager::unhook() {
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

    Result<void> DolphinHookManager::readBytes(char *buf, u32 address, size_t size) {
        if (!m_mem_view)
            return make_error<void>("SHARED_MEMORY",
                                    "Tried to read bytes without a memory handle!");

        m_memory_mutex.lock();
        const char *true_address = static_cast<const char *>(m_mem_view) + (address & 0x7FFFFFFF);
        memcpy(buf, true_address, size);
        m_memory_mutex.unlock();
        return {};
    }

    Result<void> DolphinHookManager::writeBytes(const char *buf, u32 address, size_t size) {
        if (!m_mem_view)
            return make_error<void>("SHARED_MEMORY",
                                    "Tried to write bytes without a memory handle!");

        m_memory_mutex.lock();
        char *true_address = static_cast<char *>(m_mem_view) + (address & 0x7FFFFFFF);
        memcpy(true_address, buf, size);
        m_memory_mutex.unlock();
        return {};
    }

}  // namespace Toolbox::Dolphin