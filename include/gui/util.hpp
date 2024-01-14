#pragma once

#include <chrono>
#include <string_view>

using Clock = std::chrono::high_resolution_clock;

namespace Toolbox::UI::Util {
    Clock::time_point GetTime();

    float GetDeltaTime(Clock::time_point a, Clock::time_point b);

    std::string MakeNameUnique(std::string_view name, const std::vector<std::string> &other_names,
                               size_t *clone_index = nullptr);
}  // namespace Toolbox::UI::Util
