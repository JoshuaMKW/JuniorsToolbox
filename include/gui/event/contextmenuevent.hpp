#pragma once

#include "gui/context_menu.hpp"
#include "gui/event/event.hpp"

namespace Toolbox::UI {

    class ContextMenuEvent final : public BaseEvent {
    private:
        ContextMenuEvent() = default;

    public:
        ContextMenuEvent(const ContextMenuEvent &)     = default;
        ContextMenuEvent(ContextMenuEvent &&) noexcept = default;

        ContextMenuEvent(const UUID64 &target_id, float pos_x, float pos_y);

        [[nodiscard]] void getGlobalPoint(float &x, float &y) const noexcept {
            x = m_screen_pos_x;
            y = m_screen_pos_y;
        }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        ContextMenuEvent &operator=(const ContextMenuEvent &)     = default;
        ContextMenuEvent &operator=(ContextMenuEvent &&) noexcept = default;

    private:
        float m_screen_pos_x;
        float m_screen_pos_y;
    };

}  // namespace Toolbox::UI