#pragma once

#include <chrono>
#include <string_view>

using Clock = std::chrono::high_resolution_clock;

namespace Toolbox::UI::Util {
	Clock::time_point GetTime();

	float GetDeltaTime(Clock::time_point a, Clock::time_point b);

	

    std::string Utf8ToSjis(std::string_view value);
    std::string SjisToUtf8(std::string_view value);
}
