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

        DropEvent(const ImVec2 &pos, DropType drop_type, const DragAction &action);

        DropEvent(const ImVec2 &pos, DropType drop_type, UUID64 source_uuid, UUID64 target_uuid,
                  MimeData &&data);

        [[nodiscard]] ImVec2 getGlobalPoint() const noexcept { return m_screen_pos; }

        [[nodiscard]] const MimeData &getMimeData() const noexcept { return m_mime_data; }
        [[nodiscard]] DropType getDropType() const noexcept { return m_drop_type; }
        [[nodiscard]] UUID64 getSourceId() const noexcept { return m_source_id; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        DropEvent &operator=(const DropEvent &)     = default;
        DropEvent &operator=(DropEvent &&) noexcept = default;

    private:
        ImVec2 m_screen_pos;
        DropType m_drop_type = DropType::ACTION_NONE;
        MimeData m_mime_data;
        UUID64 m_source_id;
    };

}  // namespace Toolbox::UI