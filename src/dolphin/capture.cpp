#include "dolphin/hook.hpp"
#include <algorithm>
#include <glad/glad.h>

namespace Toolbox::Dolphin {

    static Buffer YUV422ToRGB888(const u8 *yuv, int width, int height) {
        Buffer out;
        out.alloc(width * height * 3);
        out.initTo(0);

        for (int i = 0; i < width * height / 2; ++i) {  // Processing two pixels at a time
            int index = i * 4;
            int y1    = yuv[index];
            int y2    = yuv[index + 2];
            int u     = yuv[index + 1] - 128;
            int v     = yuv[index + 3] - 128;

            int rgbIndex = i * 6;  // Two pixels * 3 colors each

            // clang-format off
            // Convert first pixel (Y1, U, V) to RGB
            out.set<u8>(rgbIndex,     std::clamp<u8>(static_cast<u8>(y1 + 1.402 * v), 0, 255));
            out.set<u8>(rgbIndex + 1, std::clamp<u8>(static_cast<u8>(y1 - 0.344136 * u - 0.714136 * v), 0, 255));
            out.set<u8>(rgbIndex + 2, std::clamp<u8>(static_cast<u8>(y1 + 1.772 * u), 0, 255));

            // Convert second pixel (Y2, U, V) to RGB
            out.set<u8>(rgbIndex + 3, std::clamp<u8>(static_cast<u8>(y2 + 1.402 * v), 0, 255));
            out.set<u8>(rgbIndex + 4, std::clamp<u8>(static_cast<u8>(y2 - 0.344136 * u - 0.714136 * v), 0, 255));
            out.set<u8>(rgbIndex + 5, std::clamp<u8>(static_cast<u8>(y2 + 1.772 * u), 0, 255));
            // clang-format on
        }

        return out;
    }

    u32 DolphinHookManager::captureXFBAsTexture(int width, int height, u32 xfb_start, int xfb_width,
                                                int xfb_height) {
        if (xfb_start == 0 || xfb_width == 0 || xfb_height == 0)
            return 0xFFFFFFFF;

        std::vector<u8> xfb_data(xfb_width * xfb_height * 2);
        auto result = readBytes(reinterpret_cast<char *>(&xfb_data[0]), xfb_start, xfb_data.size());
        if (!result) {
            return 0xFFFFFFFF;
        }

        Buffer rgb_image = YUV422ToRGB888(xfb_data.data(), xfb_width, xfb_height);

        GLuint textureID;
        // Generate a texture object
        glGenTextures(1, &textureID);

        // Bind the texture object
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Set the texture's stretching properties
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Upload the texture data
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, xfb_width, xfb_height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     rgb_image.buf());

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);

        return textureID;
    }

}  // namespace Toolbox::Dolphin