#include "gui/util.hpp"

namespace Toolbox::UI::Util {
	Clock::time_point GetTime() {
		return Clock::now();
	}

	float GetDeltaTime(Clock::time_point a, Clock::time_point b) {
		auto seconds = std::chrono::duration<float>{ b - a };

		return seconds.count();
	}
}