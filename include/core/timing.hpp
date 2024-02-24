#pragma once

#include <chrono>

namespace Toolbox::Timing {

    template <typename _Callable, typename... _Args>
    inline constexpr double measure(_Callable&& fn, _Args&&... args) {
        auto t1 = std::chrono::high_resolution_clock::now();
        std::forward<_Callable>(fn)(std::forward<_Args>(args)...);
        auto t2 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(t2 - t1).count();
    }

}