#pragma once

#include "gui/dragdrop/dragaction.hpp"
#include "gui/event/event.hpp"

namespace Toolbox::UI {

    class DropEvent final : public BaseEvent {
    private:
        DropEvent() = default;

    protected:
        friend class ImWindow;

    public:
        DropEvent(const DropEvent &)     = default;
        DropEvent(DropEvent &&) noexcept = default;

        DropEvent(const ImVec2 &pos, RefPtr<DragAction> action);

        [[nodiscard]] ImVec2 getGlobalPoint() const noexcept { return m_screen_pos; }

        [[nodiscard]] const MimeData &getMimeData() const noexcept { return m_drag_action->getPayload(); }
        [[nodiscard]] DropTypes getSupportedDropTypes() const noexcept {
            return m_drag_action->getSupportedDropTypes() != DropType::ACTION_NONE
                       ? m_drag_action->getSupportedDropTypes()
                       : m_drag_action->getDefaultDropType();
        }
        [[nodiscard]] UUID64 getSourceId() const noexcept { return m_drag_action->getSourceUUID(); }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        DropEvent &operator=(const DropEvent &)     = default;
        DropEvent &operator=(DropEvent &&) noexcept = default;

    private:
        ImVec2 m_screen_pos;
        RefPtr<DragAction> m_drag_action;
    };

}  // namespace Toolbox::UI