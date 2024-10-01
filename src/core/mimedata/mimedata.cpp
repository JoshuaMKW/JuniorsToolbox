#pragma once

#include <algorithm>

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <unordered_map>

#include "color.hpp"
#include "core/memory.hpp"

#include "gui/logging/errors.hpp"

#include "core/mimedata/mimedata.hpp"

namespace Toolbox {

    static std::vector<std::string> s_image_formats = {
        "image/bmp",
        "image/jpeg",
        "image/png",
        "image/x-portable-bitmap",
        "image/x-portable-graymap",
        "image/x-portable-pixmap",
        "image/x-xbitmap",
        "image/x-xpixmap",
    };

    [[nodiscard]] bool MimeData::has_color() const {
        return m_data_map.contains("application/x-color");
    }
    [[nodiscard]] bool MimeData::has_html() const { return m_data_map.contains("text/html"); }
    [[nodiscard]] bool MimeData::has_image() const {
        return std::any_of(
            s_image_formats.begin(), s_image_formats.end(),
            [this](const std::string &format) { return m_data_map.contains(format); });
    }
    [[nodiscard]] bool MimeData::has_text() const { return m_data_map.contains("text/plain"); }
    [[nodiscard]] bool MimeData::has_urls() const { return m_data_map.contains("text/uri-list"); }

    [[nodiscard]] std::optional<Buffer> MimeData::get_data(std::string_view format) const {
        if (!has_format(format)) {
            return std::nullopt;
        }
        return m_data_map[std::string(format)];
    }

    void MimeData::set_data(std::string_view format, Buffer &&data) {
        m_data_map[std::string(format)] = std::move(data);
    }

    [[nodiscard]] std::optional<Color::RGBAShader> MimeData::get_color() const {
        if (!has_color()) {
            return std::nullopt;
        }
        Buffer &data_buf = m_data_map["application/x-color"];

        Color::RGBAShader rgba_color;
        TRY(Deserializer::BytesToObject(data_buf, rgba_color)).error([&rgba_color](const BaseError &error) {
            UI::LogError(error);
            rgba_color.setColor(0.0f, 0.0f, 0.0f);
        });
        return rgba_color;
    }

    void MimeData::set_color(const Color::RGBAShader &data) {
        Buffer _tmp;
        TRY(Serializer::ObjectToBytes(data, _tmp))
            .then([this, &_tmp]() { set_data("application/x-color", std::move(_tmp)); })
            .error([](const SerialError &error) { UI::LogError(error); });
    }

    [[nodiscard]] std::optional<std::string> MimeData::get_html() const {
        if (!has_html()) {
            return std::nullopt;
        }
        const Buffer &data_buf = m_data_map["text/html"];
        return std::string(data_buf.buf<char>(), data_buf.size());
    }

    void MimeData::set_html(std::string_view data) {
        Buffer _tmp;
        _tmp.alloc(data.size());
        std::memcpy(_tmp.buf(), data.data(), data.size());
        set_data("text/html", std::move(_tmp));
    }

    [[nodiscard]] std::optional<Buffer> MimeData::get_image() const {
        TOOLBOX_DEBUG_LOG("Image mimedata unsupported");
        return {};
    }
    void MimeData::set_image(const Buffer &data) {
        TOOLBOX_DEBUG_LOG("Image mimedata unsupported");
    }

    [[nodiscard]] std::optional<std::string> MimeData::get_text() const {
        if (!has_text()) {
            return std::nullopt;
        }
        const Buffer &data_buf = m_data_map["text/plain"];
        return std::string(data_buf.buf<char>(), data_buf.size());
    }

    void MimeData::set_text(std::string_view data) {
        Buffer _tmp;
        _tmp.alloc(data.size());
        std::memcpy(_tmp.buf(), data.data(), data.size());
        set_data("text/plain", std::move(_tmp));
    }

    [[nodiscard]] std::optional<std::string> MimeData::get_urls() const {
        if (!has_urls()) {
            return std::nullopt;
        }
        const Buffer &data_buf = m_data_map["text/uri-list"];
        return std::string(data_buf.buf<char>(), data_buf.size());
    }

    void MimeData::set_urls(std::string_view data) {
        Buffer _tmp;
        _tmp.alloc(data.size());
        std::memcpy(_tmp.buf(), data.data(), data.size());
        set_data("text/uri-list", std::move(_tmp));
    }

    void MimeData::clear() { m_data_map.clear(); }

#ifdef TOOLBOX_PLATFORM_WINDOWS

    /* TODO: Change NULL to HTML */
    static std::unordered_map<u32, std::string> s_format_to_mime = {
        {CF_UNICODETEXT, "text/plain"            },
        {CF_TEXT,        "text/plain"            },
        {CF_NULL,        "text/html"             },
        {CF_HDROP,       "text/uri-list"         },
        {CF_DIB,         "application/x-qt-image"}
    };

    static std::unordered_map<std::string, u32> s_mime_to_format = {
        {"text/plain",             CF_TEXT },
        {"text/html",              CF_NULL },
        {"text/uri-list",          CF_HDROP},
        {"application/x-qt-image", CF_DIB  },
    };

    FORMATETC MimeData::FormatForMime(std::string_view mimetype) { return FORMATETC(); }

    std::string MimeData::MimeForFormat(FORMATETC format) { return std::string(); }
#elif defined(TOOLBOX_PLATFORM_LINUX)
    static std::unordered_map<std::string, std::string> s_uti_to_mime = {
        {"public.utf8-plain-text",               "text/plain"            },
        {"public.utf16-plain-text",              "text/plain"            },
        {"public.text",                          "text/plain"            },
        {"public.html",                          "text/html"             },
        {"public.url",                           "text/uri-list"         },
        {"public.file-url",                      "text/uri-list"         },
        {"public.tiff",                          "application/x-qt-image"},
        {"public.vcard",                         "text/plain"            },
        {"com.apple.traditional-mac-plain-text", "text/plain"            },
        {"com.apple.pict",                       "application/x-qt-image"},
    };

    static std::unordered_map<std::string, std::string> s_mime_to_uti = {
        {"text/plain",             "public.text"},
        {"text/html",              "public.html"},
        {"text/uri-list",          "public.url" },
        {"application/x-qt-image", "public.tiff"},
    };

    std::string MimeData::UTIForMime(std::string_view mimetype) { return s_mime_to_uti[std::string(mimetype)]; }
    std::string MimeData::MimeForUTI(std::string_view uti) { return s_uti_to_mime[std::string(uti)]; }
#endif

}  // namespace Toolbox