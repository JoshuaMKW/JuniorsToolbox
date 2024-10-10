#pragma once

#include <filesystem>

#include "core/core.hpp"
#include "core/error.hpp"

#include "platform/process.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <TlHelp32.h>
#include <Windows.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <limits>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
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

    Result<ProcessInformation> CreateExProcess(const fs_path &program_path,
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

    // Structure to store child windows and their z-order
    struct WindowInfo {
        HWND hwnd;
        int zOrder;
    };

    BOOL CALLBACK EnumWindowsForZProc(HWND hwnd, LPARAM lParam) {
        std::vector<WindowInfo> *windows = reinterpret_cast<std::vector<WindowInfo> *>(lParam);

        // Add the child window to the vector
        WindowInfo info;
        info.hwnd   = hwnd;
        info.zOrder = windows->size();  // The index represents the Z-order among child windows
        windows->push_back(info);

        return TRUE;  // Continue enumeration
    }

    bool GetWindowZOrder(LowWindow window, int &zorder) {
        std::vector<WindowInfo> windows;
        windows.reserve(1024);

        // Get the parent window of the window
        HWND parent = GetParent(window);
        if (parent == NULL) {
            EnumWindows(EnumWindowsForZProc, reinterpret_cast<LPARAM>(&windows));
        } else {
            // Enumerate the child windows of the parent window
            EnumChildWindows(parent, EnumWindowsForZProc, reinterpret_cast<LPARAM>(&windows));
        }

        // Find the Z-order of the window among the child windows
        for (size_t i = 0; i < windows.size(); i++) {
            if (windows[i].hwnd == window) {
                zorder = windows[i].zOrder;
                return true;
            }
        }

        return false;
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
        SetWindowLongPtr(window, GWL_STYLE, GetWindowLongPtr(window, GWL_STYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
        if (!ShowWindow(window, SW_SHOW)) {
            return false;
        }
        return UpdateWindow(window);
    }

    bool SetWindowClickThrough(LowWindow window, bool click_through) {
        LONG_PTR style = GetWindowLongPtr(window, GWL_EXSTYLE);
        if (click_through) {
            style |= WS_EX_LAYERED;
            style |= WS_EX_TRANSPARENT;
        } else {
            style &= ~WS_EX_LAYERED;
            style &= ~WS_EX_TRANSPARENT;
        }
        return SetWindowLongPtr(window, GWL_EXSTYLE, style) >= 0;
    }

    bool OpenFileExplorer(const fs_path &path) {
        std::wstring path_str = path.wstring();
        return (int)ShellExecuteW(NULL, L"open", L"explorer.exe", path_str.c_str(), NULL, SW_SHOW) > 32;
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)
    std::string GetLastErrorMessage() { return strerror(errno); }

    Result<ProcessInformation> CreateExProcess(const fs_path &program_path,
                                               std::string_view cmdargs) {
        ProcessID pid = fork();  // Fork the current process
        if (pid == -1) {
            // Handle error
            return make_error<ProcessInformation>("PROCESS", GetLastErrorMessage());
        } else if (pid > 0) {
            // Parent process
            return ProcessInformation{.m_process_name=program_path.stem().string(),
                                      .m_process_id=pid};
        } else {
            // Split the argument string into components by spaces
            std::stringstream argsstream(cmdargs.data());
            std::vector<std::string> string_arg_parts;
            string_arg_parts.push_back(program_path.string());
            std::string s_next;
            while (std::getline(argsstream, s_next, ' ')) {
                string_arg_parts.push_back(s_next);
            }
            // Convert the string vector into a vector of c strings
            std::vector<const char*> cstr_arg_parts;
            for(int i = 0; i < string_arg_parts.size(); ++i) {
                cstr_arg_parts.push_back(string_arg_parts[i].c_str());
            }
            // Terminate the argument vector with a null pointer
            cstr_arg_parts.push_back(nullptr);

            // Child process
            execv(program_path.c_str(), const_cast<char* const*>(&cstr_arg_parts[0]));
            // If we get here, we probably want to know about it.
            std::perror("Error executing dolphin process");
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
            usleep(max_wait * 1000); // usleep sleeps for
                                     // microseconds, so multiply the
                                     // milliseconds by 1000
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
    std::string GetWindowTitle(LowWindow window) {
        Display* display = XOpenDisplay(0);
        XTextProperty title;
        XGetWMName(display, (Window)window, &title);
        char** stringList;
        int numStrings;
        if (!XTextPropertyToStringList(&title, &stringList, &numStrings)) {
            return "";
        }
        if (numStrings < 1){
            return "";
        }
        std::string stringTitle(stringList[0]);
        XFreeStringList(stringList);
        return stringList[0];
    }
    bool GetWindowZOrder(LowWindow window, int &zorder) {
        return false;
    }
    bool ForceWindowToFront(LowWindow window) {
        Display* display = XOpenDisplay(0);
        return XRaiseWindow(display, (Window)window);
    }
    bool ForceWindowToFront(LowWindow window, LowWindow target) {
        // Not ideal behavior, this should move just in front of
        // target, but I don't know how to do that.
        return ForceWindowToFront(window);
    }

    bool SetWindowTransparency(LowWindow window, uint8_t alpha) { return false; }

    bool SetWindowClickThrough(LowWindow window, bool click_through) { return false; }

    bool GetWindowClientRect(LowWindow window, int &x, int &y, int &width, int &height) {
        XWindowAttributes attribs;
        Display* display = XOpenDisplay(0);
        if (!XGetWindowAttributes(display, (Window)window, &attribs)) {
            return false;
        }
        x = attribs.x;
        y = attribs.y;
        width = attribs.width;
        height = attribs.height;
        return true;
    }
    void searchWindowsOfProcess(const Window w, Display* display,
                                const Atom PIDAtom, const int pid,
                                std::vector<LowWindow> &result);
    std::vector<LowWindow> FindWindowsOfProcess(const ProcessInformation &process) {
        Display* display = XOpenDisplay(0);
        Atom PIDAtom = XInternAtom(display, "_NET_WM_PID", True);

        if(PIDAtom == None)
        {
            return std::vector<LowWindow>();
        }
        std::vector<LowWindow> result;
        searchWindowsOfProcess(XDefaultRootWindow(display), display,
                               PIDAtom, process.m_process_id,
                               result);
        return result;
    }
    void searchWindowsOfProcess(const Window w, Display* display,
                                const Atom PIDAtom, const int pid,
                                std::vector<LowWindow> &result) {
        // A bunch of output parameter storage, but we're only going
        // to use the last one.
        Atom type;
        int format;
        unsigned long nItems;
        unsigned long bytesAfter;
        // The storage for the PID of the window we're looking at.
        unsigned char* propPID = 0;
        if (XGetWindowProperty(display, w, PIDAtom,
                               0 /*offset*/,
                               1 /* length in words */,
                               False, XA_CARDINAL,
                               &type, &format, &nItems, &bytesAfter,
                               &propPID) == Success) {
            if (propPID != 0) {
                // If the result pointer isn't null, and it points to
                // something matching the PID we're looking for, add
                // it to our list.
                if (pid == *((unsigned long*)propPID))
                    result.push_back((void*)w);
                // Clean up some X structures
                XFree(propPID);
            }
        }
        // Now, get the tree structure so that we can recurse on children
        Window wRoot;
        Window wParent;
        Window *wChildren;
        unsigned int nChildren;
        if (XQueryTree(display, w, &wRoot, &wParent, &wChildren, &nChildren) != 0) {
            for(unsigned int i = 0; i < nChildren; ++i) {
                searchWindowsOfProcess(wChildren[i], display, PIDAtom, pid, result);
            }
        }
    }
    bool OpenFileExplorer(const fs_path &path) {
        TOOLBOX_ERROR("Open file explorer currently unsupported on linux");
        return false;
    }
#endif
}  // namespace Toolbox::Platform
