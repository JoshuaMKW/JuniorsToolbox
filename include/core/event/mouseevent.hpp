#pragma once

#include "core/event/event.hpp"
#include "core/input/input.hpp"
#include <string>

namespace Toolbox {

    class MouseEvent final : public BaseEvent {
    private:
        MouseEvent() = default;

    public:
        MouseEvent(const MouseEvent &)     = default;
        MouseEvent(MouseEvent &&) noexcept = default;

        MouseEvent(TypeID type, float pos_x, float pos_y,
                   Input::MouseButton button = Input::MouseButton::BUTTON_NONE,
                   Input::MouseButtonState state = Input::MouseButtonState::STATE_NONE);

        [[nodiscard]] bool isPressEvent() const noexcept {
            return m_mouse_press_state == Input::MouseButtonState::STATE_PRESS;
        }
        [[nodiscard]] bool isReleasedEvent() const noexcept {
            return m_mouse_press_state == Input::MouseButtonState::STATE_RELEASE;
        }
        [[nodiscard]] bool isHeldEvent() const noexcept {
            return m_mouse_press_state == Input::MouseButtonState::STATE_HELD;
        }
        [[nodiscard]] bool isUpdateEvent() const noexcept {
            return !isPressEvent() && !isReleasedEvent();
        }

        [[nodiscard]] Input::MouseButton getButton() const noexcept { return m_mouse_button; }
        [[nodiscard]] Input::MouseButtons getState() const noexcept { return m_mouse_state; }
        [[nodiscard]] std::pair<float, float> getGlobalPoint() const noexcept {
            return {m_screen_pos_x, m_screen_pos_y};
        }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        MouseEvent &operator=(const MouseEvent &)     = default;
        MouseEvent &operator=(MouseEvent &&) noexcept = default;

    private:
        float m_screen_pos_x;
        float m_screen_pos_y;
        Input::MouseButton m_mouse_button;
        Input::MouseButtons m_mouse_state;
        Input::MouseButtonState m_mouse_press_state;
    };

}  // namespace Toolbox