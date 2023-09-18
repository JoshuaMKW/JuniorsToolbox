#pragma once

#include "types.hpp"
#include <bit>
#include <expected>
#include <iostream>
#include <span>
#include <stacktrace>
#include <vector>

namespace Toolbox {

    struct SerialError {
        std::vector<std::string> m_message;
        std::size_t m_error_pos;
        std::string m_file_path;
        std::stacktrace m_stack_trace;
    };

    class Serializer {
    public:
        Serializer(std::streambuf *out) : m_out(out) {}
        Serializer(std::streambuf *out, std::string_view file_path)
            : m_out(out), m_file_path(file_path) {}
        Serializer(const Serializer &) = default;
        Serializer(Serializer &&)      = default;

        std::ostream &stream() { return m_out; }
        std::string_view filepath() const { return m_file_path; }

        template <typename T, std::endian E = std::endian::native> size_t write(const T &t) {
            if constexpr (E == std::endian::native) {
                return writeBytes(std::span(reinterpret_cast<const char *>(&t), sizeof(T)));
            } else {
                if constexpr (std::is_same_v<T, f32>) {
                    u32 t2 = std::byteswap(std::bit_cast<u32>(t));
                    return write<u32, E>(t2);
                } else if constexpr (std::is_same_v<T, f64>) {
                    u64 t2 = std::byteswap(std::bit_cast<u64>(t));
                    return write<u64, E>(t2);
                } else {
                    T t2 = std::byteswap(t);
                    return writeBytes(std::span(reinterpret_cast<const char *>(&t2), sizeof(T)));
                }
            }
        }

        template <std::endian E = std::endian::native> size_t writeString(const std::string &str) {
            auto len = write<u16, E>(str.size() & 0xFFFF);
            writeBytes(std::span(str.data(), str.size()));
            return len;
        }

        size_t writeBytes(std::span<const char> bytes) {
            m_out.write(bytes.data(), bytes.size());
            return bytes.size();
        }

    private:
        std::ostream m_out;
        std::string m_file_path = "[unknown path]";
    };

    class Deserializer {
    public:
        Deserializer(std::streambuf *in) : m_in(in) {}
        Deserializer(std::streambuf *in, std::string_view file_path)
            : m_in(in), m_file_path(file_path) {}
        Deserializer(const Deserializer &) = default;
        Deserializer(Deserializer &&)      = default;

        std::istream &stream() { return m_in; }
        std::string_view filepath() const { return m_file_path; }

        template <typename T, std::endian E = std::endian::native> T read() {
            T t{};
            if constexpr (E == std::endian::native) {
                readBytes(std::span(reinterpret_cast<char *>(&t), sizeof(T)));
                return t;
            } else {
                if constexpr (std::is_same_v<T, f32>) {
                    u32 t2{};
                    readBytes(std::span(reinterpret_cast<char *>(&t2), sizeof(u32)));
                    t = std::bit_cast<f32>(std::byteswap(t2));
                    return t;
                } else if constexpr (std::is_same_v<T, f64>) {
                    u64 t2{};
                    readBytes(std::span(reinterpret_cast<char *>(&t2), sizeof(u64)));
                    t = std::bit_cast<f64>(std::byteswap(t2));
                    return t;
                } else {
                    T t2{};
                    readBytes(std::span(reinterpret_cast<char *>(&t2), sizeof(T)));
                    t = std::byteswap(t2);
                    return t;
                }
            }
        }

        template <typename T, std::endian E = std::endian::native> size_t read(T &t) {
            if constexpr (E == std::endian::native) {
                return readBytes(std::span(reinterpret_cast<char *>(&t), sizeof(T)));
            } else {
                T t2{};
                int ret = readBytes(std::span(reinterpret_cast<char *>(&t2), sizeof(T)));
                t       = std::byteswap(t2);
                return ret;
            }
        }

        template <std::endian E = std::endian::native> std::string readString() {
            std::string str;
            auto len = read<u16, E>();
            str.resize(len);
            readBytes(std::span(str.data(), len));
            return str;
        }

        size_t readString(std::string &str) {
            auto len = read<u16>();
            str.resize(len);
            readBytes(std::span(str.data(), len));
            return len;
        }

        size_t readBytes(std::span<char> bytes) {
            m_in.read(bytes.data(), bytes.size());
            return bytes.size();
        }

    private:
        std::istream m_in;
        std::string m_file_path = "[unknown path]";
    };

    class ISerializable {
    public:
        ISerializable() = default;
        ISerializable(Deserializer &in) { deserialize(in); }

        virtual std::expected<void, SerialError> serialize(Serializer &out) const = 0;
        virtual std::expected<void, SerialError> deserialize(Deserializer &in)    = 0;

        void operator<<(Serializer &out) { serialize(out); }
        void operator>>(Deserializer &in) { deserialize(in); }
    };

    inline SerialError make_serial_error(std::string_view context, std::string_view reason,
                                         size_t error_pos, std::string_view filepath) {
        return SerialError{
            {std::format("SerialError: {}", context), std::format("Reason: {}", reason)},
            error_pos,
            std::string(filepath),
            std::stacktrace::current()
        };
    }

    inline SerialError make_serial_error(Serializer &s, std::string_view reason, int error_adjust) {
        return make_serial_error("Unexpected byte at position {} ({:X}).", reason,
                                 std::size_t(s.stream().tellp()) + error_adjust, s.filepath());
    }

    inline SerialError make_serial_error(Serializer &s, std::string_view reason) {
        return make_serial_error(s, reason, 0);
    }

    inline SerialError make_serial_error(Deserializer &s, std::string_view reason,
                                         int error_adjust) {
        return make_serial_error("Unexpected byte at position {} ({:X}).", reason,
                                 std::size_t(s.stream().tellg()) + error_adjust, s.filepath());
    }

    inline SerialError make_serial_error(Deserializer &s, std::string_view reason) {
        return make_serial_error(s, reason, 0);
    }

}  // namespace Toolbox