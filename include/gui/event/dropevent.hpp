#pragma once

#include "gui/dragdrop/dragaction.hpp"
#include "gui/event/event.hpp"

namespace Toolbox::UI {

    class DropEvent final : public BaseEvent {
    private:
        DropEvent() = default;

    public:
        DropEvent(const DropEvent &)     = default;
        DropEvent(DropEvent &&) noexcept = default;

        DropEvent(float pos_x, float pos_y, DropType drop_type, const DragAction &action);

        [[nodiscard]] void getGlobalPoint(float &x, float &y) const noexcept {
            x = m_screen_pos_x;
            y = m_screen_pos_y;
        }

        [[nodiscard]] DropType getDropType() const noexcept { return m_drop_type; }
        [[nodiscard]] UUID64 getSourceId() const noexcept { return m_source_id; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        DropEvent &operator=(const DropEvent &)     = default;
        DropEvent &operator=(DropEvent &&) noexcept = default;

    private:
        float m_screen_pos_x;
        float m_screen_pos_y;
        DropType m_drop_type;
        UUID64 m_source_id;
    };

}  // namespace Toolbox::UI