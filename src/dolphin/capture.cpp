#include "dolphin/hook.hpp"
#include <algorithm>
#include <glad/glad.h>

namespace Toolbox::Dolphin {

    static Buffer YUV422ToRGB888(const u8 *yuv, int width, int height) {
        Buffer out;
        out.alloc(width * height * 3);

        // YUV 4:2:2 each 4 bytes is 2 pixels
        // Y0 U0 Y1 V0
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 2) {
                int base_index = (y * width + x) * 2;

                // Scale colors from studio to full range
                int y0        = static_cast<int>((yuv[base_index] - 16) * 255 / 219);
                int y1        = static_cast<int>((yuv[base_index + 2] - 16) * 255 / 219);
                int u         = static_cast<int>((yuv[base_index + 1] - 128) * 255 / 224);
                int v         = static_cast<int>((yuv[base_index + 3] - 128) * 255 / 224);

                int r0 = std::clamp(static_cast<int>(y0 + 1.402 * v), 0, 255);
                int g0 = std::clamp(static_cast<int>(y0 - 0.344136 * u - 0.714136 * v), 0, 255);
                int b0 = std::clamp(static_cast<int>(y0 + 1.772 * u), 0, 255);

                int r1 = std::clamp(static_cast<int>(y1 + 1.402 * v), 0, 255);
                int g1 = std::clamp(static_cast<int>(y1 - 0.344136 * u - 0.714136 * v), 0, 255);
                int b1 = std::clamp(static_cast<int>(y1 + 1.772 * u), 0, 255);

                size_t rgb_index = static_cast<size_t>((y * width + x) * 3);

                out.set(rgb_index, static_cast<u8>(r0));
                out.set(rgb_index + 1, static_cast<u8>(g0));
                out.set(rgb_index + 2, static_cast<u8>(b0));

                out.set(rgb_index + 3, static_cast<u8>(r1));
                out.set(rgb_index + 4, static_cast<u8>(g1));
                out.set(rgb_index + 5, static_cast<u8>(b1));
            }
        }

        return out;
    }

    ImageHandle DolphinHookManager::captureXFBAsTexture(int width, int height, u32 xfb_start, int xfb_width,
                                                int xfb_height) {
        if (xfb_start == 0 || xfb_width == 0 || xfb_height == 0)
            return ImageHandle();

        std::vector<u8> xfb_data(xfb_width * xfb_height * 2);
        auto result = readBytes(reinterpret_cast<char *>(&xfb_data[0]), xfb_start, xfb_data.size());
        if (!result) {
            return ImageHandle();
        }

        Buffer rgb_image = YUV422ToRGB888(xfb_data.data(), xfb_width, xfb_height);
        return ImageHandle(rgb_image, 3, xfb_width, xfb_height);
    }

}  // namespace Toolbox::Dolphin