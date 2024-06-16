#pragma once

#include "gui/dragdrop/dragaction.hpp"
#include "gui/event/event.hpp"

namespace Toolbox::UI {

    class DragEvent final : public BaseEvent {
    private:
        DragEvent() = default;

    public:
        DragEvent(const DragEvent &)     = default;
        DragEvent(DragEvent &&) noexcept = default;

        DragEvent(TypeID type, float pos_x, float pos_y, DragAction &&action);

        [[nodiscard]] void getGlobalPoint(float &x, float &y) const noexcept {
            x = m_screen_pos_x;
            y = m_screen_pos_y;
        }

        [[nodiscard]] DragAction &getDragAction() noexcept { return m_drag_action; }
        [[nodiscard]] UUID64 getSourceId() const noexcept { return m_drag_action.getSourceUUID(); }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        DragEvent &operator=(const DragEvent &)     = default;
        DragEvent &operator=(DragEvent &&) noexcept = default;

    private:
        float m_screen_pos_x;
        float m_screen_pos_y;
        DragAction m_drag_action;
    };

}  // namespace Toolbox::UI