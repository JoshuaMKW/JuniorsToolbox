#pragma once

#include "core/event/event.hpp"

namespace Toolbox {

    class ShortcutEvent final : public BaseEvent {
    private:
        ShortcutEvent() = default;

    public:
        ShortcutEvent(const ShortcutEvent &)     = default;
        ShortcutEvent(ShortcutEvent &&) noexcept = default;

        [[nodiscard]] bool isSystemBind() const;

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