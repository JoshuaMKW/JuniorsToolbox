#pragma once

#include "image/imagehandle.hpp"

namespace Toolbox::UI {

    class ImagePainter {
    public:
        ImagePainter()                        = default;
        ImagePainter(const ImagePainter &)    = default;
        ImagePainter(ImagePainter &) noexcept = default;

        ImVec2 getUV0(const ImVec2 &uv0) const { return m_uv0; }
        ImVec2 getUV1(const ImVec2 &uv1) const { return m_uv1; }
        ImVec4 getTintColor(const ImVec4 &color) const { return m_tint_color; }
        ImVec4 getBorderColor(const ImVec4 &color) const { return m_border_color; }

        void setUV0(const ImVec2 &uv0) { m_uv0 = uv0; }
        void setUV1(const ImVec2 &uv1) { m_uv1 = uv1; }
        void setTintColor(const ImVec4 &color) { m_tint_color = color; }
        void setBorderColor(const ImVec4 &color) { m_border_color = color; }

        bool render(const ImageHandle &image) const;
        bool render(const ImageHandle &image, const ImVec2 &size) const;
        bool render(const ImageHandle &image, const ImVec2 &pos, const ImVec2 &size) const;

        bool renderOverlay(const ImageHandle &image, const ImVec2 &pos, const ImVec2 &size) const;

    private:
        ImVec2 m_uv0 = {0, 0};
        ImVec2 m_uv1 = {1, 1};
        ImVec4 m_tint_color = {1, 1, 1, 1};
        ImVec4 m_border_color = {0, 0, 0, 0};
    };

}  // namespace Toolbox::UI