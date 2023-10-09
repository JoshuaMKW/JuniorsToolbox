#pragma once

#include <chrono>

using Clock = std::chrono::high_resolution_clock;

namespace Toolbox::UI::Util {
	Clock::time_point GetTime();

	float GetDeltaTime(Clock::time_point a, Clock::time_point b);
}
