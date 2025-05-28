#pragma once

#include "core/mimedata/mimedata.hpp"
#include "gui/dragdrop/dropaction.hpp"
#include "image/imagehandle.hpp"
#include "unique.hpp"
#include <imgui/imgui.h>

namespace Toolbox::UI {

    class DragAction {
        friend class DragDropManager;
        friend class DragEvent;

    protected:
        DragAction() = default;

    public:
        using render_fn = std::function<void(const ImVec2 &pos, const ImVec2 &size)>;

        DragAction(UUID64 source_uuid) : m_source_uuid(source_uuid) {}
        DragAction(const DragAction &) = default;
        DragAction(DragAction &&)      = default;

        void render(const ImVec2 &size) const {
            if (m_render) {
                m_render(ImGui::GetWindowPos(), size);
            }
        }

        struct DropState {
            bool m_valid_target;
            DropType m_pending_action;
        };

        const DropState &getDropState() const { return m_drop_state; }

        [[nodiscard]] const ImVec2 &getHotSpot() const { return m_hot_spot; }

        [[nodiscard]] const MimeData &getPayload() const { return m_mime_data; }
        [[nodiscard]] DropType getDefaultDropType() const { m_default_drop_type; }
        [[nodiscard]] DropTypes getSupportedDropTypes() const { return m_supported_drop_types; }

        [[nodiscard]] UUID64 getSourceUUID() const { return m_source_uuid; }
        [[nodiscard]] UUID64 getTargetUUID() const { return m_target_uuid; }

        void setHotSpot(const ImVec2 &absp) { m_hot_spot = absp; }
        void setRender(render_fn render) { m_render = render; }
        void setPayload(const MimeData &data) { m_mime_data = data; }
        void setTargetUUID(const UUID64 &uuid) { m_target_uuid = uuid; }
        void setSupportedDropTypes(DropTypes types) { m_supported_drop_types = types; }

    protected:
        void setDropState(DropState &&state) { m_drop_state = std::move(state); }

    private:
        ImVec2 m_hot_spot;
        render_fn m_render;
        DropState m_drop_state;

        MimeData m_mime_data;
        DropType m_default_drop_type     = DropType::ACTION_MOVE;
        DropTypes m_supported_drop_types = DropType::ACTION_COPY | DropType::ACTION_MOVE;

        UUID64 m_source_uuid = 0;
        UUID64 m_target_uuid = 0;
    };

}  // namespace Toolbox::UI