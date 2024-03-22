#pragma once

#include "core/event/event.hpp"
#include "core/input/input.hpp"
#include <string>

namespace Toolbox {

    class WindowEvent final : public BaseEvent {
    private:
        WindowEvent() = default;

    public:
        WindowEvent(const WindowEvent &)     = default;
        WindowEvent(WindowEvent &&) noexcept = default;

        WindowEvent(TypeID type, float pos_x, float pos_y, float size_x, float size_y);

        [[nodiscard]] bool isHideEvent() const noexcept { return getType() == EVENT_WINDOW_HIDE; }

        [[nodiscard]] bool isMoveEvent() const noexcept { return getType() == EVENT_WINDOW_MOVE; }

        [[nodiscard]] bool isResizeEvent() const noexcept {
            return getType() == EVENT_WINDOW_RESIZE;
        }

        [[nodiscard]] bool isShowEvent() const noexcept { return getType() == EVENT_WINDOW_SHOW; }

        [[nodiscard]] void getGlobalPoint(float &x, float &y) const noexcept {
            x = m_screen_pos_x;
            y = m_screen_pos_y;
        }

        [[nodiscard]] void getSize(float &x, float &y) const noexcept {
            x = m_size_x;
            y = m_size_y;
        }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        WindowEvent &operator=(const WindowEvent &)     = default;
        WindowEvent &operator=(WindowEvent &&) noexcept = default;

    private:
        float m_screen_pos_x;
        float m_screen_pos_y;
        float m_size_x;
        float m_size_y;
    };

}  // namespace Toolbox