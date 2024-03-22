#pragma once

#include <chrono>

namespace Toolbox {

    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    inline TimePoint GetTime() {
        return std::chrono::high_resolution_clock::now();
    }

}  // namespace Toolbox::Platform