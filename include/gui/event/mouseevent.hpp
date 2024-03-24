#pragma once

#include "core/input/input.hpp"
#include "gui/event/event.hpp"
#include <string>

namespace Toolbox::UI {

    class MouseEvent final : public BaseEvent {
    private:
        MouseEvent() = default;

    public:
        MouseEvent(const MouseEvent &)     = default;
        MouseEvent(MouseEvent &&) noexcept = default;

        MouseEvent(const UUID64 &target_id, TypeID type, float pos_x, float pos_y,
                   Input::MouseButton button     = Input::MouseButton::BUTTON_NONE,
                   Input::MouseButtonState state = Input::MouseButtonState::STATE_NONE);

        [[nodiscard]] bool isPressEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_PRESS || type == EVENT_MOUSE_PRESS_DBL ||
                   type == EVENT_MOUSE_PRESS_NON_CLIENT || type == EVENT_MOUSE_PRESS_DBL_NON_CLIENT;
        }

        [[nodiscard]] bool isDoubleClickEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_PRESS_DBL || type == EVENT_MOUSE_PRESS_DBL_NON_CLIENT;
        }

        [[nodiscard]] bool isNonClientEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_PRESS_NON_CLIENT || type == EVENT_MOUSE_PRESS_DBL_NON_CLIENT ||
                   type == EVENT_MOUSE_RELEASE_NON_CLIENT || type == EVENT_MOUSE_MOVE_NON_CLIENT;
        }

        [[nodiscard]] bool isReleaseEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_RELEASE || type == EVENT_MOUSE_RELEASE_NON_CLIENT;
        }

        [[nodiscard]] bool isUpdateEvent() const noexcept {
            return !isPressEvent() && !isReleaseEvent();
        }

        [[nodiscard]] bool isButtonHeld() const noexcept {
            return !isReleaseEvent() && m_mouse_press_state == Input::MouseButtonState::STATE_HELD;
        }

        [[nodiscard]] Input::MouseButton getButton() const noexcept { return m_mouse_button; }
        [[nodiscard]] Input::MouseButtons getHeldButtons() const noexcept { return m_mouse_state; }

        [[nodiscard]] void getGlobalPoint(float &x, float &y) const noexcept {
            x = m_screen_pos_x;
            y = m_screen_pos_y;
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