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

#include "strutil.hpp"

namespace Toolbox {

    static std::vector<std::string> s_image_formats = {
        "image/bmp",
        "image/jpeg",
        "image/png",
        "image/tiff",
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
        TRY(Deserializer::BytesToObject(data_buf, rgba_color))
            .error([&rgba_color](const BaseError &error) {
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

    [[nodiscard]] std::optional<ImageData> MimeData::get_image() const {
        TOOLBOX_DEBUG_LOG("Image mimedata unsupported");
        return {};
    }
    void MimeData::set_image(const ImageData &data) {
        TOOLBOX_DEBUG_LOG("Image mimedata unsupported");
    }

    [[nodiscard]] std::optional<std::string> MimeData::get_text() const {
        if (!has_text()) {
            return std::nullopt;
        }
        const Buffer &data_buf = m_data_map["text/plain"];
        return std::string(data_buf.buf<char>(), data_buf.size() - 1);
    }

    void MimeData::set_text(std::string_view data) {
        Buffer _tmp;
        _tmp.alloc(data.size() + 1);
        std::memcpy(_tmp.buf(), data.data(), data.size());
        _tmp.get<char>(data.size()) = '\0';
        set_data("text/plain", std::move(_tmp));
    }

    [[nodiscard]] std::optional<std::vector<std::string>> MimeData::get_urls() const{
        if (!has_urls()) {
            return std::nullopt;
        }
        const Buffer &data_buf = m_data_map["text/uri-list"];
        std::string data_string(data_buf.buf<char>(), data_buf.size() - 1);
        std::vector<std::string> result;
        for (auto line : splitLines(data_string)) {
            result.push_back(std::string(line));
        }
        return result;
    }

    void MimeData::set_urls(const std::vector<std::string> &data){
        std::string url_data = joinStrings(data, "\r\n");
        Buffer _tmp;
        _tmp.alloc(url_data.size() + 1);
        std::memcpy(_tmp.buf(), url_data.data(), url_data.size());
        _tmp.get<char>(url_data.size()) = '\0';
        set_data("text/uri-list", std::move(_tmp));
    }

    void MimeData::clear() { m_data_map.clear(); }

}  // namespace Toolbox