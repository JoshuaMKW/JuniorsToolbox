#pragma once

#include "gui/image/imagepainter.hpp"

namespace Toolbox::UI {

    bool ImagePainter::render(const ImageHandle &image) const {
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
        ImVec2 orig_pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(pos);
        ImGui::Image((ImTextureID)image.m_image_handle, size, m_uv0, m_uv1, m_tint_color,
                     m_border_color);
        ImGui::SetCursorScreenPos(orig_pos);
        return true;
    }

    bool ImagePainter::renderOverlay(const ImageHandle &image, const ImVec2 &pos,
                                     const ImVec2 &size) const {
        if (!image) {
            return false;
        }
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        draw_list->AddImage((ImTextureID)image.m_image_handle, pos, pos + size, m_uv0, m_uv1,
                                     ImGui::ColorConvertFloat4ToU32(m_border_color));
        return true;
    }

}  // namespace Toolbox::UI