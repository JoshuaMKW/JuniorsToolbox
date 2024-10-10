#include <fstream>

#include "image/imagehandle.hpp"

#include "core/memory.hpp"
#include "fsystem.hpp"

#include <glad/glad.h>
#include <imgui/imgui.h>

#include <stb/stb_image.h>

namespace Toolbox {

    ImageHandle::ImageHandle(const ImageHandle &other) { copyGL(other); }

    ImageHandle::ImageHandle(ImageHandle &&other) noexcept { moveGL(std::move(other)); }

    ImageHandle::ImageHandle(const std::filesystem::path &res_path) {
        int width, height, channels;
        stbi_uc *data = stbi_load(res_path.string().c_str(), &width, &height, &channels, 0);

        Buffer data_buf;
        data_buf.setBuf(data, static_cast<size_t>(width * height * channels));

        loadGL(data_buf, channels, width, height);

        stbi_image_free(data);
    }

    ImageHandle::ImageHandle(std::span<u8> data) {
        int width, height, channels;
        stbi_uc *stbi_buf =
            stbi_load_from_memory(data.data(), data.size(), &width, &height, &channels, 0);

        Buffer data_buf;
        data_buf.setBuf(stbi_buf, static_cast<size_t>(width * height * channels));

        loadGL(data_buf, channels, width, height);

        stbi_image_free(stbi_buf);
    }

    ImageHandle::ImageHandle(std::span<u8> data, int channels, int dx, int dy) {
        Buffer data_buf;
        data_buf.setBuf(data.data(), data.size());

        loadGL(data_buf, channels, dx, dy);
    }

    ImageHandle::ImageHandle(const Buffer &data) {
        int width, height, channels;
        stbi_uc *stbi_buf =
            stbi_load_from_memory(data.buf<u8>(), data.size(), &width, &height, &channels, 0);

        Buffer data_buf;
        data_buf.setBuf(stbi_buf, static_cast<size_t>(width * height * channels));

        loadGL(data_buf, channels, width, height);

        stbi_image_free(stbi_buf);
    }

    ImageHandle::ImageHandle(const ImageData &data) {
        loadGL(data.m_data, data.getChannels(), data.getWidth(), data.getHeight());
    }

    ImageHandle::ImageHandle(const Buffer &data, int channels, int dx, int dy) {
        loadGL(data, channels, dx, dy);
    }

    bool ImageHandle::isValid() const noexcept {
        return m_image_handle != std::numeric_limits<u64>::max();
    }

    ImageHandle::~ImageHandle() { unloadGL(); }

    ImageHandle &ImageHandle::operator=(const ImageHandle &other) {
        copyGL(other);
        return *this;
    }

    ImageHandle &ImageHandle::operator=(ImageHandle &&other) noexcept {
        moveGL(std::move(other));
        return *this;
    }

    ImageHandle::operator bool() const noexcept { return isValid(); }

    void ImageHandle::loadGL(const Buffer &data, int channels, int dx, int dy) {
        // Generate a texture object
        GLuint handle;
        glGenTextures(1, &handle);

        // Bind the texture object
        glBindTexture(GL_TEXTURE_2D, handle);

        // Set the texture's stretching properties
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLint format;
        switch (channels) {
        case 1:
            format = GL_RED;
            break;
        case 3:
        default:
            format = GL_RGB;
            break;
        case 4:
            format = GL_RGBA;
            break;
        }

        // Upload the texture data
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, dx, dy, 0, format, GL_UNSIGNED_BYTE, data.buf());

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);

        m_image_handle = (u64)handle;
        m_image_width  = dx;
        m_image_height = dy;
        m_image_format = format;
    }

    void ImageHandle::copyGL(const ImageHandle &image) {
        unloadGL();
        GLuint handle;
        if (image.isValid()) {
            glGenTextures(1, &handle);
            glBindTexture(GL_TEXTURE_2D, handle);
            glCopyTexImage2D(GL_TEXTURE_2D, 0, image.m_image_format, 0, 0, image.m_image_width,
                             image.m_image_height, 0);
            m_image_handle = (u64)handle;
            m_image_format = image.m_image_format;
            m_image_width  = image.m_image_width;
            m_image_height = image.m_image_height;
        } else {
            m_image_handle = std::numeric_limits<u64>::max();
            m_image_format = 0;
            m_image_width  = 0;
            m_image_height = 0;
        }
    }

    void ImageHandle::moveGL(ImageHandle &&image) {
        unloadGL();
        m_image_handle       = image.m_image_handle;
        m_image_format       = image.m_image_format;
        m_image_width        = image.m_image_width;
        m_image_height       = image.m_image_height;
        image.m_image_handle = std::numeric_limits<u64>::max();
    }

    void ImageHandle::unloadGL() {
        if (!isValid()) {
            return;
        }
        GLuint handle = (GLuint)m_image_handle;
        glDeleteTextures(1, &handle);
        m_image_handle = std::numeric_limits<u64>::max();
        m_image_format = 0;
        m_image_width  = 0;
        m_image_height = 0;
    }

}  // namespace Toolbox