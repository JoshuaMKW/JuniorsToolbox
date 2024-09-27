#pragma once

#include "core/memory.hpp"
#include "fsystem.hpp"
#include <imgui/imgui.h>
#include <span>

namespace Toolbox {

    namespace UI {
        class ImagePainter;
    }

    class ImageHandle {
        friend class UI::ImagePainter;

    public:
        ImageHandle() = default;
        ImageHandle(const ImageHandle &);
        ImageHandle(ImageHandle &&) noexcept;
        ~ImageHandle();

        ImageHandle(const std::filesystem::path &res_path);

        ImageHandle(std::span<u8> data);
        ImageHandle(std::span<u8> data, int channels, int dx, int dy);

        ImageHandle(const Buffer &data);

        // Data should be a valid RED, RGB, or RGBA image as loaded by stbi
        ImageHandle(const Buffer &data, int channels, int dx, int dy);

        [[nodiscard]] bool isValid() const noexcept;
        std::pair<int, int> size() const { return {m_image_width, m_image_height}; }

        ImageHandle &operator=(const ImageHandle &);
        ImageHandle &operator=(ImageHandle &&) noexcept;

        operator bool() const noexcept;

    protected:
        void loadGL(const Buffer &data, int channels, int dx, int dy);
        void copyGL(const ImageHandle &image);
        void moveGL(ImageHandle &&image);
        void unloadGL();

    private:
        u64 m_image_handle = std::numeric_limits<u64>::max();
        int m_image_width  = 0;
        int m_image_height = 0;
        u32 m_image_format = 0;
    };

}  // namespace Toolbox