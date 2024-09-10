#include "platform/capture.hpp"
#include "core/types.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS

#include <Windows.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment(lib, "Gdiplus.lib")

// Initialize GDI+
ULONG_PTR gdiplusToken;
GdiplusStartupInput gdiplusStartupInput;
Status s_status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

#endif


using namespace Toolbox;

namespace Toolbox::Platform {

#ifdef TOOLBOX_PLATFORM_WINDOWS

    struct ProcInfo {
        const DWORD m_target_proc;
        std::vector<HWND> *m_windows_out;
    };

    // Callback function for EnumWindows
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM l_param) {
        DWORD current_proc_id;
        GetWindowThreadProcessId(hwnd, &current_proc_id);

        ProcInfo *proc_info = reinterpret_cast<ProcInfo *>(l_param);
        if (current_proc_id == proc_info->m_target_proc) {
            // Save or process the window handle as needed
            // For example, you can save it to a vector passed via lParam
            proc_info->m_windows_out->push_back(hwnd);
        }
        return TRUE;  // Continue enumeration
    }

    // Function to resize a bitmap to the size of a given window
    static HBITMAP ResizeBitmapToWindow(int width, int height, HBITMAP originalBitmap) {
        return originalBitmap;
    }

    GLuint CaptureWindowTexture(const ProcessInformation &process, std::string_view name_selector,
                                int width, int height) {
        if (width <= 0 || height <= 0) {
            return std::numeric_limits<GLuint>::max();
        }

        std::vector<HWND> windows_of_process;

        ProcInfo proc_info = {process.m_process_id, &windows_of_process};

        // Enumerate windows using EnumWindowsProc
        if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&proc_info))) {
            return std::numeric_limits<GLuint>::max();
        }

        HWND dolphin_window_handle = NULL;

        for (const HWND &hwnd : windows_of_process) {
            std::string window_name;
            window_name.resize(256);

            if (GetWindowText(hwnd, window_name.data(), (int)window_name.size()) == 0) {
                continue;
            }

            if (window_name.contains(name_selector)) {
                dolphin_window_handle = hwnd;
                break;
            }
        }

        if (!dolphin_window_handle) {
            return std::numeric_limits<GLuint>::max();
        }

        RECT window_rect;
        if (!GetWindowRect(dolphin_window_handle, &window_rect)) {
            return std::numeric_limits<GLuint>::max();
        }

        // W * H * RGBA
        std::vector<u8> pixels(width * height * 4);

        // Create the texture handle
        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        // Set filters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA,
                     GL_UNSIGNED_BYTE, &pixels[0]);

        return texture_id;
    }
#elifdef TOOLBOX_PLATFORM_LINUX
#endif

}  // namespace Toolbox::Platform