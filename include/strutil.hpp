#pragma once

#include <cmath>
#include <expected>
#include <format>
#include <iconv.h>
#include <string>
#include <string_view>
#include <iterator>

#include "core/error.hpp"

#define GAME_ENCODING  "SHIFT_JIS"
#define IMGUI_ENCODING "UTF-8"

namespace Toolbox::String {

    struct EncodingError : public BaseError {
        std::string m_encoding_from;
        std::string m_encoding_to;
    };

    template <typename T>
    inline Result<T, EncodingError>
    make_encoding_error(std::string_view context, std::string_view reason,
                        std::string_view from_encoding, std::string_view to_encoding) {
        EncodingError err = {std::vector({std::format("{}: {}.", context, reason)}),
                             std::stacktrace::current(), std::string(from_encoding),
                             std::string(to_encoding)};
        return std::unexpected<EncodingError>(err);
    }

    inline Result<std::string, EncodingError>
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

    inline Result<std::string, EncodingError> fromGameEncoding(std::string_view value) {
        return asEncoding(value, GAME_ENCODING, IMGUI_ENCODING);
    }

    inline Result<std::string, EncodingError> toGameEncoding(std::string_view value) {
        return asEncoding(value, IMGUI_ENCODING, GAME_ENCODING);
    }
    inline std::vector<std::string_view> splitLines(std::string_view s) {
        std::vector<std::string_view> result;
        size_t last_pos         = 0;
        size_t next_newline_pos = s.find('\n', 0);
        while (next_newline_pos != std::string::npos) {
            if (s[next_newline_pos - 1] == '\r') {
                result.push_back(s.substr(last_pos, next_newline_pos - last_pos - 1));
            } else {
                result.push_back(s.substr(last_pos, next_newline_pos - last_pos));
            }
            last_pos         = next_newline_pos + 1;
            next_newline_pos = s.find('\n', last_pos);
        }
        if (last_pos < s.size()) {
            if (s[s.size() - 1] == '\r') {
                result.push_back(s.substr(last_pos, s.size() - last_pos - 1));
            } else {
                result.push_back(s.substr(last_pos));
            }
        }
        return result;
    }
    inline std::string joinStrings(std::vector<std::string> strings, const char *const delimiter) {
        std::ostringstream os;
        auto b = strings.begin(), e = strings.end();

        if (b != e) {
            std::copy(b, prev(e), std::ostream_iterator<std::string>(os, delimiter));
            b = prev(e);
        }
        if (b != e) {
            os << *b;
        }

        return os.str();
    }
}  // namespace Toolbox::String
