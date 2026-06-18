#pragma once

#include "core/types.hpp"
#include <type_traits>

namespace Toolbox {

    template <typename T> constexpr T Wrap(T value, T min, T max) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "Wrap requires integral or floating point values");
        if (value > max) {
            value = min + (value - max);
        } else if (value < min) {
            value = max + (value - min);
        }
        return value;
    }

}  // namespace Toolbox