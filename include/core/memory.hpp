#pragma once

#include <cstring>
#include <memory>

#include "core/assert.hpp"
#include "core/types.hpp"

namespace Toolbox {

    template <typename T> using RefPtr   = std::shared_ptr<T>;
    template <typename T> using ScopePtr = std::unique_ptr<T>;

    template <typename T, typename... Args> RefPtr<T> make_referable(Args &&...args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename F> RefPtr<T> ref_cast(const RefPtr<F> &ref) {
        return std::reinterpret_pointer_cast<T, F>(ref);
    }

    template <typename T, typename F> RefPtr<T> ref_cast(RefPtr<F> &&ref) {
        return std::reinterpret_pointer_cast<T, F>(ref);
    }

    template <typename T, typename F> RefPtr<T> ref_cast(ScopePtr<F> &&ptr) {
        static_assert(std::is_convertible_v<F *, T *>);
        return RefPtr<T>(static_cast<T *>(ptr.release()), [](T *p) { delete p; });
    }

    template <typename T, typename... Args> ScopePtr<T> make_scoped(Args &&...args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename _T> using Referable = std::enable_shared_from_this<_T>;

    class Buffer {
    public:
        using byte_t = u8;

        Buffer() = default;
        Buffer(const Buffer &other) { other.copyTo(*this); }
        Buffer(Buffer &&other) noexcept {
            m_buf.m_ext       = other.m_buf.m_ext;
            m_size            = other.m_size;
            m_owns_buf        = other.m_owns_buf;
            other.m_buf.m_ext = nullptr;
            other.m_size      = 0;
            other.m_owns_buf  = false;
        }
        ~Buffer() { free(); }

        bool alloc(uint32_t size) {
            if (size == 0) {
                return false;
            }
            if (m_owns_buf && !free()) {
                return false;
            }
            bool ret = true;
            if (!isSizeInline(size)) {
                m_buf.m_ext = static_cast<byte_t *>(std::malloc(size));
                ret &= m_buf.m_ext != nullptr;
            }
            if (ret) {
                m_size     = size;
                m_owns_buf = true;
            }
            return ret;
        }

        bool copyTo(Buffer &other) const {
#if 0
            other.resize(m_size);
            memcpy(other.buf(), buf(), m_size);
#else
            if (!other.resize(m_size))
                return false;
            memcpy(other.buf(), buf(), m_size);
#endif
            return true;
        }

        bool initTo(char fill) {
            byte_t *bf = buf<byte_t>();
            if (bf == nullptr)
                return false;
            memset(bf, fill, m_size);
            return true;
        }

        bool free() {
            if (!m_owns_buf) {
                return false;
            }

            if (!isInlineBuf()) {
                std::free(m_buf.m_ext);
                m_buf.m_ext = nullptr;
                m_owns_buf  = false;
            }

            m_size = 0;
        }

        bool resize(uint32_t size) {
            if (size == m_size) {
                return true;
            }

            if (size == 0) {
                return free();
            }

            if (!m_owns_buf) {
                if (m_size != 0) {
                    // Called resize on shared buf
                    return false;
                }

                return alloc(size);
            }

            uint32_t to_cpy = std::min(size, m_size);

            if (size < m_size) {
                if (!isInlineBuf()) {
                    if (isSizeInline(size)) {
                        byte_t *to_del = m_buf.m_ext;
                        memcpy(&m_buf.m_inline, to_del, to_cpy);
                        std::free(to_del);
                        m_size = size;
                    } else {
                        m_size = size;
                    }
                } else {
                    m_size = size;
                }
            } else {
                if (!isInlineBuf()) {
                    byte_t *new_buf = new byte_t[size];
                    memcpy(new_buf, m_buf.m_ext, to_cpy);
                    std::free(m_buf.m_ext);
                    setOwnBuf(new_buf, size);
                } else {
                    if (isSizeInline(size)) {
                        m_size = size;
                    } else {
                        byte_t *new_buf = new byte_t[size];
                        memcpy(new_buf, &m_buf.m_inline, to_cpy);
                        setOwnBuf(new_buf, size);
                    }
                }
            }

            return true;
        }

        uint32_t size() const noexcept { return m_size; }

        template <typename T> T &as() { return get<T>(0); }

        template <typename T = void> T *buf() noexcept {
            if (!isInlineBuf()) {
                return reinterpret_cast<T *>(m_buf.m_ext);
            }
            return reinterpret_cast<T *>(&m_buf.m_inline);
        }

        template <typename T = void> const T *buf() const noexcept {
            if (!isInlineBuf()) {
                return reinterpret_cast<const T *>(m_buf.m_ext);
            }
            return reinterpret_cast<const T *>(&m_buf.m_inline);
        }

        void setBuf(void *buf, uint32_t size) {
            if (m_buf.m_ext == (byte_t *)buf) {
                return;
            }

            free();
            m_buf.m_ext = (byte_t *)buf;
            m_owns_buf  =  false;
            m_size      = size;
        }

        template <typename T> T &get(uint32_t ofs) {
            byte_t *bf = buf<byte_t>();
            TOOLBOX_CORE_ASSERT(bf);
            return *reinterpret_cast<std::decay_t<T> *>(bf + ofs);
        }

        template <typename T> T get(uint32_t ofs) const {
            const byte_t *bf = buf<byte_t>();
            TOOLBOX_CORE_ASSERT(bf);
            return *reinterpret_cast<const std::decay_t<T> *>(bf + ofs);
        }

        template <typename T> void set(uint32_t ofs, const T &value) {
            byte_t *bf = buf<byte_t>();
            TOOLBOX_CORE_ASSERT(bf);
            *reinterpret_cast<std::decay_t<T> *>(bf + ofs) = value;
        }

        template <typename T> void set(uint32_t ofs, T &&value) {
            byte_t *bf = buf<byte_t>();
            TOOLBOX_CORE_ASSERT(bf);
            *reinterpret_cast<std::decay_t<T> *>(bf + ofs) = std::move(value);
        }

        template <typename T> bool goodFor() const noexcept { return sizeof(T) <= m_size; }

        operator bool() const { return m_size > 0; }

        byte_t &operator[](int index) {
            byte_t *bf = buf<byte_t>();
            TOOLBOX_CORE_ASSERT(bf && index < m_size);
            return bf[index];
        }

        byte_t operator[](int index) const {
            const byte_t *bf = buf<byte_t>();
            TOOLBOX_CORE_ASSERT(bf && index < m_size);
            return bf[index];
        }

        Buffer &operator=(const Buffer &other) {
            other.copyTo(*this);
            return *this;
        }

        Buffer &operator=(Buffer &&other) noexcept {
            m_buf.m_ext       = other.m_buf.m_ext;
            m_size            = other.m_size;
            m_owns_buf        = other.m_owns_buf;
            other.m_buf.m_ext = nullptr;
            other.m_size      = 0;
            other.m_owns_buf  = false;
            return *this;
        }

        bool operator==(const Buffer &other) noexcept {
            return m_buf.m_ext == other.m_buf.m_ext && m_size == other.m_size;
        }

    private:
        inline bool isInlineBuf() const noexcept {
            return m_size <= sizeof(m_buf.m_inline) && m_owns_buf;
        }

        inline bool isSizeInline(uint32_t size) const noexcept {
            return size <= sizeof(m_buf.m_inline);
        }

        void setOwnBuf(void *buf, uint32_t size) {
            if (m_buf.m_ext == (byte_t *)buf) {
                return;
            }

            free();
            m_buf.m_ext = (byte_t *)buf;
            m_owns_buf  = true;
            m_size      = size;
        }

        union {
            byte_t *m_ext;
            uint64_t m_inline;
        } m_buf;
        uint32_t m_size   = 0;
        bool m_owns_buf = false;
    };

}  // namespace Toolbox