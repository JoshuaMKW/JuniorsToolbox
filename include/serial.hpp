#pragma once

#include "types.hpp"
#include <bit>
#include <iostream>
#include <span>
#include "types.hpp"

class Serializer {
public:
    Serializer(std::streambuf *out) : m_out(out) {}

    template <typename T, std::endian E = std::endian::native> int write(const T &t) {
        if constexpr (E == std::endian::native) {
            return writeBytes(std::span(reinterpret_cast<const char *>(&t), sizeof(T)));
        } else {
            T t2 = std::byteswap(t);
            return writeBytes(std::span(reinterpret_cast<const char *>(&t2), sizeof(T)));
        }
    }

    template <std::endian E = std::endian::native> int writeString(const std::string &str) {
        auto len = write<u16, E>(str.size() & 0xFFFF);
        writeBytes(std::span(str.data(), str.size()));
        return len;
    }

    int writeBytes(std::span<const char> bytes) {
        m_out.write(bytes.data(), bytes.size());
        return bytes.size();
    }

private:
    std::ostream m_out;
};

class Deserializer {
public:
    Deserializer(std::streambuf *in) : m_in(in) {}

    template <typename T, std::endian E = std::endian::native> T read() {
        T t{};
        if constexpr (E == std::endian::native) {
            readBytes(std::span(reinterpret_cast<char *>(&t), sizeof(T)));
            return t;
        } else {
            T t2{};
            readBytes(std::span(reinterpret_cast<char *>(&t2), sizeof(T)));
            t = std::byteswap(t2);
            return t;
        }
    }

    template <typename T, std::endian E = std::endian::native> int read(T &t) {
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

    int readString(std::string &str) {
        auto len = read<u16>();
        str.resize(len);
        readBytes(std::span(str.data(), len));
        return len;
    }

    int readBytes(std::span<char> bytes) {
        m_in.read(bytes.data(), bytes.size());
        return bytes.size();
    }

private:
    std::istream m_in;
};

class ISerializable {
public:
    ISerializable() = default;
    ISerializable(Deserializer &in) { deserialize(in); }

    virtual void serialize(Serializer &out) const = 0;
    virtual void deserialize(Deserializer &in)    = 0;

    void operator<<(Serializer &out) { serialize(out); }
    void operator>>(Deserializer &in) { deserialize(in); }
};