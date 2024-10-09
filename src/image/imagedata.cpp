#include "image/imagedata.hpp"

#include <stb/stb_image.h>

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb/stb_image_write.h>

namespace Toolbox {

    ImageData::ImageData(const fs_path &res_path) {
        stbi_uc *data = stbi_load(res_path.string().c_str(), &m_width, &m_height, &m_channels, 0);
        m_data.setBuf(data, static_cast<size_t>(m_width * m_height * m_channels));
    }

    ImageData::ImageData(std::span<u8> data) {
        stbi_uc *stbi_buf =
            stbi_load_from_memory(data.data(), data.size(), &m_width, &m_height, &m_channels, 0);
        m_data.setBuf(stbi_buf, static_cast<size_t>(m_width * m_height * m_channels));
    }

    ImageData::ImageData(std::span<u8> data, int channels, int dx, int dy)
        : m_channels(channels), m_width(dx), m_height(dy) {
        m_data.setBuf(data.data(), data.size());
    }

    ImageData::ImageData(const Buffer &data) {
        stbi_uc *stbi_buf =
            stbi_load_from_memory(data.buf<u8>(), data.size(), &m_width, &m_height, &m_channels, 0);
        m_data.setBuf(stbi_buf, static_cast<size_t>(m_width * m_height * m_channels));
    }

    ImageData::ImageData(Buffer &&data, int channels, int dx, int dy)
        : m_channels(channels), m_width(dx), m_height(dy) {
        m_data = std::move(data);
    }

    ImageData::~ImageData() { 
        if (m_data) {
            //stbi_image_free(m_data.buf<u8>());
            m_data.free();
        }
    }

    bool ImageData::save(const fs_path &filename) const {
        stbi_write_png(filename.string().c_str(), m_width, m_height, m_channels, m_data.buf<u8>(),
                       0);
        return true;
    }

}  // namespace Toolbox
