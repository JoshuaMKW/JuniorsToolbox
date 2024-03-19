#pragma once

#include "core/event/event.hpp"
#include <string>

namespace Toolbox {

    class MouseEvent final : public BaseEvent {
    public:
        enum MouseButton {
            MOUSE_NONE,
            MOUSE_LEFT         = (1 << 0),
            MOUSE_RIGHT        = (1 << 1),
            MOUSE_MIDDLE       = (1 << 2),
            MOUSE_CONTEXT_UNDO = (1 << 3),
            MOUSE_CONTEXT_REDO = (1 << 4),
            MOUSE_TASK         = (1 << 5),
        };

        using MouseButtons = MouseButton;

    private:
        MouseEvent() = default;

    public:
        MouseEvent(const MouseEvent &)     = default;
        MouseEvent(MouseEvent &&) noexcept = default;

        [[nodiscard]] bool isPressEvent() const noexcept { return m_mouse_pressed; }
        [[nodiscard]] bool isReleasedEvent() const noexcept { return m_mouse_released; }
        [[nodiscard]] bool isUpdateEvent() const noexcept {
            return !isPressEvent() && !isReleasedEvent();
        }

        [[nodiscard]] MouseButton getButton() const noexcept { return m_mouse_button; }
        [[nodiscard]] MouseButtons getState() const noexcept { return m_mouse_state; }
        [[nodiscard]] std::pair<float, float> getGlobalPoint() const noexcept {
            return {m_screen_pos_x, m_screen_pos_y};
        }

        ScopePtr<ISmartResource> clone(bool deep) const override;

    private:
        float m_screen_pos_x;
        float m_screen_pos_y;
        MouseButton m_mouse_button;
        MouseButtons m_mouse_state;
        bool m_mouse_pressed;
        bool m_mouse_released;
    };

}  // namespace Toolbox