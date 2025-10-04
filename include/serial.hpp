#pragma once

#include "core/error.hpp"
#include "core/memory.hpp"
#include "core/types.hpp"
#include <bit>
#include <expected>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <stack>
#include <stacktrace>
#include <vector>

namespace Toolbox {

    class ISerializable;

    struct SerialError : public BaseError {
        std::size_t m_error_pos;
        std::string m_file_path;
    };

    template <typename _Ret>
    inline Result<_Ret, SerialError> make_serial_error(std::string_view context,
                                                       std::string_view reason, size_t error_pos,
                                                       std::string_view filepath) {
        SerialError err = {
            std::vector<std::string>(
                {std::format("SerialError: {}", context), std::format("Reason: {}", reason)}),
            std::stacktrace::current(),
            error_pos,
            std::string(filepath),
        };
        return std::unexpected(err);
    }

    class Serializer {
    public:
        Serializer(Buffer &buf, std::streambuf *rdbuf) : m_out(rdbuf) {
            rdbuf->pubsetbuf(buf.buf<char>(), buf.size());
        }
        Serializer(Buffer &buf, std::streambuf *rdbuf, std::string_view file_path)
            : m_out(rdbuf), m_file_path(file_path) {
            rdbuf->pubsetbuf(buf.buf<char>(), buf.size());
        }
        Serializer(std::streambuf *out) : m_out(out) {}
        Serializer(std::streambuf *out, std::string_view file_path)
            : m_out(out), m_file_path(file_path) {}
        Serializer(const Serializer &) = default;
        Serializer(Serializer &&)      = default;

        std::ostream &stream() { return m_out; }
        std::string_view filepath() const { return m_file_path; }

        template <typename _S, std::endian E = std::endian::native>
        static Result<void, SerialError> ObjectToBytes(const _S &_s, Buffer &buf_out,
                                                       size_t offset = 0) {
            if constexpr (std::is_base_of_v<ISerializable, _S>) {
                std::streampos startpos = 0;
                std::streampos endpos;

                Result<void, SerialError> result;

                std::stringstream strstream;
                Serializer sout(strstream.rdbuf());

                // Write padding bytes
                for (size_t i = 0; i < offset; ++i) {
                    sout.write<u8>(0);
                }

                sout.pushBreakpoint();
                {
                    result = _s.serialize(sout);
                    endpos = sout.tell();
                }
                sout.popBreakpoint();

                if (!result) {
                    return std::unexpected(result.error());
                }

                size_t objsize = static_cast<size_t>(endpos - startpos);

                buf_out.alloc(objsize);
                strstream.read(buf_out.buf<char>(), objsize);

                return result;
            } else if constexpr (std::is_standard_layout_v<_S>) {
                buf_out.alloc(offset + sizeof(_S));
                _S *out_obj = reinterpret_cast<_S *>(buf_out.buf<char>() + offset);
                *out_obj    = _s;
                return {};
            } else {
                return make_serial_error<void>(
                    "[BytesToObject]",
                    "Type provided does not implement the serial interface and is not a POD type.",
                    0, "");
            }
        }

        template <typename T, std::endian E = std::endian::native> Serializer &write(const T &t) {
            if constexpr (E == std::endian::native) {
                writeBytes(std::span(reinterpret_cast<const char *>(&t), sizeof(T)));
                return *this;
            } else {
                if constexpr (std::is_same_v<T, f32>) {
                    u32 t2 = std::byteswap(std::bit_cast<u32>(t));
                    writeBytes(std::span(reinterpret_cast<const char *>(&t2), sizeof(u32)));
                    return *this;
                } else if constexpr (std::is_same_v<T, f64>) {
                    u64 t2 = std::byteswap(std::bit_cast<u64>(t));
                    writeBytes(std::span(reinterpret_cast<const char *>(&t2), sizeof(u64)));
                    return *this;
                } else {
                    T t2 = std::byteswap(t);
                    writeBytes(std::span(reinterpret_cast<const char *>(&t2), sizeof(T)));
                    return *this;
                }
            }
        }

        template <std::endian E = std::endian::native>
        Serializer &writeString(const std::string &str) {
            if (str.size() == 0)
                return *this;
            write<u16, E>(str.size() & 0xFFFF);
            writeBytes(std::span(str.data(), str.size()));
            return *this;
        }

