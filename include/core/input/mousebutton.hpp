#pragma once

#include <array>
#include <ostream>
#include <vector>

#include "core/types.hpp"

using namespace Toolbox;

namespace Toolbox::Input {

    enum class MouseButton : u16 {
        BUTTON_NONE   = std::numeric_limits<u16>::max(),
        BUTTON_0      = 0,
        BUTTON_1      = 1,
        BUTTON_2      = 2,
        BUTTON_3      = 3,
        BUTTON_4      = 4,
        BUTTON_5      = 5,
        BUTTON_LEFT   = BUTTON_0,
        BUTTON_RIGHT  = BUTTON_1,
        BUTTON_MIDDLE = BUTTON_2,
    };

    using MouseButtons = std::vector<MouseButton>;

    enum class MouseButtonState : u8 {
        STATE_NONE,
        STATE_PRESS,
        STATE_HELD,
        STATE_RELEASE,
    };

    inline std::ostream &operator<<(std::ostream &os, MouseButton button) {
        return (os << static_cast<s32>(button));
    }

    constexpr std::array<MouseButton, 6> GetMouseButtons() {
        return {
            MouseButton::BUTTON_0, MouseButton::BUTTON_1, MouseButton::BUTTON_2,
            MouseButton::BUTTON_3, MouseButton::BUTTON_4, MouseButton::BUTTON_5,
        };
    }

}  // namespace Toolbox::Input