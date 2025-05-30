#pragma once

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <unordered_map>

#include "color.hpp"
#include "core/memory.hpp"

#include "image/imagedata.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)

#else
#error "Unsupported OS"
#endif

namespace Toolbox {

    class MimeData {
    public:
        MimeData()                 = default;
        MimeData(const MimeData &) = default;
        MimeData(MimeData &&)      = default;

        [[nodiscard]] bool has_format(std::string_view type) const {
            return m_data_map.contains(std::string(type));
        }
        std::vector<std::string> get_all_formats() const {
            std::vector<std::string> formats;
            formats.reserve(m_data_map.size());
            for (auto &[k, v] : m_data_map) {
                formats.push_back(k);
            }
            return formats;
        }

        [[nodiscard]] bool has_color() const;
        [[nodiscard]] bool has_html() const;
        [[nodiscard]] bool has_image() const;
        [[nodiscard]] bool has_text() const;
        [[nodiscard]] bool has_urls() const;

        [[nodiscard]] std::optional<Buffer> get_data(std::string_view format) const;
        void set_data(std::string_view format, Buffer &&data);

        [[nodiscard]] std::optional<Color::RGBAShader> get_color() const;
        void set_color(const Color::RGBAShader &data);

        [[nodiscard]] std::optional<std::string> get_html() const;
        void set_html(std::string_view data);

        [[nodiscard]] std::optional<ImageData> get_image() const;
        void set_image(const ImageData &data);

        [[nodiscard]] std::optional<std::string> get_text() const;
        void set_text(std::string_view data);

        [[nodiscard]] std::optional<std::vector<std::string>> get_urls() const;
        void set_urls(const std::vector<std::string> &data);

        void clear();

        MimeData &operator=(const MimeData &other) {
            m_data_map = other.m_data_map;
            return *this;
        }

        MimeData &operator=(MimeData &&other) {
            m_data_map = std::move(other.m_data_map);
            return *this;
        }

        static bool isMimeTarget(const std::string &target) {
            return target.starts_with("image/") || target.starts_with("application/") ||
                   target.starts_with("text/");
        }

    private:
        mutable std::unordered_map<std::string, Buffer> m_data_map;
    };

}  // namespace Toolbox