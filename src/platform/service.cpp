#if _WIN32
#include <Windows.h>
#endif
#include <iostream>

#include "platform/service.hpp"
#include "core/core.hpp"

namespace Toolbox::Platform {

#ifdef TOOLBOX_PLATFORM_WINDOWS
    Result<bool, BaseError> IsServiceRunning(std::string_view name) {
        // Open the Service Control Manager
        SC_HANDLE scm_handle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
        if (!scm_handle) {
            return make_error<bool>("WIN_SERVICE", "Failed to open service control manager.");
        }

        // Open the service
        SC_HANDLE service_handle = OpenService(scm_handle, (LPCSTR)name.data(), SERVICE_QUERY_STATUS);
        if (!service_handle) {
            std::cerr << "Failed to open service: " << name << std::endl;
            CloseServiceHandle(scm_handle);
            return make_error<bool>("WIN_SERVICE", std::format("Failed to open handle to win service \"{}\".", name));
        }

        // Query the service status
        SERVICE_STATUS_PROCESS service_status{};
        DWORD bytesNeeded;
        if (!QueryServiceStatusEx(service_handle, SC_STATUS_PROCESS_INFO,
                                  reinterpret_cast<LPBYTE>(&service_status), sizeof(service_status),
                                  &bytesNeeded)) {
            std::cerr << "Failed to query service status." << std::endl;
            CloseServiceHandle(service_handle);
            CloseServiceHandle(scm_handle);
            return make_error<bool>("WIN_SERVICE",
                                    "Failed to query service status.");
        }

        // Check the running state
        bool isRunning = (service_status.dwCurrentState == SERVICE_RUNNING);

        // Cleanup
        CloseServiceHandle(service_handle);
        CloseServiceHandle(scm_handle);

        return isRunning;
    }
#elifdef TOOLBOX_PLATFORM_LINUX
    Result<bool, BaseError> IsServiceRunning(std::string_view name) {
      return false;
    }
#else
  #error "Unsupported OS"
#endif

}  // namespace Toolbox::Platform