#pragma once

#include "serial.hpp"
#include "types.hpp"
#include <expected>

namespace Toolbox::Color {

    class BaseColor : public ISerializable {
    public:
        constexpr void setColor(f32 r, f32 g, f32 b, f32 a) { transformInputAndSet(r, g, b, a); }
        constexpr void setColor(f32 r, f32 g, f32 b) { setColor(r, g, b, 1.0f); }
        constexpr void getColor(f32 &r, f32 &g, f32 &b, f32 &a) const {
            transformOutputAndGet(r, g, b, a);
        }

        constexpr bool operator==(const BaseColor& other) const {
            f32 r, g, b, a;
            other.getColor(r, g, b, a);
            f32 r2, g2, b2, a2;
            getColor(r2, g2, b2, a2);
            return r == r2 && g == g2 && b == b2 && a == a2;
        }

    protected:
        constexpr virtual void transformInputAndSet(f32 r, f32 g, f32 b, f32 a)            = 0;
        constexpr virtual void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const = 0;
    };

    class RGBAShader final : public BaseColor {
    public:
        f32 m_r, m_g, m_b, m_a;

        constexpr RGBAShader() = default;
        constexpr RGBAShader(f32 r, f32 g, f32 b, f32 a) { setColor(r, g, b, a); }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.write(m_r);
            out.write(m_g);
            out.write(m_b);
            out.write(m_a);
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            m_r = in.read<f32>();
            m_g = in.read<f32>();
            m_b = in.read<f32>();
            m_a = in.read<f32>();
            return {};
        }

