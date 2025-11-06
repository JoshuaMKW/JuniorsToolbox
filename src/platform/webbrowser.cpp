#include <format>

#include "platform/webbrowser.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace Toolbox {

#ifdef TOOLBOX_PLATFORM_WINDOWS

    void Platform::TryOpenBrowserURL(const std::string &url) {
        LPWSTR wide_url = static_cast<LPWSTR>(std::malloc((url.size() + 1) * 2));
        if (!wide_url) {
            return;
        }

        std::mbstowcs(wide_url, url.c_str(), url.size() + 1);
        ShellExecuteW(NULL, NULL, wide_url, NULL, NULL, SW_HIDE);
    }

#elif TOOLBOX_PLATFORM_LINUX

    void Platform::TryOpenBrowserURL(const std::string &url) {
        std::string cmd = std::format("xdg-open {}", url);
        std::system(cmd.c_str());
    }

#endif

}  // namespace
