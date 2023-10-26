#pragma once

#include <chrono>

using Clock = std::chrono::high_resolution_clock;

namespace Toolbox::UI::Util {
	Clock::time_point GetTime();

	float GetDeltaTime(Clock::time_point a, Clock::time_point b);

	

    std::string Utf8ToSjis(const std::string &value);
    std::string SjisToUtf8(const std::string &value);
}
