#pragma once

#include <imgui/imgui_internal.h>
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

        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        const float cos_a = std::cosf(m_rotation);
        const float sin_a = std::sinf(m_rotation);

        const int vtx_idx_begin = draw_list->_VtxCurrentIdx;

        const auto [image_width, image_height] = image.size();

        ImGui::SetCursorScreenPos(pos);

        ImGui::Image((ImTextureID)image.m_image_handle, size, m_uv0, m_uv1, m_tint_color,
                     m_border_color);

        const int vtx_idx_end = draw_list->_VtxCurrentIdx;

        // Rotate and offset the generated vertices
        const ImVec2 pivot = pos + ImVec2(image_width * m_pivot.x, image_height * m_pivot.y);
        ImGui::ShadeVertsTransformPos(draw_list, vtx_idx_begin, vtx_idx_end, pivot, cos_a, sin_a,
                                      pivot);

        ImGui::SetCursorScreenPos(orig_pos);
        ImGui::Dummy({0, 0});
        return true;
    }

    bool ImagePainter::renderOverlay(const ImageHandle &image, const ImVec2 &pos,
                                     const ImVec2 &size) const {
        if (!image) {
            return false;
        }

        ImDrawList *draw_list = ImGui::GetForegroundDrawList();

        const float cos_a = std::cosf(m_rotation);
        const float sin_a = std::sinf(m_rotation);

        const int vtx_idx_begin = draw_list->_VtxCurrentIdx;

        const auto [image_width, image_height] = image.size();
        ImGui::SetCursorScreenPos(pos);

        draw_list->AddImage((ImTextureID)image.m_image_handle, pos, pos + size, m_uv0,
                            m_uv1,
                            ImGui::ColorConvertFloat4ToU32(m_border_color));

        const int vtx_idx_end = draw_list->_VtxCurrentIdx;

        // Rotate and offset the generated vertices
        const ImVec2 pivot = pos + ImVec2(image_width * m_pivot.x, image_height * m_pivot.y);
        ImGui::ShadeVertsTransformPos(draw_list, vtx_idx_begin, vtx_idx_end, pivot, cos_a, sin_a,
                                      pivot);

        return true;
    }

}  // namespace Toolbox::UI