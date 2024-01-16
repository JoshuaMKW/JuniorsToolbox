#pragma once

#include <cmath>
#include <expected>
#include <format>
#include <iconv.h>
#include <string>
#include <string_view>

#include "error.hpp"

#define GAME_ENCODING  "SHIFT_JIS"
#define IMGUI_ENCODING "UTF-8"

namespace Toolbox::String {

    struct EncodingError : public BaseError {
        std::string m_encoding_from;
        std::string m_encoding_to;
    };

    template <typename T>
    inline std::expected<T, EncodingError>
    make_encoding_error(std::string_view context, std::string_view reason,
                        std::string_view from_encoding, std::string_view to_encoding) {
        EncodingError err = {std::vector({std::format("{}: {}.", context, reason)}),
                             std::stacktrace::current(), std::string(from_encoding),
                             std::string(to_encoding)};
        return std::unexpected<EncodingError>(err);
    }

    inline std::expected<std::string, EncodingError>
    asEncoding(std::string_view value, std::string_view from, std::string_view to) {
        iconv_t conv = iconv_open(to.data(), from.data());
        if (conv == (iconv_t)(-1)) {
            return make_encoding_error<std::string>("ICONV", "Failed to open converter", from, to);
        }

        size_t inbytesleft = value.size();
        char *inbuf        = const_cast<char *>(value.data());

        size_t outbytesleft = value.size() * 2;
        std::string sjis(outbytesleft, '\0');
        char *outbuf = &sjis[0];

        if (iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)(-1)) {
            iconv_close(conv);
            return make_encoding_error<std::string>(
                "ICONV", std::format("Failed to convert string encoding from {} to {}", from, to),
                from, to);
        }

        sjis.resize(std::max(sjis.size() - outbytesleft, size_t(0)));
        iconv_close(conv);
        return sjis;
    }

    inline std::expected<std::string, EncodingError> fromGameEncoding(std::string_view value) {
        return asEncoding(value, GAME_ENCODING, IMGUI_ENCODING);
    }

    inline std::expected<std::string, EncodingError> toGameEncoding(std::string_view value) {
        return asEncoding(value, IMGUI_ENCODING, GAME_ENCODING);
    }

}  // namespace Toolbox::String