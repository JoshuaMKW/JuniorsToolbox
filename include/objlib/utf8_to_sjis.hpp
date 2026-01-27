#pragma once

#include <array>
#include <string>
#include <string_view>

namespace Toolbox {

    template <size_t N> constexpr std::string to_str(const char8_t (&input)[N]) {
        static_assert(N > 0, "This shouldn't happen!");
        std::string output;
        output.assign(reinterpret_cast<const char *>(input), N - 1);
        return output;
    }

}  // namespace Toolbox