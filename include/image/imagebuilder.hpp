#pragma once

#include "image/imagedata.hpp"
#include "image/imagehandle.hpp"

namespace Toolbox::UI {

    class ImageBuilder {
    public:
        static ScopePtr<ImageData> ImageAdd(const ImageData &a, const ImageData &b,
                                            float scale = 1.0f, float offset = 0.0f);
        static ScopePtr<ImageData> ImageAddModulo(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageBlend(const ImageData &a, const ImageData &b,
                                              float alpha);
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
        static ScopePtr<ImageData> ImageAND(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageOR(const ImageData &a, const ImageData &b);
        static ScopePtr<ImageData> ImageXOR(const ImageData &a, const ImageData &b);
    };

}  // namespace Toolbox::UI