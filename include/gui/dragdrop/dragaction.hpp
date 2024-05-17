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
        DragAction(const DragAction &) = default;
        DragAction(DragAction &&)      = default;

        [[nodiscard]] ImVec2 getHotSpot() const { return m_hot_spot; }
        [[nodiscard]] const ImageHandle &getImage() { return m_image_handle; }

        [[nodiscard]] const MimeData &getMimeData() const { return m_mime_data; }
        [[nodiscard]] DropType getDefaultDropType() const { m_default_drop_type; }
        [[nodiscard]] DropTypes getSupportedDropTypes() const { return m_supported_drop_types; }

        [[nodiscard]] UUID64 getSourceUUID() const { return m_source_uuid; }
        [[nodiscard]] std::optional<UUID64> getTargetUUID() const { return m_source_uuid; }

        void setHotSpot(const ImVec2 &absp) { m_hot_spot = absp; }
        void setImage(const ImageHandle &image) { m_image_handle = image; }
        void setMimeData(const MimeData &mimedata) { m_mime_data = mimedata; }

        void cancel();
        DropType execute(DropTypes supported_drop_types = DropType::ACTION_MOVE);
        DropType execute(DropTypes supported_drop_types, DropType default_drop_type);

    private:
        ImVec2 m_hot_spot;
        ImageHandle m_image_handle;

        MimeData m_mime_data;
        DropType m_default_drop_type;
        DropType m_supported_drop_types;

        UUID64 m_source_uuid;
        std::optional<UUID64> m_target_uuid;
    };

}  // namespace Toolbox::UI