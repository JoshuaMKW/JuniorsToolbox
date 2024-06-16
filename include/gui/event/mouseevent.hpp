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

        MouseEvent(const UUID64 &target_id, TypeID type, const ImVec2 &pos,
                   Input::MouseButton button     = Input::MouseButton::BUTTON_NONE,
                   Input::MouseButtonState state = Input::MouseButtonState::STATE_NONE,
                   bool is_client                = true);

        [[nodiscard]] bool isPressEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_PRESS || type == EVENT_MOUSE_PRESS_DBL;
        }

        [[nodiscard]] bool isDoubleClickEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_PRESS_DBL;
        }

        [[nodiscard]] bool isNonClientEvent() const noexcept { return !m_is_client; }

        [[nodiscard]] bool isReleaseEvent() const noexcept {
            TypeID type = getType();
            return type == EVENT_MOUSE_RELEASE;
        }

        [[nodiscard]] bool isUpdateEvent() const noexcept {
            return !isPressEvent() && !isReleaseEvent();
        }

        [[nodiscard]] bool isButtonHeld() const noexcept {
            return !isReleaseEvent() && m_mouse_press_state == Input::MouseButtonState::STATE_HELD;
        }

        [[nodiscard]] Input::MouseButton getButton() const noexcept { return m_mouse_button; }
        [[nodiscard]] Input::MouseButtons getHeldButtons() const noexcept { return m_mouse_state; }

        [[nodiscard]] ImVec2 getGlobalPoint() const noexcept { return m_screen_pos; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        MouseEvent &operator=(const MouseEvent &)     = default;
        MouseEvent &operator=(MouseEvent &&) noexcept = default;

    private:
        ImVec2 m_screen_pos;
        Input::MouseButton m_mouse_button;
        Input::MouseButtons m_mouse_state;
        Input::MouseButtonState m_mouse_press_state;
        bool m_is_client;
    };

}  // namespace Toolbox::UI