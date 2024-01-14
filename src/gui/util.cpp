#include "gui/util.hpp"
#include <iconv.h>
#include <cmath>

namespace Toolbox::UI::Util {
    Clock::time_point GetTime() { return Clock::now(); }

    float GetDeltaTime(Clock::time_point a, Clock::time_point b) {
        auto seconds = std::chrono::duration<float>{b - a};

        return seconds.count();
    }

    std::string MakeNameUnique(std::string_view name,
                               const std::vector<std::string> &other_names, size_t *clone_index) {
        std::string unique_name = std::string(name);
        size_t unique_id        = 1;
        while (true) {
            auto name_it = std::find(other_names.begin(), other_names.end(), unique_name);
            if (name_it == other_names.end()) {
                break;
            }
            unique_id += 1;
            unique_name = std::format("{} ({})", name, unique_id);
        }
        if (clone_index)
            *clone_index = unique_id;
        return unique_name;
    }
}  // namespace Toolbox::UI::Util