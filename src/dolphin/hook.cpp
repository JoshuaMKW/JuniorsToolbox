#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "gui/application.hpp"
#include "gui/settings.hpp"

#include "dolphin/hook.hpp"

#ifdef TOOLBOX_PLATFORM_LINUX
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

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
                // Sometimes dead processes are still in the list
                if (pe32.cntThreads == 0) {
                    continue;
                }
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);

        return pid;
    }

    static Result<Platform::MemHandle, BaseError>
    OpenProcessMemory(std::string_view memory_name) {
        Platform::MemHandle memory_handle =
            OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, memory_name.data());
        if (!memory_handle) {
            return make_error<Platform::MemHandle>(
                "SHARED_MEMORY",
                std::format("Failed to find shared process memory handle \"{}\".", memory_name));
        }

        return memory_handle;
    }

    static Result<void *, BaseError> OpenMemoryView(Platform::MemHandle memory_handle) {
        void *mem_buf = MapViewOfFile(memory_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!mem_buf) {
            return make_error<void *>("SHARED_MEMORY",
                                      "Failed to get view of shared process memory handle.");
        }
        return mem_buf;
    }

    static Result<void> CloseProcessMemory(Platform::MemHandle memory_handle,
                                           const char* dolphin_memory_name) {
        CloseHandle(memory_handle);
        return {};
    }

    static Result<void> CloseMemoryView(void *memory_view) {
        UnmapViewOfFile(memory_view);
        return {};
    }

    static bool IsHandleOpen(Platform::LowHandle handle) {
        DCB flags;
        return GetCommState(handle, &flags);
    }

#elifdef TOOLBOX_PLATFORM_LINUX
    static Result<Platform::ProcessID, BaseError> FindProcessPID(std::string_view process_name) {
        std::array<char, 128> buffer;
        std::string pidof_cmd = "pidof ";
        pidof_cmd += process_name;
        std::string pidof_result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(pidof_cmd.c_str(), "r"), pclose);
        if (!pipe) {
            return make_error<Platform::ProcessID>("popen() failed!");
        }
        while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            pidof_result += buffer.data();
        }
        if (pidof_result.length() == 0) {
            return std::numeric_limits<Platform::ProcessID>::max();
        }
        return std::stoi(pidof_result);
    }

    static Result<Platform::MemHandle, BaseError>
    OpenProcessMemory(std::string_view memory_name) {
        char* memory_name_owned = new char[memory_name.size() + 1];
        std::memcpy(memory_name_owned, memory_name.data(), memory_name.size());
        memory_name_owned[memory_name.size()] = '\0';
        int fd = shm_open(memory_name_owned, O_RDWR, 0666);
        if (fd) {
            return make_error<Platform::MemHandle>(
                "SHARED_MEMORY",
                std::format("Failed to find shared process memory handle \"{}\".", memory_name));
        }
        delete[] memory_name_owned;
        return fd;
    }

    static Result<void *, BaseError> OpenMemoryView(Platform::MemHandle memory_handle) {
        void *ptr = mmap(NULL, 0x1800000, PROT_READ | PROT_WRITE, MAP_SHARED,
                         memory_handle, 0);
        if (ptr == MAP_FAILED) {
            return make_error<void *>("SHARED_MEMORY",
                                      "Failed to get view of shared process memory handle.");
        }
        return ptr;
    }

    static Result<void> CloseProcessMemory(Platform::MemHandle memory_handle,
                                           const char* dolphin_memory_name) {
        if (shm_unlink(dolphin_memory_name) != 0) {
            return make_error<void>("SHARED_MEMORY",
                                    "Failed to close handle to shared memory object");
        }
        return {};
    }

    static Result<void> CloseMemoryView(void *memory_view) {
        if (munmap(memory_view, 0x1800000) != 0) {
            return make_error<void>("SHARED_MEMORY",
                                    "Failed to close memory view");
        }
        return {};
    }
