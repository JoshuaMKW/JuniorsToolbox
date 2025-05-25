#pragma once

#include <cmath>
#include <expected>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>
#include <unicode/ustring.h>

#include "core/error.hpp"

#ifdef TOOLBOX_USE_ICONV
#define GAME_ENCODING  "SHIFT_JIS"
#define IMGUI_ENCODING "UTF-8"
#else
#define GAME_ENCODING  "Shift_JIS"
#define IMGUI_ENCODING "UTF-8"
#endif

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

#ifdef TOOLBOX_USE_ICONV

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

#else

    static inline Result<uint32_t, EncodingError> getCharacterCount(UConverter *converter,
                                                                    std::string_view value) {
        UErrorCode status = U_ZERO_ERROR;

        int32_t utf16_length =
            ucnv_toUChars(converter, nullptr, 0, value.data(), value.size(), &status);
        if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
            return make_encoding_error<uint32_t>(
                "ICU", "Failed to determine the character count for the given string", "", "");
        }

        status = U_ZERO_ERROR;

        std::vector<UChar> utf16_buffer(utf16_length);
        ucnv_toUChars(converter, utf16_buffer.data(), utf16_buffer.size(), value.data(), value.size(),
                      &status);
        if (U_FAILURE(status)) {
            return make_encoding_error<uint32_t>("ICU", "Failed to convert string to UTF-16", "",
                                                 "");
        }

        int32_t count = u_countChar32(utf16_buffer.data(), utf16_buffer.size());
        if (U_FAILURE(status)) {
            return make_encoding_error<uint32_t>(
                "ICU", "Failed to determine the character count for the given string", "", "");
        }
        return count;
    }

    inline Result<std::string, EncodingError>
    asEncoding(std::string_view value, std::string_view from, std::string_view to) {
        UErrorCode status = U_ZERO_ERROR;

        u_init(&status);

        UConverter *converter = ucnv_open(from.data(), &status);
        if (U_FAILURE(status)) {
            ucnv_close(converter);
            return make_encoding_error<std::string>(
                "ICU", std::format("Failed to open converter to {} from {}", to, from), from, to);
        }

        status = U_ZERO_ERROR;

        UConverter *target_converter = ucnv_open(to.data(), &status);
        if (U_FAILURE(status)) {
            ucnv_close(converter);
            ucnv_close(target_converter);
            return make_encoding_error<std::string>(
                "ICU", std::format("Failed to open converter to {} from {}", to, from), from, to);
        }

        status = U_ZERO_ERROR;

        Result<std::string, EncodingError> result = getCharacterCount(converter, value)
            .and_then([&](size_t string_size) {
                // With the character count result, we can more effectively
                // convert the string to the new encoding.

                icu::UnicodeString unicode_str = icu::UnicodeString::fromUTF8(value);
                icu::UnicodeString converted_str;

                // Allocate enough space for the converted string
                char16_t *buffer = unicode_str.getBuffer(string_size);
                if (buffer == nullptr) {
                    ucnv_close(converter);
                    ucnv_close(target_converter);
                    return make_encoding_error<std::string>(
                        "ICU", "Failed to get buffer for Unicode string", from, to);
                }

                // Set up arguments
                char *target     = reinterpret_cast<char *>(buffer);
                char *target_start = target;
                const char *target_end = target_start + string_size;

                const char *source_start     = reinterpret_cast<const char *>(value.data());
                const char *source_end = source_start + value.size();

                //// Create the pivot buffer
                //std::vector<UChar> pivot_buffer(string_size);
                //UChar *pivot_start  = pivot_buffer.data();
                //UChar *pivot_target = pivot_start;
                //UChar *pivot_end    = pivot_start + string_size;

                ucnv_convertEx(target_converter, converter, &target_start, target_end,
                               &source_start, source_end, NULL, NULL, NULL, NULL, true, true,
                               &status);

                ucnv_close(converter);
                ucnv_close(target_converter);

                if (U_FAILURE(status)) {
                    return make_encoding_error<std::string>(
                        "ICU", "Failed to convert string encoding", from, to);
                }

                return Result<std::string, EncodingError>(std::string(target, string_size));
            })
            .or_else(
                [](const EncodingError &error) -> Result<std::string, EncodingError> {
                return std::unexpected<EncodingError>(error);
              });

        return result;
    }

#endif

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
