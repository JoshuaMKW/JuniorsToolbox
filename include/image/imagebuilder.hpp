#pragma once

#include "image/imagedata.hpp"
#include "image/imagehandle.hpp"

namespace Toolbox::UI {

    class ImageBuilder {
    public:
        enum EPixelChannel {
            RED   = 0,
            GREEN = 1,
            BLUE  = 2,
            ALPHA = 3,
        };

        struct SwizzleMatrix {
            EPixelChannel m_swizzle_channels[4];

            const EPixelChannel &operator[](int chnl) const { return m_swizzle_channels[chnl]; }
            EPixelChannel &operator[](int chnl) { return m_swizzle_channels[chnl]; }
        };

        static ScopePtr<ImageData> ImageAdd(const ImageData &a, const ImageData &b,
                                            float scale = 1.0f, float offset = 0.0f);
        static ScopePtr<ImageData> ImageAddModulo(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageBlend(const ImageData &a, const ImageData &b, float alpha);
        static ScopePtr<ImageData> ImageComposite(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageDarker(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageDifference(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageDivide(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageLighter(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageMultiply(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageOffset(const ImageData &image, int x, int y);
        static ScopePtr<ImageData> ImageOverlay(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageResize(const ImageData &image, int width, int height);
        static ScopePtr<ImageData> ImageScreen(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageSubtract(const ImageData &a, const ImageData &b,
                                                 float scale = 1.0f, float offset = 0.0f);
        static ScopePtr<ImageData> ImageSubtractModulo(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageSwizzle(const ImageData &a, const SwizzleMatrix &mtx);
        static ScopePtr<ImageData> ImageAND(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageOR(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageXOR(const ImageData &a, const ImageData &b);
    };

}  // namespace Toolbox::UI