    protected:
        constexpr void transformInputAndSet(f32 r, f32 g, f32 b, f32 a) override {
            m_r = r;
            m_g = g;
            m_b = b;
            m_a = a;
        }
        constexpr void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const override {
            r = m_r;
            g = m_g;
            b = m_b;
            a = m_a;
        }
    };

    class RGBShader final : public BaseColor {
    public:
        f32 m_r, m_g, m_b;

        constexpr RGBShader() = default;
        constexpr RGBShader(f32 r, f32 g, f32 b) { setColor(r, g, b, 1.0f); }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.write(m_r);
            out.write(m_g);
            out.write(m_b);
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            m_r = in.read<f32>();
            m_g = in.read<f32>();
            m_b = in.read<f32>();
            return {};
        }

    protected:
        constexpr void transformInputAndSet(f32 r, f32 g, f32 b, f32 a) override {
            m_r = r;
            m_g = g;
            m_b = b;
        }
        constexpr void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const override {
            r = m_r;
            g = m_g;
            b = m_b;
            a = 1.0f;
        }
    };

    class RGBA32 final : public BaseColor {
    public:
        u8 m_r, m_g, m_b, m_a;

        constexpr RGBA32() = default;
        constexpr RGBA32(u8 r, u8 g, u8 b, u8 a) : m_r(r), m_g(g), m_b(b), m_a(a) {}
        constexpr RGBA32(f32 r, f32 g, f32 b, f32 a) { setColor(r, g, b, a); }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.write<u8, std::endian::big>(m_r);
            out.write<u8, std::endian::big>(m_g);
            out.write<u8, std::endian::big>(m_b);
            out.write<u8, std::endian::big>(m_a);
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            m_r = in.read<u8, std::endian::big>();
            m_g = in.read<u8, std::endian::big>();
            m_b = in.read<u8, std::endian::big>();
            m_a = in.read<u8, std::endian::big>();
            return {};
        }

    protected:
        constexpr void transformInputAndSet(f32 r, f32 g, f32 b, f32 a) override {
            m_r = static_cast<u8>(r * 255);
            m_g = static_cast<u8>(g * 255);
            m_b = static_cast<u8>(b * 255);
            m_a = static_cast<u8>(a * 255);
        }
        constexpr void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const override {
            r = static_cast<f32>(m_r) / 255;
            g = static_cast<f32>(m_g) / 255;
            b = static_cast<f32>(m_b) / 255;
            a = static_cast<f32>(m_a) / 255;
        }
    };

    class RGB5A3 final : public BaseColor {
    public:
        struct ColorData {
            u8 m_r : 5;
            u8 m_g : 5;
            u8 m_b : 5;
            u8 m_a : 3;
        } m_color;

        constexpr RGB5A3() = default;
        constexpr RGB5A3(f32 r, f32 g, f32 b, f32 a) { setColor(r, g, b, a); }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.writeBytes({reinterpret_cast<const char *>(&m_color), sizeof(m_color)});
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            in.readBytes({reinterpret_cast<char *>(&m_color), sizeof(m_color)});
            return {};
        }

    protected:
        constexpr void transformInputAndSet(f32 r, f32 g, f32 b, f32 a) override {
            m_color.m_r = static_cast<u8>(r * 255) >> 3;
            m_color.m_g = static_cast<u8>(g * 255) >> 3;
            m_color.m_b = static_cast<u8>(b * 255) >> 3;
            m_color.m_a = static_cast<u8>(a * 255) >> 5;
        }
        constexpr void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const override {
            r = static_cast<f32>(m_color.m_r << 3) / 255;
            g = static_cast<f32>(m_color.m_g << 3) / 255;
            b = static_cast<f32>(m_color.m_b << 3) / 255;
            a = static_cast<f32>(m_color.m_a << 5) / 255;
        }
    };

    class RGB24 final : public BaseColor {
    public:
        u8 m_r = 0, m_g = 0, m_b = 0;

        constexpr RGB24() = default;
        constexpr RGB24(u8 r, u8 g, u8 b) : m_r(r), m_g(g), m_b(b) {}
        constexpr RGB24(f32 r, f32 g, f32 b) { setColor(r, g, b, 1.0f); }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.write<u8, std::endian::big>(m_r);
            out.write<u8, std::endian::big>(m_g);
            out.write<u8, std::endian::big>(m_b);
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            m_r = in.read<u8, std::endian::big>();
            m_g = in.read<u8, std::endian::big>();
            m_b = in.read<u8, std::endian::big>();
            return {};
        }

    protected:
        constexpr void transformInputAndSet(f32 r, f32 g, f32 b, f32 a) override {
            m_r = static_cast<u8>(r * 255);
            m_g = static_cast<u8>(g * 255);
            m_b = static_cast<u8>(b * 255);
        }
        constexpr void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const override {
            r = static_cast<f32>(m_r) / 255;
            g = static_cast<f32>(m_g) / 255;
            b = static_cast<f32>(m_b) / 255;
            a = 1.0f;
        }
    };

    class RGB565 final : public BaseColor {
    public:
        struct ColorData {
            u8 m_r : 5;
            u8 m_g : 6;
            u8 m_b : 5;
        } m_color;

        constexpr RGB565() = default;
        constexpr RGB565(f32 r, f32 g, f32 b, f32 a) { setColor(r, g, b, a); }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.writeBytes({reinterpret_cast<const char *>(&m_color), sizeof(m_color)});
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            in.readBytes({reinterpret_cast<char *>(&m_color), sizeof(m_color)});
            return {};
        }

    protected:
        constexpr void transformInputAndSet(f32 r, f32 g, f32 b, f32 a) override {
            m_color.m_r = static_cast<u8>(r * 255) >> 3;
            m_color.m_g = static_cast<u8>(g * 255) >> 2;
            m_color.m_b = static_cast<u8>(b * 255) >> 3;
        }
        constexpr void transformOutputAndGet(f32 &r, f32 &g, f32 &b, f32 &a) const override {
            r = static_cast<f32>(m_color.m_r << 3) / 255;
            g = static_cast<f32>(m_color.m_g << 2) / 255;
            b = static_cast<f32>(m_color.m_b << 3) / 255;
        }
    };

}  // namespace Toolbox::Color

template <> struct std::formatter<Toolbox::Color::RGBA32> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Color::RGBA32 &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(r: {}, g: {}, b: {}, a: {})", obj.m_r, obj.m_g,
                       obj.m_b, obj.m_a);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

template <> struct std::formatter<Toolbox::Color::RGB5A3> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Color::RGB5A3 &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "{r: {}, g: {}, b: {}, a: {}}", obj.m_color.m_r,
                       obj.m_color.m_g, obj.m_color.m_b, obj.m_color.m_a);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

template <> struct std::formatter<Toolbox::Color::RGB24> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Color::RGB24 &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(r: {}, g: {}, b: {})", obj.m_r,
                       obj.m_g, obj.m_b);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

template <> struct std::formatter<Toolbox::Color::RGB565> : std::formatter<string_view> {
    template <typename FormatContext>
    auto format(const Toolbox::Color::RGB565 &obj, FormatContext &ctx) {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(r: {}, g: {}, b: {})", obj.m_color.m_r,
                       obj.m_color.m_g, obj.m_color.m_b);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};