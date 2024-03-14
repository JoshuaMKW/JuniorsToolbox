#pragma once

#include <imgui/imgui.h>
#include "core/mimedata/mimedata.hpp"
#include "gui/image/imagehandle.hpp"

namespace Toolbox::UI {

    class DragAction {
    public:
        DragAction()                   = default;
        DragAction(const DragAction &) = delete;
        DragAction(DragAction &&)      = default;

        [[nodiscard]] const ImageHandle &getImage() { return m_image_handle; }
        [[nodiscard]] ImVec2 getHotSpot() const { return m_hot_spot; }
        [[nodiscard]] const MimeData &getMimeData() const { return m_mime_data; }

        void setHotSpot(const ImVec2 &absp) { m_hot_spot = absp; }
        void setImage(const ImageHandle &image) { m_image_handle = image; }
        void setMimeData(const MimeData &mimedata) { m_mime_data = mimedata; }

    private:
        ImVec2 m_hot_spot;
        MimeData m_mime_data;
        ImageHandle m_image_handle;
    };

}  // namespace Toolbox::UI