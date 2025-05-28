#pragma once

#include "core/core.hpp"
#include "color.hpp"
#include "fsystem.hpp"

#include <span>

namespace Toolbox {

    class ImageData {
    public:
        friend class ImageBuilder;
        friend class ImageHandle;

        ImageData()                      = default;
        ImageData(const ImageData &)     = default;
        ImageData(ImageData &&) noexcept = default;

        ~ImageData();

        ImageData(const fs_path &res_path);

        ImageData(std::span<u8> data);
        ImageData(std::span<u8> data, int channels, int dx, int dy);

        ImageData(const Buffer &data);

        // Data should be a valid RED, RGB, or RGBA image as loaded by stbi
        ImageData(Buffer &&data, int channels, int dx, int dy);

        [[nodiscard]] bool save(const fs_path &filename) const;

        [[nodiscard]] bool isValid() const noexcept {
            return m_width > 0 && m_height > 0 && m_channels > 0;
        }

        [[nodiscard]] int getWidth() const noexcept { return m_width; }
        [[nodiscard]] int getHeight() const noexcept { return m_height; }
        [[nodiscard]] int getChannels() const noexcept { return m_channels; }

        [[nodiscard]]
        [[nodiscard]] const u8 *getData() const noexcept { return m_data.buf<u8>(); }
        [[nodiscard]] size_t getSize() const noexcept { return m_data.size(); }

        ImageData &operator=(const ImageData &)     = default;
        ImageData &operator=(ImageData &&) noexcept = default;

    private:
        int m_width    = 0;
        int m_height   = 0;
        int m_channels = 0;
        Buffer m_data;
    };

}  // namespace Toolbox