#endif

    DolphinHookManager &DolphinHookManager::instance() {
        static DolphinHookManager _instance;
        return _instance;
    }

    bool DolphinHookManager::isProcessRunning() {
        return Platform::IsExProcessRunning(m_proc_info);
    }

    Result<void> DolphinHookManager::startProcess() {
        AppSettings &settings       = SettingsManager::instance().getCurrentProfile();
        GUIApplication &application = GUIApplication::instance();

        if (isProcessRunning()) {
            return {};
        }

        std::string dolphin_args =
            std::format("-e {}/sys/main.dol -d -c -a HLE", application.getProjectRoot().string());
#ifdef TOOLBOX_PLATFORM_LINUX
        // Force dolphin to run on X11 instead of Wayland if running on Linux
        putenv("QT_QPA_PLATFORM=xcb");
#endif

        auto process_result = Platform::CreateExProcess(settings.m_dolphin_path, dolphin_args);
        if (!process_result) {
            return std::unexpected(process_result.error());
        }

        m_proc_info = process_result.value();
        return {};
    }

    Result<void> DolphinHookManager::stopProcess() {
        auto process_result = Platform::KillExProcess(m_proc_info, 100);
        if (!process_result) {
            return std::unexpected(process_result.error());
        }

        m_proc_info  = Platform::ProcessInformation{};
        m_mem_handle = NULL_MEMHANDLE;
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

        std::string dolphin_memory_name = std::format("dolphin-emu.{}",
                                                      m_proc_info.m_process_id);

        auto handle_result = OpenProcessMemory(dolphin_memory_name);
        if (!handle_result || handle_result.value() == NULL_MEMHANDLE) {
            return std::unexpected(handle_result.error());
        }

        m_mem_handle = handle_result.value();

        auto view_result = OpenMemoryView(m_mem_handle);
        if (!view_result || view_result.value() == nullptr) {
            CloseProcessMemory(m_mem_handle, dolphin_memory_name.data());
            m_mem_handle = NULL_MEMHANDLE;
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

        std::string dolphin_memory_name = std::format("dolphin-emu.{}",
                                                      m_proc_info.m_process_id);
        auto handle_result = CloseProcessMemory(m_mem_handle, dolphin_memory_name.data());
        if (!handle_result) {
            return std::unexpected(view_result.error());
        }
        m_mem_handle = NULL_MEMHANDLE;
        return {};
    }

    Result<bool> DolphinHookManager::refresh() {
        if (!Platform::IsExProcessRunning(m_proc_info)) {
            auto unhook_result = unhook();
            if (!unhook_result) {
                return std::unexpected(unhook_result.error());
            }
        }

        /*if (m_mem_view) {
          m_memory_mutex.lock();
            CloseMemoryView(m_mem_view);
            auto view_result = OpenMemoryView(m_mem_handle);
            if (!view_result || view_result.value() == nullptr) {
                CloseProcessMemory(m_mem_handle);
                m_mem_handle = nullptr;
                m_mem_view   = nullptr;
                m_memory_mutex.unlock();
                return std::unexpected(view_result.error());
            }
            m_memory_mutex.unlock();
        }*/

        return hook();
    }

    Result<void> DolphinHookManager::readBytes(char *buf, u32 address, size_t size) {
        std::unique_lock lock(m_memory_mutex);
        if (!m_mem_view) {
            return make_error<void>("SHARED_MEMORY",
                                    "Tried to read bytes without a memory handle!");
        }

        if ((address & 0x7FFFFFFF) >= 0x1800000) {
            return make_error<void>("SHARED_MEMORY",
                                    "Tried to read bytes to a protected memory region!");
        }

        const char *true_address = static_cast<const char *>(m_mem_view) + (address & 0x7FFFFFFF);
        memcpy(buf, true_address, size);
        return {};
    }

    Result<void> DolphinHookManager::writeBytes(const char *buf, u32 address, size_t size) {
        std::unique_lock lock(m_memory_mutex);
        if (!m_mem_view) {
            return make_error<void>("SHARED_MEMORY",
                                    "Tried to write bytes without a memory handle!");
        }

        if ((address & 0x7FFFFFFF) >= 0x1800000) {
            return make_error<void>("SHARED_MEMORY",
                                    "Tried to write bytes to a protected memory region!");
        }

        char *true_address = static_cast<char *>(m_mem_view) + (address & 0x7FFFFFFF);
        memcpy(true_address, buf, size);
        return {};
    }

}  // namespace Toolbox::Dolphin