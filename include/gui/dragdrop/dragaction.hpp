#pragma once

#include <imgui/imgui.h>
#include "unique.hpp"
#include "core/mimedata/mimedata.hpp"
#include "image/imagehandle.hpp"
#include "gui/dragdrop/dropaction.hpp"

namespace Toolbox::UI {

    class DragAction {
        friend class DragDropManager;

    protected:
        DragAction()                   = default;

    public:
        DragAction(UUID64 source_uuid) : m_source_uuid(source_uuid) {}
        DragAction(const DragAction &) = default;
        DragAction(DragAction &&)      = default;

        [[nodiscard]] ImVec2 getHotSpot() const { return m_hot_spot; }
        [[nodiscard]] const ImageHandle &getImage() { return m_image_handle; }

        [[nodiscard]] const ImGuiPayload *getPayload() const { return m_imgui_payload; }
        [[nodiscard]] DropType getDefaultDropType() const { m_default_drop_type; }
        [[nodiscard]] DropTypes getSupportedDropTypes() const { return m_supported_drop_types; }

        [[nodiscard]] UUID64 getSourceUUID() const { return m_source_uuid; }
        [[nodiscard]] std::optional<UUID64> getTargetUUID() const { return m_source_uuid; }

        void setHotSpot(const ImVec2 &absp) { m_hot_spot = absp; }
        void setImage(const ImageHandle &image) { m_image_handle = image; }
        void setPayload(const ImGuiPayload *payload) { m_imgui_payload = payload; }

    private:
        ImVec2 m_hot_spot;
        ImageHandle m_image_handle;

        const ImGuiPayload *m_imgui_payload;
        DropType m_default_drop_type;
        DropType m_supported_drop_types;

        UUID64 m_source_uuid;
        std::optional<UUID64> m_target_uuid;
    };

}  // namespace Toolbox::UI