        template <std::endian E = std::endian::native>
        Serializer &writeString(std::string_view str) {
            if (str.size() == 0)
                return *this;
            write<u16, E>(str.size() & 0xFFFF);
            writeBytes(std::span(str.data(), str.size()));
            return *this;
        }

        Serializer &writeCString(const std::string &str) {
            writeBytes(std::span(str.data(), str.size()));
            write('\0');
            return *this;
        }

        Serializer &writeCString(std::string_view str) {
            writeBytes(std::span(str.data(), str.size()));
            write('\0');
            return *this;
        }

        Serializer &writeBytes(std::span<const char> bytes) {
            m_out.write(bytes.data(), bytes.size());
            return *this;
        }

        Serializer &padTo(std::size_t alignment, std::span<const char> fill) {
            std::size_t pos = tell();
            std::size_t pad = (alignment - (pos % alignment));
            if (pad == alignment)
                return *this;
            std::size_t fill_size = fill.size();
            for (std::size_t i = 0; i < pad; ++i) {
                write<u8>(fill[i % fill_size]);
            }
            return *this;
        }

        Serializer &padTo(std::size_t alignment, char fill) {
            std::size_t pos = tell();
            std::size_t pad = (alignment - (pos % alignment));
            if (pad == alignment)
                return *this;
            for (std::size_t i = 0; i < pad; ++i) {
                write<u8>(0);
            }
            return *this;
        }

        Serializer &padTo(std::size_t alignment) { return padTo(alignment, '\x00'); }

        Serializer &seek(std::streamoff off, std::ios_base::seekdir way) {
            m_out.seekp(off, way);
            return *this;
        }

        Serializer &seek(std::streampos pos) { return seek(pos, std::ios::cur); }

        std::streampos tell() { return m_out.tellp(); }

        size_t size() {
            auto pos = tell();
            seek(0, std::ios::end);
            auto size = static_cast<size_t>(tell());
            seek(pos, std::ios::beg);
            return size;
        }

        bool eof() const { return m_out.eof(); }
        bool good() const { return m_out.good(); }
        bool fail() const { return m_out.fail(); }
        bool bad() const { return m_out.bad(); }

        void pushBreakpoint();
        Result<void, SerialError> popBreakpoint();

    private:
        std::ostream m_out;
        std::stack<std::streampos> m_breakpoints;
        std::string m_file_path = "[unknown path]";
    };

    class Deserializer {
    public:
        Deserializer(Buffer &buf, std::streambuf *rdbuf) : m_in(rdbuf) {
            rdbuf->pubsetbuf(buf.buf<char>(), buf.size());
        }
        Deserializer(Buffer &buf, std::streambuf *rdbuf, std::string_view file_path)
            : m_in(rdbuf), m_file_path(file_path) {
            rdbuf->pubsetbuf(buf.buf<char>(), buf.size());
        }
        Deserializer(std::streambuf *in) : m_in(in) {}
        Deserializer(std::streambuf *in, std::string_view file_path)
            : m_in(in), m_file_path(file_path) {}
        Deserializer(const Deserializer &) = default;
        Deserializer(Deserializer &&)      = default;

        std::istream &stream() { return m_in; }
        std::string_view filepath() const { return m_file_path; }

        template <typename _S, std::endian E = std::endian::native>
        static Result<void, SerialError> BytesToObject(const Buffer &serial_data, _S &obj,
                                                       size_t offset = 0) {
            if constexpr (std::is_base_of_v<ISerializable, _S>) {
                std::stringstream str_in(
                    std::string(serial_data.buf<char>() + offset, serial_data.size() - offset));

                Deserializer in(str_in.rdbuf());
                return obj.deserialize(in);
            } else if constexpr (std::is_standard_layout_v<_S>) {
                obj = *reinterpret_cast<const _S *>(serial_data.buf<char>() + offset);
                return {};
            } else {
                return make_serial_error<void>(
                    "[BytesToObject]",
                    "Type provided does not implement the serial interface and is not a POD type.",
                    0, "");
            }
        }

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

        template <typename T, std::endian E = std::endian::native> Deserializer &read(T &t) {
            if constexpr (E == std::endian::native) {
                readBytes(std::span(reinterpret_cast<char *>(&t), sizeof(T)));
                return *this;
            } else {
                T t2{};
                readBytes(std::span(reinterpret_cast<char *>(&t2), sizeof(T)));
                t = std::byteswap(t2);
                return *this;
            }
        }

