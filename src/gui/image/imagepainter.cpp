#pragma once

#include "gui/image/imagepainter.hpp"

namespace Toolbox::UI {

    bool Toolbox::UI::ImagePainter::render(const ImageHandle &image) const {
        return render(image, {(float)image.m_image_width, (float)image.m_image_height});
    }

    bool ImagePainter::render(const ImageHandle &image, const ImVec2 &size) const {
        return render(image, ImGui::GetCursorScreenPos(), size);
    }

    bool ImagePainter::render(const ImageHandle &image, const ImVec2 &pos,
                              const ImVec2 &size) const {
        if (!image) {
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        ImGui::Image((ImTextureID)image.m_image_handle, size, m_uv0, m_uv1, m_tint_color,
                     m_border_color);
        return true;
    }

}  // namespace Toolbox::UI