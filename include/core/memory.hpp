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

        Buffer()               = default;
        Buffer(const Buffer &) = default;
        Buffer(Buffer &&)      = default;

        bool alloc(size_t size) {
            free();
            if (size == 0)
                return false;
            m_buf = new byte_t[size];
        }

        bool initTo(char fill) {
            if (m_buf == nullptr)
                return false;
            memset(m_buf, fill, m_size);
            return true;
        }

        void free() {
            delete[] m_buf;
            m_size = 0;
        }

        template <typename T> T &as() { get<T>(0); }

        template <typename T> T &get(size_t at) {
            TOOLBOX_CORE_ASSERT(m_buf);
            return *reinterpret_cast<T>(((byte_t *)m_buf) + at);
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

    private:
        void *m_buf;
        size_t m_size;
    };

}  // namespace Toolbox