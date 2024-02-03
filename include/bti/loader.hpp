#pragma once

#include "serial.hpp"
#include "core/types.hpp"
#include <include/decode.h>

namespace Toolbox::Texture {

    enum class EncodingFormat {
        I4,
        I8,
        IA4,
        IA8,
        RGB565,
        RGB5A3,
        RGBA32,
        C4 = 8,
        C8,
        C14X2,
        CMPR = 14
    };

    enum class WrapMode {
        Clamp,
        Repeat,
        Mirror
    };

    struct RGB8Texture {
        EncodingFormat m_original_texture_fmt;
        EncodingFormat m_original_palette_fmt;
        //...
    };

    std::vector<u8> TextureFromBTI(Deserializer &in) {
        EncodingFormat encoding;
        u16 width, height;
        in.pushBreakpoint();
        {
            encoding = static_cast<EncodingFormat>(in.read<u16, std::endian::big>());
            width    = in.read<u16, std::endian::big>();
            height   = in.read<u16, std::endian::big>();
        }
        in.popBreakpoint();
        in.seek(0x20);

        std::vector<u8> result;
        std::vector<u8> src;

        switch (encoding) {
        case Toolbox::Texture::EncodingFormat::I4:
            break;
        case Toolbox::Texture::EncodingFormat::I8:
            break;
        case Toolbox::Texture::EncodingFormat::IA4:
            break;
        case Toolbox::Texture::EncodingFormat::IA8:
            break;
        case Toolbox::Texture::EncodingFormat::RGB565:
            break;
        case Toolbox::Texture::EncodingFormat::RGB5A3:
            break;
        case Toolbox::Texture::EncodingFormat::RGBA32:
            break;
        case Toolbox::Texture::EncodingFormat::C4:
            break;
        case Toolbox::Texture::EncodingFormat::C8:
            break;
        case Toolbox::Texture::EncodingFormat::C14X2:
            break;
        case Toolbox::Texture::EncodingFormat::CMPR:
            break;
        default:
            break;
        }

        return result;
    }

}  // namespace Toolbox::Texture