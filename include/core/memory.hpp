#pragma once

#include <memory>

#include "core/assert.hpp"
#include "core/types.hpp"

namespace Toolbox {

    template <typename T> using RefPtr = std::shared_ptr<T>;
    template <typename T, typename... Args> RefPtr<T> make_referable(Args &&...args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T> using ScopePtr = std::unique_ptr<T>;
    template <typename T, typename... Args> ScopePtr<T> make_scoped(Args &&...args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    class Buffer {
    public:
        using byte_t = u8;

        Buffer() = default;
        Buffer(const Buffer &other) {
            m_buf  = new u8[other.m_size];
            m_size = other.m_size;
            m_owns_buf = true;
            memcpy(m_buf, other.m_buf, other.m_size);
        }
        Buffer(Buffer &&other) noexcept {
            m_buf        = std::move(other.m_buf);
            m_size       = other.m_size;
            m_owns_buf   = other.m_owns_buf;
            other.m_buf  = nullptr;
            other.m_size = 0;
            other.m_owns_buf = false;
        }
        ~Buffer() { free(); }

        bool alloc(size_t size) {
            free();
            if (size == 0)
                return false;
            m_buf = new byte_t[size];
            if (m_buf)
                m_size = size;
            m_owns_buf = true;
            return m_buf != nullptr;
        }

        bool initTo(char fill) {
            if (m_buf == nullptr)
                return false;
            memset(m_buf, fill, m_size);
            return true;
        }

        void free() {
            if (m_buf && m_owns_buf) {
                delete[] m_buf;
            }
            m_size = 0;
        }

        void resize(size_t size) {
            if (m_buf && size == m_size) {
                return;
            }
            void *new_buf = new u8[size];
            if (m_buf && m_size > 0) {
                memcpy(new_buf, m_buf, std::min(size, m_size));
                delete[] m_buf;
            }
            m_buf  = new_buf;
            m_size = size;
        }

        size_t size() const { return m_size; }

        template <typename T> T &as() { return get<T>(0); }

        template <typename T = void> T *buf() { return reinterpret_cast<T *>(m_buf); }
        void setBuf(void *buf, size_t size) {
            if (m_buf != buf) {
                free();
                m_buf = buf;
                m_owns_buf = false;
            }
            m_size = size;
        }

        template <typename T> T &get(size_t at) {
            TOOLBOX_CORE_ASSERT(m_buf);
            return *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + at);
        }

        template <typename T> T get(size_t at) const {
            TOOLBOX_CORE_ASSERT(m_buf);
            return *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + at);
        }

        template <typename T> void set(size_t at, const T &value) {
            TOOLBOX_CORE_ASSERT(m_buf);
            *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + at) = value;
        }

        template <typename T> void set(size_t at, T &&value) {
            TOOLBOX_CORE_ASSERT(m_buf);
            *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + at) = value;
        }

        template <typename T> bool goodFor() { return sizeof(T) <= m_size && m_buf != nullptr; }

        operator bool() const { return m_buf != nullptr; }

        byte_t &operator[](int index) {
            TOOLBOX_CORE_ASSERT(m_buf);
            return ((byte_t *)m_buf)[index];
        }
        byte_t operator[](int index) const {
            TOOLBOX_CORE_ASSERT(m_buf);
            return ((byte_t *)m_buf)[index];
        }

        bool operator==(const Buffer &other) { return m_buf == other.m_buf; }

    private:
        void *m_buf   = nullptr;
        size_t m_size = 0;
        bool m_owns_buf = true;
    };

}  // namespace Toolbox