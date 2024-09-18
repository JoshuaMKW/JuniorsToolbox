#pragma once

#include <memory>
#include <cstring>

#include "core/assert.hpp"
#include "core/types.hpp"

namespace Toolbox {

    template <typename T> using RefPtr = std::shared_ptr<T>;
    template <typename T, typename... Args> RefPtr<T> make_referable(Args &&...args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename F> RefPtr<T> ref_cast(const RefPtr<F> &ref) {
        return std::reinterpret_pointer_cast<T, F>(ref);
    }

    template <typename T, typename F> RefPtr<T> ref_cast(RefPtr<F> &&ref) {
        return std::reinterpret_pointer_cast<T, F>(ref);
    }

    template <typename T> using ScopePtr = std::unique_ptr<T>;
    template <typename T, typename... Args> ScopePtr<T> make_scoped(Args &&...args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename _T>
    using Referable = std::enable_shared_from_this<_T>;

    class Buffer {
    public:
        using byte_t = u8;

        Buffer() = default;
        Buffer(const Buffer &other) { other.copyTo(*this); }
        Buffer(Buffer &&other) noexcept {
            m_buf            = std::move(other.m_buf);
            m_size           = other.m_size;
            m_owns_buf       = other.m_owns_buf;
            other.m_buf      = nullptr;
            other.m_size     = 0;
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

        bool copyTo(Buffer &other) const {
            if (!other.alloc(m_size))
                return false;
            memcpy(other.m_buf, m_buf, m_size);
            return true;
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
            byte_t *new_buf = new byte_t[size];
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
        template <typename T = void> const T *buf() const { return reinterpret_cast<T *>(m_buf); }

        void setBuf(void *buf, size_t size) {
            if (m_buf != (byte_t*)buf) {
                free();
                m_buf      = (byte_t*)buf;
                m_owns_buf = false;
            }
            m_size = size;
        }

        template <typename T> T &get(size_t ofs) {
            TOOLBOX_CORE_ASSERT(m_buf);
            return *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + ofs);
        }

        template <typename T> T get(size_t ofs) const {
            TOOLBOX_CORE_ASSERT(m_buf);
            return *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + ofs);
        }

        template <typename T> void set(size_t ofs, const T &value) {
            TOOLBOX_CORE_ASSERT(m_buf);
            *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + ofs) = value;
        }

        template <typename T> void set(size_t ofs, T &&value) {
            TOOLBOX_CORE_ASSERT(m_buf);
            *reinterpret_cast<std::decay_t<T> *>(reinterpret_cast<char *>(m_buf) + ofs) = value;
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

        Buffer &operator=(Buffer &&other) noexcept {
            m_buf            = std::move(other.m_buf);
            m_size           = other.m_size;
            m_owns_buf       = other.m_owns_buf;
            other.m_buf      = nullptr;
            other.m_size     = 0;
            other.m_owns_buf = false;
            return *this;
        }

        bool operator==(const Buffer &other) { return m_buf == other.m_buf; }

    private:
        byte_t *m_buf   = nullptr;
        size_t m_size   = 0;
        bool m_owns_buf = true;
    };

}  // namespace Toolbox