        template <std::endian E = std::endian::native> std::string readString() {
            std::string str;
            auto len = read<u16, E>();
            str.resize(len);
            readBytes(std::span(str.data(), len));
            return str;
        }

        template <std::endian E = std::endian::native> Deserializer &readString(std::string &str) {
            auto len = read<u16, E>();
            str.resize(len);
            readBytes(std::span(str.data(), len));
            return *this;
        }

        std::string readCString(size_t limit = std::string::npos) {
            std::string str;
            if (limit != std::string::npos) {
                str.reserve(limit);
            }
            while (true) {
                char c = read<char>();
                if (c == '\0') {
                    break;
                }
                str.push_back(c);
                if (limit != std::string::npos && str.size() >= limit) {
                    break;
                }
            }
            return str;
        }

        Deserializer &readCString(std::string &str, size_t limit = std::string::npos) {
            if (limit != std::string::npos) {
                str.reserve(limit);
            }
            while (true) {
                char c = read<char>();
                if (c == '\0') {
                    break;
                }
                str.push_back(c);
                if (limit != std::string::npos && str.size() >= limit) {
                    break;
                }
            }
            return *this;
        }

        Deserializer &readBytes(std::span<char> bytes) {
            m_in.read(bytes.data(), bytes.size());
            return *this;
        }

        Deserializer &alignTo(size_t alignment) {
            return seek((static_cast<size_t>(tell()) + alignment - 1) & ~(alignment - 1),
                        std::ios::beg);
        }

        Deserializer &seek(std::streamoff off, std::ios_base::seekdir way) {
            m_in.seekg(off, way);
            return *this;
        }

        Deserializer &seek(std::streampos pos) { return seek(pos, std::ios::cur); }

        std::streampos tell() { return m_in.tellg(); }

        size_t size() {
            auto pos = tell();
            seek(0, std::ios::end);
            auto size = static_cast<size_t>(tell());
            seek(pos, std::ios::beg);
            return size;
        }

        size_t remaining() {
            auto pos = tell();
            seek(0, std::ios::end);
            auto size = static_cast<size_t>(tell());
            seek(pos, std::ios::beg);
            return size - pos;
        }

        bool eof() const { return m_in.eof(); }
        bool good() const { return m_in.good(); }
        bool fail() const { return m_in.fail(); }
        bool bad() const { return m_in.bad(); }

        void pushBreakpoint();
        Result<void, SerialError> popBreakpoint();

    private:
        std::istream m_in;
        std::stack<std::streampos> m_breakpoints;
        std::string m_file_path = "[unknown path]";
    };

    class ISerializable {
    public:
        ISerializable() = default;
        ISerializable(Deserializer &in) { deserialize(in); }
        ISerializable(Serializer &out) { serialize(out); }
        virtual ~ISerializable() = default;

        virtual Result<void, SerialError> serialize(Serializer &out) const = 0;
        virtual Result<void, SerialError> deserialize(Deserializer &in)    = 0;

        void operator<<(Serializer &out) { serialize(out); }
        void operator>>(Deserializer &in) { deserialize(in); }
    };

    template <typename _Ret>
    inline Result<_Ret, SerialError> make_serial_error(Serializer &s, std::string_view reason,
                                                       int error_adjust) {
        auto pos = std::max(static_cast<size_t>(s.tell()) + error_adjust, static_cast<size_t>(0));
        return make_serial_error<_Ret>(
            std::format("Unexpected byte at position {} ({:X}).", pos, pos), reason,
            pos + error_adjust, s.filepath());
    }

    template <typename _Ret>
    inline Result<_Ret, SerialError> make_serial_error(Serializer &s, std::string_view reason) {
        return make_serial_error<_Ret>(s, reason, 0);
    }

    template <typename _Ret>
    inline Result<_Ret, SerialError> make_serial_error(Deserializer &s, std::string_view reason,
                                                       int error_adjust) {
        auto pos = std::max(static_cast<size_t>(s.tell()) + error_adjust, static_cast<size_t>(0));
        return make_serial_error<_Ret>(
            std::format("Unexpected byte at position {} ({:X}).", pos, pos), reason,
            pos + error_adjust, s.filepath());
    }

    template <typename _Ret>
    inline Result<_Ret, SerialError> make_serial_error(Deserializer &s, std::string_view reason) {
        return make_serial_error<_Ret>(s, reason, 0);
    }

}  // namespace Toolbox