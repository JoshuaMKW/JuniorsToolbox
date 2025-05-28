#include "image/imagebuilder.hpp"
#include "image/imagehandle.hpp"

#include <stb/stb_image.h>

#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif
#include <stb/stb_image_resize2.h>

#include <glad/glad.h>
#include <imgui/imgui.h>

namespace Toolbox::UI {

    using pixel_pix_op_1_t   = std::function<u8(u8)>;
    using pixel_color_op_1_t = std::function<void(u8 *, const u8 *, int)>;

    using pixel_pix_op_2_t   = std::function<u8(u8, u8)>;
    using pixel_color_op_2_t = std::function<void(u8 *, const u8 *, const u8 *, int, int, int)>;

    static ScopePtr<ImageData> _ImageApplyOperationPix(const ImageData &a,
                                                       pixel_pix_op_1_t operation) {
        const u8 *A = a.getData();

        int channels = a.getChannels();
        int height   = a.getHeight();
        int width    = a.getWidth();

        Buffer result_buf;
        result_buf.alloc(width * height * channels);

        // Apply the operation to the overlapping region
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                // Process for channels in A and B
                for (int ch = 0; ch < channels; ++ch) {
                    int ind  = row * width * channels + col * channels + ch;
                    u8 pixel = (ch < channels) ? A[ind] : 0;

                    // Store in the result image
                    int ind_res         = row * width * channels + col * channels + ch;
                    result_buf[ind_res] = operation(pixel);
                }
            }
        }

        return make_scoped<ImageData>(std::move(result_buf), channels, width, height);
    }

    static ScopePtr<ImageData> _ImageApplyOperationColor(const ImageData &a,
                                                         pixel_color_op_1_t operation) {
        const u8 *A = a.getData();

        int channels = a.getChannels();
        int height   = a.getHeight();
        int width    = a.getWidth();

        Buffer result_buf;
        result_buf.alloc(width * height * channels);

        // Apply the operation to the overlapping region
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                // Process for channels in A and B
                for (int ch = 0; ch < channels; ++ch) {
                    int ind  = row * width * channels + col * channels + ch;
                    u8 pixel = (ch < channels) ? A[ind] : 0;

                    // Store in the result image
                    int ind_res         = row * width * channels + col * channels + ch;
                    operation(result_buf.buf<u8>() + ind_res, A + ind, channels);
                }
            }
        }

        return make_scoped<ImageData>(std::move(result_buf), channels, width, height);
    }

    static ScopePtr<ImageData> _ImageApplyOperationPix(const ImageData &a, const ImageData &b,
                                                       pixel_pix_op_2_t operation) {
        const u8 *A = a.getData();
        const u8 *B = b.getData();

        int chan_a = a.getChannels();
        int chan_b = b.getChannels();

        int rows_a = a.getHeight();
        int cols_a = a.getWidth();
        int rows_b = b.getHeight();
        int cols_b = b.getWidth();

        // Determine the intersection size
        int min_height = std::min(rows_a, rows_b);
        int min_width  = std::min(cols_a, cols_b);

        int max_height      = std::max(rows_a, rows_b);
        int max_width       = std::max(cols_a, cols_b);
        int result_channels = std::max(chan_a, chan_b);

        Buffer result_buf;
        result_buf.alloc(max_width * max_height * result_channels);

        // Apply the operation to the overlapping region
        for (int row = 0; row < min_height; ++row) {
            for (int col = 0; col < min_width; ++col) {
                // Process for channels in A and B
                for (int ch = 0; ch < result_channels; ++ch) {
                    int ind_a  = row * cols_a * chan_a + col * chan_a + ch;
                    int ind_b  = row * cols_b * chan_b + col * chan_b + ch;
                    u8 pixel_A = (ch < chan_a) ? A[ind_a] : 0;
                    u8 pixel_B = (ch < chan_b) ? B[ind_b] : 0;

                    // Store in the result image
                    int ind_res = row * max_width * result_channels + col * result_channels + ch;
                    result_buf[ind_res] = operation(pixel_A, pixel_B);
                }
            }
        }

        // Copy the rest of A if it's bigger than B
        for (int row = min_height; row < rows_a; ++row) {
            for (int col = min_width; col < cols_a; ++col) {
                // Skip pixels that are already handled in the overlap region
                if (row < min_height && col < min_width)
                    continue;

                for (int ch = 0; ch < result_channels; ++ch) {
                    u8 pixel_A = (ch < chan_a) ? A[row * cols_a * chan_a + col * chan_a + ch] : 0;
                    result_buf[row * max_width * result_channels + col * result_channels + ch] =
                        pixel_A;
                }
            }
        }

        // Copy the rest of B if it's bigger than A
        for (int row = min_height; row < rows_b; ++row) {
            for (int col = min_width; col < cols_b; ++col) {
                // Skip pixels that are already handled in the overlap region
                if (row < min_height && col < min_width)
                    continue;

                for (int ch = 0; ch < result_channels; ++ch) {
                    u8 pixel_B = (ch < chan_b) ? B[row * cols_b * chan_b + col * chan_b + ch] : 0;
                    result_buf[row * max_width * result_channels + col * result_channels + ch] =
                        pixel_B;
                }
            }
        }

        return make_scoped<ImageData>(std::move(result_buf), result_channels, max_width,
                                      max_height);
    }

    static ScopePtr<ImageData> _ImageApplyOperationColor(const ImageData &a, const ImageData &b,
                                                         pixel_color_op_2_t operation) {
        const u8 *A = a.getData();
        const u8 *B = b.getData();

        int chan_a = a.getChannels();
        int chan_b = b.getChannels();

        int rows_a = a.getHeight();
        int cols_a = a.getWidth();
        int rows_b = b.getHeight();
        int cols_b = b.getWidth();

        // Determine the intersection size
        int min_height = std::min(rows_a, rows_b);
        int min_width  = std::min(cols_a, cols_b);

        int max_height      = std::max(rows_a, rows_b);
        int max_width       = std::max(cols_a, cols_b);
        int result_channels = std::max(chan_a, chan_b);

        Buffer result_buf;
        result_buf.alloc(max_width * max_height * result_channels);

        // Apply the operation to the overlapping region
        for (int row = 0; row < min_height; ++row) {
            for (int col = 0; col < min_width; ++col) {
                // Process for channels in A and B
                int ind_a   = row * cols_a * chan_a + col * chan_a;
                int ind_b   = row * cols_b * chan_b + col * chan_b;
                int ind_res = row * max_width * result_channels + col * result_channels;
                operation(result_buf.buf<u8>() + ind_res, A + ind_a, B + ind_b, result_channels,
                          chan_a, chan_b);
            }
        }

        // Copy the rest of A if it's bigger than B
        for (int row = min_height; row < rows_a; ++row) {
            for (int col = min_width; col < cols_a; ++col) {
                // Skip pixels that are already handled in the overlap region
                if (row < min_height && col < min_width)
                    continue;

                for (int ch = 0; ch < result_channels; ++ch) {
                    u8 pixel_A = (ch < chan_a) ? A[row * cols_a * chan_a + col * chan_a + ch] : 0;
                    result_buf[row * max_width * result_channels + col * result_channels + ch] =
                        pixel_A;
                }
            }
        }

        // Copy the rest of B if it's bigger than A
        for (int row = min_height; row < rows_b; ++row) {
            for (int col = min_width; col < cols_b; ++col) {
                // Skip pixels that are already handled in the overlap region
                if (row < min_height && col < min_width)
                    continue;

                for (int ch = 0; ch < result_channels; ++ch) {
                    u8 pixel_B = (ch < chan_b) ? B[row * cols_b * chan_b + col * chan_b + ch] : 0;
                    result_buf[row * max_width * result_channels + col * result_channels + ch] =
                        pixel_B;
                }
            }
        }

        return make_scoped<ImageData>(std::move(result_buf), result_channels, max_width,
                                      max_height);
    }

    ScopePtr<ImageData> ImageBuilder::ImageAdd(const ImageData &a, const ImageData &b, float scale,
                                               float offset) {
        // Function to add two pixels safely (with clamping)
        auto add_pixel = [scale, offset](u8 px_a, u8 px_b) -> u8 {
            int sum = (static_cast<int>(px_a) + static_cast<int>(px_b)) / scale + (offset * 255.0f);
            return static_cast<u8>(std::min(255, sum));  // Clamp to 255
        };

        return _ImageApplyOperationPix(a, b, add_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageAddModulo(const ImageData &a, const ImageData &b) {
        // Function to add two pixels safely (with clamping)
        auto add_pixel = [](u8 px_a, u8 px_b) -> u8 {
            int sum = (static_cast<int>(px_a) + static_cast<int>(px_b));
            return static_cast<u8>(sum % 256);
        };

        return _ImageApplyOperationPix(a, b, add_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageBlend(const ImageData &a, const ImageData &b,
                                                 float alpha) {
        auto blend = [alpha](u8 px_a, u8 px_b) -> u8 {
            return static_cast<u8>(
                std::lerp(static_cast<float>(px_a), static_cast<float>(px_b), alpha));
        };

        return _ImageApplyOperationPix(a, b, blend);
    }

    ScopePtr<ImageData> ImageBuilder::ImageComposite(const ImageData &a, const ImageData &b) {
        auto composite = [](u8 *dst, const u8 *a, const u8 *b, int ch_dst, int ch_a,
                            int ch_b) -> void {
            // Composite the two images
            if (ch_b < 4) {
                for (int i = 0; i < ch_dst; ++i) {
                    dst[i] = (i < ch_b) ? b[i] : 0;
                }
                return;
            }

            float alpha = static_cast<float>(b[3]) / 255.0f;

            for (int ch = 0; ch < ch_dst; ++ch) {
                float pa = (ch < ch_a) ? static_cast<float>(a[ch]) : 0.0f;
                float pb = (ch < ch_b) ? static_cast<float>(b[ch]) : 0.0f;
                dst[ch]  = static_cast<u8>(std::lerp(pa, pb, alpha));
            }
        };

        return _ImageApplyOperationColor(a, b, composite);
    }

    ScopePtr<ImageData> ImageBuilder::ImageDarker(const ImageData &a, const ImageData &b) {
        auto darker = [](u8 px_a, u8 px_b) -> u8 { return std::min(px_a, px_b); };

        return _ImageApplyOperationPix(a, b, darker);
    }

    ScopePtr<ImageData> ImageBuilder::ImageDifference(const ImageData &a, const ImageData &b) {
        auto darker = [](u8 px_a, u8 px_b) -> u8 { return std::abs(px_a - px_b); };

        return _ImageApplyOperationPix(a, b, darker);
    }

    ScopePtr<ImageData> ImageBuilder::ImageDivide(const ImageData &a, const ImageData &b) {
        auto divide = [](u8 px_a, u8 px_b) -> u8 {
            float a = static_cast<float>(px_a) / 255.0f;
            float b = static_cast<float>(px_b) / 255.0f;
            int div = static_cast<int>((static_cast<float>(a) / static_cast<float>(b) * 255.0f));
            return static_cast<u8>(std::min(255, div));
        };

        return _ImageApplyOperationPix(a, b, divide);
    }

    ScopePtr<ImageData> ImageBuilder::ImageLighter(const ImageData &a, const ImageData &b) {
        auto lighter = [](u8 px_a, u8 px_b) -> u8 { return std::max(px_a, px_b); };

        return _ImageApplyOperationPix(a, b, lighter);
    }

    ScopePtr<ImageData> ImageBuilder::ImageMultiply(const ImageData &a, const ImageData &b) {
        auto divide = [](u8 px_a, u8 px_b) -> u8 {
            float a = static_cast<float>(px_a) / 255.0f;
            float b = static_cast<float>(px_b) / 255.0f;
            return static_cast<u8>((static_cast<float>(a) * static_cast<float>(b) * 255.0f));
        };

        return _ImageApplyOperationPix(a, b, divide);
    }

    ScopePtr<ImageData> ImageBuilder::ImageOffset(const ImageData &image, int d_x, int d_y) {
        int width  = image.getWidth();
        int height = image.getHeight();
        int ch     = image.getChannels();

        const u8 *A = image.getData();

        Buffer result_buf;
        result_buf.alloc(width * height * ch);
        result_buf.initTo(0);

        for (int y = 0; (y + d_y) < height; ++y) {
            for (int x = 0; (x + d_x) < width; ++x) {
                for (int c = 0; c < ch; ++ch) {
                    int src_ind         = y * width * c + x * c + c;
                    int dst_ind         = (y + d_y) * width * c + (x + d_x) * c + c;
                    result_buf[dst_ind] = A[src_ind];
                }
            }
        }

        return make_scoped<ImageData>(std::move(result_buf), ch, width, height);
    }

    ScopePtr<ImageData> ImageBuilder::ImageOverlay(const ImageData &a, const ImageData &b) {
        auto overlay = [](u8 px_a, u8 px_b) -> u8 {
            float a = static_cast<float>(px_a) / 255.0f;
            float b = static_cast<float>(px_b) / 255.0f;
            if (px_b < 128)
                return 2.0f * a * b * 255.0f;
            return (1.0f - 2.0f * (1.0f - a) * (1.0f - b)) * 255.0f;
        };

        return _ImageApplyOperationPix(a, b, overlay);
    }

    ScopePtr<ImageData> ImageBuilder::ImageResize(const ImageData &image, int width, int height) {
        Buffer result_buf;
        result_buf.alloc(width * height * image.getChannels());

        stbir_pixel_layout layouts[] = {STBIR_1CHANNEL, STBIR_2CHANNEL, STBIR_RGB, STBIR_RGBA};

        stbir_resize_uint8_linear(image.getData(), image.getWidth(), image.getHeight(), 0,
                                  result_buf.buf<u8>(), width, height, 0,
                                  layouts[image.getChannels()]);

        return make_scoped<ImageData>(std::move(result_buf), image.getChannels(), width, height);
    }

    ScopePtr<ImageData> ImageBuilder::ImageScreen(const ImageData &a, const ImageData &b) {
        auto screen = [](u8 px_a, u8 px_b) -> u8 {
            float a = static_cast<float>(px_a) / 255.0f;
            float b = static_cast<float>(px_b) / 255.0f;
            return (1.0f - (1.0f - a) * (1.0f - b)) * 255.0f;
        };

        return _ImageApplyOperationPix(a, b, screen);
    }

    ScopePtr<ImageData> ImageBuilder::ImageSubtract(const ImageData &a, const ImageData &b,
                                                    float scale, float offset) {
        auto sub_pixel = [scale, offset](u8 px_a, u8 px_b) -> u8 {
            int sum = (static_cast<int>(px_a) - static_cast<int>(px_b)) / scale + (offset * 255.0f);
            return static_cast<u8>(std::min(255, sum));  // Clamp to 255
        };

        return _ImageApplyOperationPix(a, b, sub_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageSubtractModulo(const ImageData &a, const ImageData &b) {
        auto sub_pixel = [](u8 px_a, u8 px_b) -> u8 {
            int diff = (static_cast<int>(px_a) - static_cast<int>(px_b));
            return static_cast<u8>(((diff % 256) + 256) % 256);
        };

        return _ImageApplyOperationPix(a, b, sub_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageSwizzle(const ImageData &a, const SwizzleMatrix &mtx) {
        auto sub_pixel = [&mtx](u8 *dst, const u8 *a, int ch) -> void {
            for (int i = 0; i < ch; ++i) {
                dst[i] = a[mtx[i]];
            }
        };

        return _ImageApplyOperationColor(a, sub_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageAND(const ImageData &a, const ImageData &b) {
        auto sub_pixel = [](u8 px_a, u8 px_b) -> u8 { return px_a & px_b; };

        return _ImageApplyOperationPix(a, b, sub_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageOR(const ImageData &a, const ImageData &b) {
        auto sub_pixel = [](u8 px_a, u8 px_b) -> u8 { return px_a | px_b; };

        return _ImageApplyOperationPix(a, b, sub_pixel);
    }

    ScopePtr<ImageData> ImageBuilder::ImageXOR(const ImageData &a, const ImageData &b) {
        auto sub_pixel = [](u8 px_a, u8 px_b) -> u8 { return px_a ^ px_b; };

        return _ImageApplyOperationPix(a, b, sub_pixel);
    }

}  // namespace Toolbox::UI
