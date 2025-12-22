#pragma once

// License: GNU General Public License v3.0
// Copyright (c) 2025 JoshuaMK
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

#ifndef GCM_NAMESPACE
#define GCM_NAMESPACE gcm
#endif

#ifndef GCM_HAS_CXX11
#if ((defined(_MSC_VER) && _MSVC_LANG >= 201103L) || __cplusplus >= 201103L)
#define GCM_HAS_CXX11 1
#else
#define GCM_HAS_CXX11 0
#endif
#endif

#if !GCM_HAS_CXX11
#error "This library requires C++11 or newer!"
#endif

#ifndef GCM_HAS_CXX14
#if ((defined(_MSC_VER) && _MSVC_LANG >= 201402L) || __cplusplus >= 201402L)
#define GCM_HAS_CXX14 1
#else
#define GCM_HAS_CXX14 0
#endif
#endif

#ifndef GCM_HAS_CXX17
#if ((defined(_MSC_VER) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
#define GCM_HAS_CXX17 1
#else
#define GCM_HAS_CXX17 0
#endif
#endif

#ifndef GCM_HAS_CXX20
#if ((defined(_MSC_VER) && _MSVC_LANG >= 202002L) || __cplusplus >= 202002L)
#define GCM_HAS_CXX20 1
#else
#define GCM_HAS_CXX20 0
#endif
#endif

#ifndef GCM_HAS_CXX23
#if ((defined(_MSC_VER) && _MSVC_LANG >= 202302L) || __cplusplus >= 202302L)
#define GCM_HAS_CXX23 1
#else
#define GCM_HAS_CXX23 0
#endif
#endif

#if GCM_HAS_CXX17
#define GCM_DEPRECATED [[deprecated]]
#define GCM_NODISCARD [[nodiscard]]
#define GCM_CONSTEXPR constexpr
#else
#define GCM_DEPRECATED
#define GCM_NODISCARD
#define GCM_CONSTEXPR
#endif

#define GCM_NOEXCEPT noexcept
#define GCM_INLINE inline

#if GCM_HAS_CXX20
#define GCM_LIKELY(x) (x) [[likely]]
#define GCM_UNLIKELY(x) (x) [[unlikely]]
#elif defined(__GNUC__)
#define GCM_LIKELY(x) (__builtin_expect(!!(x), 1))
#define GCM_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define GCM_LIKELY(x) (x)
#define GCM_UNLIKELY(x) (x)
#endif

#define GCM_STATIC_ASSERT(expr, errmsg) static_assert((expr), (errmsg))
#define GCM_RUNTIME_ASSERT(expr, errmsg) assert((expr) && (errmsg))

namespace gcm {

    using s8 = std::int8_t;
    using u8 = std::uint8_t;
    using s16 = std::int16_t;
    using u16 = std::uint16_t;
    using s32 = std::int32_t;
    using u32 = std::uint32_t;
    using s64 = std::int64_t;
    using u64 = std::uint64_t;
    using f32 = float;
    using f64 = double;

}

#if GCM_HAS_CXX23 && false  // windows msvc and ubuntu clang with C++23 flags float/double due to integral concept enforcement, so this is disabled for now
#include <bit>
#define GCM_BYTESWAP(val) std::byteswap((val))
#else
namespace gcm {

#if GCM_HAS_CXX17
        template <typename T>
        inline constexpr T byteswap(T val) {
            if constexpr (std::is_arithmetic_v<T>) {
                union {
                    T value;
                    unsigned char bytes[sizeof(T)];
                } src {}, dst {};

                src.value = val;

                for (size_t i = 0; i < sizeof(T); i++) {
                    dst.bytes[i] = src.bytes[sizeof(T) - 1 - i];
                }

                return dst.value;
            }

            else {
                static_assert(std::is_arithmetic_v<T>, "byteswap only supports arithmetic types");
                return val;
            }
        }
#else
        template <typename T>
        inline typename std::enable_if<std::is_arithmetic<T>::value, T>::type
        byteswap(T val) {
            union {
                T value;
                unsigned char bytes[sizeof(T)];
            } src = {}, dst = {};

            src.value = val;

            // Standard C++11 loop
            for (size_t i = 0; i < sizeof(T); i++) {
                dst.bytes[i] = src.bytes[sizeof(T) - 1 - i];
            }

            return dst.value;
        }
#endif

}
#define GCM_BYTESWAP(val) gcm::byteswap((val))
#endif

#if !GCM_HAS_CXX14
namespace std {
    template <class _Ty, class... _Types, typename enable_if<!is_array<_Ty>::value, int>::type = 0>
    GCM_NODISCARD inline unique_ptr<_Ty> make_unique(_Types&&... _Args)
    { // make a unique_ptr
        return unique_ptr<_Ty>(new _Ty(std::forward<_Types>(_Args)...));
    }

    template <class _Ty, typename enable_if<is_array<_Ty>::value && extent<_Ty>::value == 0, int>::type = 0>
    GCM_NODISCARD inline unique_ptr<_Ty> make_unique(const size_t _Size)
    { // make a unique_ptr
        using _Elem = typename remove_extent<_Ty>::type;
        return unique_ptr<_Ty>(new _Elem[_Size]());
    }

    template <class _Ty, class... _Types, typename enable_if<extent<_Ty>::value != 0, int>::type = 0>
    void make_unique(_Types&&...) = delete;
}
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define GCM_BIG_TO_SYSTEM_ENDIAN(val) (val)
#define GCM_SYSTEM_TO_BIG_ENDIAN(val) (val)
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define GCM_BIG_TO_SYSTEM_ENDIAN(val) GCM_BYTESWAP(val)
#define GCM_SYSTEM_TO_BIG_ENDIAN(val) GCM_BYTESWAP(val)
#elif defined(_WIN32) || defined(_WIN64)
#define GCM_BIG_TO_SYSTEM_ENDIAN(val) GCM_BYTESWAP(val)
#define GCM_SYSTEM_TO_BIG_ENDIAN(val) GCM_BYTESWAP(val)
#else
#define GCM_BIG_TO_SYSTEM_ENDIAN(val) GCM_BYTESWAP(val)
#define GCM_SYSTEM_TO_BIG_ENDIAN(val) GCM_BYTESWAP(val)
#endif

#if GCM_HAS_CXX20
#include <span>

namespace gcm {

    using ByteView = std::span<const u8>;

    // make_view(ptr, size)
    GCM_CONSTEXPR inline ByteView make_view(const u8* ptr, size_t count) GCM_NOEXCEPT {
        return ByteView(ptr, count);
    }

    // make_view(C-Array)
    template <std::size_t N>
    GCM_CONSTEXPR inline ByteView make_view(const u8 (&arr)[N]) GCM_NOEXCEPT {
        return ByteView(arr, N);
    }

    // make_view(Container)
    template <typename Container>
    inline ByteView make_view(Container& c)
    {
        return ByteView(c);
    }

}
#else
namespace gcm {

    namespace detail {

        template <typename T>
        class span {
        public:
            // Standard STL typedefs
            using element_type = T;
            using value_type = typename std::remove_cv<T>::type;
            using index_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using const_pointer = const T*;
            using reference = T&;
            using const_reference = const T&;
            using iterator = T*;
            using const_iterator = const T*;
            using reverse_iterator = std::reverse_iterator<iterator>;

            // Default (empty)
            GCM_CONSTEXPR span() GCM_NOEXCEPT
                : ptr_(nullptr)
                , len_(0) {}

            // Pointer + Size
            GCM_CONSTEXPR span(pointer ptr, index_type count) GCM_NOEXCEPT
                : ptr_(ptr)
                , len_(count) {}

            // Pointer + Pointer (Start, End)
            GCM_CONSTEXPR span(pointer first, pointer last) GCM_NOEXCEPT
                : ptr_(first)
                , len_(static_cast<index_type>(last - first)) {}

            // C-Style Array (e.g., int arr[5])
            template <std::size_t N>
            GCM_CONSTEXPR span(element_type (&arr)[N]) GCM_NOEXCEPT
                : ptr_(arr)
                , len_(N) {}

            // Generic Container Constructor (std::vector, std::array, etc.)
            // Uses SFINAE to ensure the container has compatible .data() and .size()
            template <typename Container,
                typename = typename std::enable_if<
                    !std::is_same<typename std::remove_const<Container>::type, span>::value && std::is_convertible<decltype(std::declval<Container>().data()), pointer>::value>::type>
            span(Container& cont) GCM_NOEXCEPT
                : ptr_(cont.data())
                , len_(cont.size()) {}

            // Const-conversion (e.g., span<int> -> span<const int>)
            template <typename U,
                typename = typename std::enable_if<
                    std::is_same<typename std::remove_const<T>::type, typename std::remove_const<U>::type>::value && std::is_convertible<U*, T*>::value>::type>
            GCM_CONSTEXPR span(const span<U>& other) GCM_NOEXCEPT
                : ptr_(other.data())
                , len_(other.size()) {}

            GCM_CONSTEXPR pointer data() const GCM_NOEXCEPT { return ptr_; }
            GCM_CONSTEXPR index_type size() const GCM_NOEXCEPT { return len_; }
            GCM_CONSTEXPR index_type size_bytes() const GCM_NOEXCEPT { return len_ * sizeof(T); }
            GCM_CONSTEXPR bool empty() const GCM_NOEXCEPT { return len_ == 0; }

            GCM_CONSTEXPR reference operator[](index_type idx) const GCM_NOEXCEPT {
                return ptr_[idx];
            }

            GCM_CONSTEXPR reference front() const GCM_NOEXCEPT { return ptr_[0]; }
            GCM_CONSTEXPR reference back() const GCM_NOEXCEPT { return ptr_[len_ - 1]; }

            // ----------------------------------------------------------------
            // Iterators
            // ----------------------------------------------------------------

            GCM_CONSTEXPR iterator begin() const GCM_NOEXCEPT { return ptr_; }
            GCM_CONSTEXPR iterator end() const GCM_NOEXCEPT { return ptr_ + len_; }

            GCM_CONSTEXPR const_iterator cbegin() const GCM_NOEXCEPT { return ptr_; }
            GCM_CONSTEXPR const_iterator cend() const GCM_NOEXCEPT { return ptr_ + len_; }

            span<T> first(index_type count) const
            {
                assert(count <= len_);
                return span<T>(ptr_, count);
            }

            span<T> last(index_type count) const
            {
                assert(count <= len_);
                return span<T>(ptr_ + (len_ - count), count);
            }

            span<T> subspan(index_type offset, index_type count = static_cast<index_type>(-1)) const
            {
                assert(offset <= len_);
                if (count == static_cast<index_type>(-1))
                    count = len_ - offset;
                assert(offset + count <= len_);
                return span<T>(ptr_ + offset, count);
            }

        private:
            pointer ptr_;
            index_type len_;
        };

    }

    // make_span(ptr, size)
    template <typename T>
    GCM_CONSTEXPR inline detail::span<T> make_span(T* ptr, size_t count) GCM_NOEXCEPT {
        return detail::span<T>(ptr, count);
    }

    // make_span(C-Array)
    template <typename T, std::size_t N>
    GCM_CONSTEXPR inline detail::span<T> make_span(T (&arr)[N]) GCM_NOEXCEPT {
        return detail::span<T>(arr, N);
    }

    // make_span(Container)
    template <typename Container>
    inline detail::span<typename Container::value_type> make_span(Container& c) {
        return detail::span<typename Container::value_type>(c);
    }

    using ByteView = gcm::detail::span<const u8>;

    // make_view(ptr, size)
    GCM_CONSTEXPR inline ByteView make_view(const u8* ptr, size_t count) GCM_NOEXCEPT {
        return ByteView(ptr, count);
    }

    // make_view(C-Array)
    template <std::size_t N>
    GCM_CONSTEXPR inline ByteView make_view(const u8 (&arr)[N]) GCM_NOEXCEPT {
        return ByteView(arr, N);
    }

    // make_view(Container)
    template <typename Container>
    inline ByteView make_view(Container& c) {
        return ByteView(c);
    }

}
#endif

namespace gcm {

    template <typename T>
    GCM_NODISCARD GCM_CONSTEXPR inline typename std::enable_if<std::is_integral<T>::value, T>::type
    RoundUp(T val, T to) {
        return (val + to - 1) & ~(to - 1);
    }
    template <typename T>
    GCM_NODISCARD GCM_CONSTEXPR inline typename std::enable_if<std::is_integral<T>::value, T>::type
    RoundDown(T val, T to) {
        return val & ~(to - 1);
    }

#define _GCM_CLASS_COPYABLE(class_)              \
    class_ (const class_ &) = default;           \
    class_ &operator=(const class_ &) = default;

#define _GCM_CLASS_MOVEABLE(class_)                      \
    class_ (class_  &&) GCM_NOEXCEPT = default;          \
    class_ &operator=(class_ &&) GCM_NOEXCEPT = default;

#if GCM_HAS_CXX20
#define _GCM_CLASS_EQUIVALENCE(class_) GCM_NODISCARD bool operator==(const class_ &) const GCM_NOEXCEPT = default;
#endif

    enum class ERegion {
        NTSCJ = 0,
        NTSCU = 1,
        PAL = 2,
        NTSCK = 0,
        UNKNOWN = -1,
    };

    enum class EConsole {
        GCN = 0,
        WII = 1,
        UNKNOWN = -1,
    };

    class Apploader {
    public:
        GCM_NODISCARD static std::unique_ptr<Apploader> FromData(ByteView _in);
        GCM_NODISCARD static std::unique_ptr<Apploader> FromFile(const std::string& _path);
        GCM_NODISCARD bool ToData(std::vector<u8>& _out) const;
        GCM_NODISCARD bool ToFile(const std::string& _path) const;

        GCM_NODISCARD bool IsValid() const GCM_NOEXCEPT;

        GCM_NODISCARD std::string GetBuildDate() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntryPoint() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetLoaderSize() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetTrailerSize() const GCM_NOEXCEPT;
        GCM_NODISCARD ByteView GetLoaderView() const GCM_NOEXCEPT;
        GCM_NODISCARD ByteView GetTrailerView() const GCM_NOEXCEPT;

        void SetBuildDate(const std::string&) GCM_NOEXCEPT;
        void SetEntryPoint(u32) GCM_NOEXCEPT;
        void SetLoaderData(ByteView) GCM_NOEXCEPT;
        void SetTrailerData(ByteView) GCM_NOEXCEPT;

    public:
        Apploader();
        ~Apploader() = default;

        _GCM_CLASS_COPYABLE(Apploader)
        _GCM_CLASS_MOVEABLE(Apploader)

#if GCM_HAS_CXX20
        _GCM_CLASS_EQUIVALENCE(Apploader)
#else
        GCM_NODISCARD bool operator==(const Apploader& other) const GCM_NOEXCEPT {
            return m_data == other.m_data;
        }
#endif

    private:
        std::vector<u8> m_data;
    };

    class DOLExecutable {
    public:
#if GCM_HAS_CXX17
        static GCM_CONSTEXPR const u32 INVALID_SECTION = 0xFF;
        static GCM_CONSTEXPR const u32 MAX_TEXT_SECTIONS = 7;
        static GCM_CONSTEXPR const u32 MAX_DATA_SECTIONS = 11;
#else
        enum : u32 { INVALID_SECTION = 0xFF, MAX_TEXT_SECTIONS = 7, MAX_DATA_SECTIONS = 11 };
#endif

        GCM_NODISCARD static std::unique_ptr<DOLExecutable> FromData(ByteView _in);
        GCM_NODISCARD static std::unique_ptr<DOLExecutable> FromFile(const std::string& _path);
        GCM_NODISCARD bool ToData(std::vector<u8>& _out) const;
        GCM_NODISCARD bool ToFile(const std::string& _path) const;

        GCM_NODISCARD bool IsValid() const GCM_NOEXCEPT;

        GCM_NODISCARD u32 GetBSSAddress() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetBSSSize() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntryAddress() const GCM_NOEXCEPT;
        
        void SetBSSAddress(u32) GCM_NOEXCEPT;
        void SetBSSSize(u32) GCM_NOEXCEPT;
        void SetEntryAddress(u32) GCM_NOEXCEPT;

        GCM_NODISCARD u8 GetTextSectionCount() const GCM_NOEXCEPT;
        GCM_NODISCARD u8 GetDataSectionCount() const GCM_NOEXCEPT;

        GCM_NODISCARD u8 GetTextSectionForAddress(u32 virtual_addr, u32 desired_len = 1) const GCM_NOEXCEPT;
        GCM_NODISCARD u8 GetDataSectionForAddress(u32 virtual_addr, u32 desired_len = 1) const GCM_NOEXCEPT;

        GCM_NODISCARD u32 GetTextSectionAddress(u8 section_idx) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetDataSectionAddress(u8 section_idx) const GCM_NOEXCEPT;

        GCM_NODISCARD ByteView GetTextSectionView(u8 section_idx) const GCM_NOEXCEPT;
        GCM_NODISCARD ByteView GetDataSectionView(u8 section_idx) const GCM_NOEXCEPT;

        void SetTextSectionData(u8 section_idx, ByteView) GCM_NOEXCEPT;
        void SetDataSectionData(u8 section_idx, ByteView) GCM_NOEXCEPT;

        GCM_NODISCARD bool ReadAddressBool(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD s8 ReadAddressS8(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD u8 ReadAddressU8(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD s16 ReadAddressS16(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD u16 ReadAddressU16(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD s32 ReadAddressS32(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 ReadAddressU32(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD f32 ReadAddressF32(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD f64 ReadAddressF64(u32 virtual_addr) const GCM_NOEXCEPT;
        GCM_NODISCARD std::string ReadAddressCString(u32 virtual_addr) const GCM_NOEXCEPT;

        void WriteAddressBool(u32 virtual_addr, bool) GCM_NOEXCEPT;
        void WriteAddressS8(u32 virtual_addr, s8) GCM_NOEXCEPT;
        void WriteAddressU8(u32 virtual_addr, u8) GCM_NOEXCEPT;
        void WriteAddressS16(u32 virtual_addr, s16) GCM_NOEXCEPT;
        void WriteAddressU16(u32 virtual_addr, u16) GCM_NOEXCEPT;
        void WriteAddressS32(u32 virtual_addr, s32) GCM_NOEXCEPT;
        void WriteAddressU32(u32 virtual_addr, u32) GCM_NOEXCEPT;
        void WriteAddressF32(u32 virtual_addr, f32) GCM_NOEXCEPT;
        void WriteAddressF64(u32 virtual_addr, f64) GCM_NOEXCEPT;
        void WriteAddressCString(u32 virtual_addr, const std::string&) GCM_NOEXCEPT;

    public:
        DOLExecutable();
        ~DOLExecutable() = default;

        _GCM_CLASS_COPYABLE(DOLExecutable)
        _GCM_CLASS_MOVEABLE(DOLExecutable)

#if GCM_HAS_CXX20
        _GCM_CLASS_EQUIVALENCE(DOLExecutable)
#else
        GCM_NODISCARD bool operator==(const DOLExecutable& other) const GCM_NOEXCEPT {
            return m_data == other.m_data;
        }
#endif

    private:
        std::vector<u8> m_data;
    };

    class BI2Sector {
    public:
#if GCM_HAS_CXX17
        static GCM_CONSTEXPR const u32 FORMAT_SIZE = 0x2000;
#else
        enum : u32 { FORMAT_SIZE = 0x2000 };
#endif

        GCM_NODISCARD static std::unique_ptr<BI2Sector> FromData(ByteView _in);
        GCM_NODISCARD static std::unique_ptr<BI2Sector> FromFile(const std::string& _path);
        GCM_NODISCARD bool ToData(std::vector<u8>& _out) const;
        GCM_NODISCARD bool ToFile(const std::string& _path) const;

        GCM_NODISCARD bool IsValid() const GCM_NOEXCEPT;

        GCM_NODISCARD u32 GetArgumentOffset() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetDebugMonitorSize() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetDebugFlag() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetSimulatedMemSize() const GCM_NOEXCEPT;
        GCM_NODISCARD ERegion GetRegion() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetTrackLocation() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetTrackSize() const GCM_NOEXCEPT;

        void SetArgumentOffset(u32 _val) GCM_NOEXCEPT;
        void SetDebugMonitorSize(u32 _val) GCM_NOEXCEPT;
        void SetDebugFlag(u32 _val) GCM_NOEXCEPT;
        void SetSimulatedMemSize(u32 _val) GCM_NOEXCEPT;
        void SetRegion(ERegion _val) GCM_NOEXCEPT;
        void SetTrackLocation(u32 _val) GCM_NOEXCEPT;
        void SetTrackSize(u32 _val) GCM_NOEXCEPT;

    public:
        BI2Sector();
        ~BI2Sector() = default;

        _GCM_CLASS_COPYABLE(BI2Sector)
        _GCM_CLASS_MOVEABLE(BI2Sector)

#if GCM_HAS_CXX20
        _GCM_CLASS_EQUIVALENCE(BI2Sector)
#else
        GCM_NODISCARD bool operator==(const BI2Sector& other) const GCM_NOEXCEPT {
            return m_data == other.m_data;
        }
#endif

    private:
        std::array<u8, FORMAT_SIZE> m_data;
    };

    class BootSector {
    public:
#if GCM_HAS_CXX17
        static GCM_CONSTEXPR const u32 FORMAT_SIZE = 0x440;
#else
        enum : u32 { FORMAT_SIZE = 0x440 };
#endif

        GCM_NODISCARD static std::unique_ptr<BootSector> FromData(ByteView _in);
        GCM_NODISCARD static std::unique_ptr<BootSector> FromFile(const std::string& _path);
        GCM_NODISCARD bool ToData(std::vector<u8>& _out) const;
        GCM_NODISCARD bool ToFile(const std::string& _path) const;

        GCM_NODISCARD bool IsValid() const GCM_NOEXCEPT;

        GCM_NODISCARD u32 GetGameCode() const GCM_NOEXCEPT;
        GCM_NODISCARD u16 GetMakerCode() const GCM_NOEXCEPT;
        GCM_NODISCARD u8 GetDiskID() const GCM_NOEXCEPT;
        GCM_NODISCARD u8 GetDiskVersion() const GCM_NOEXCEPT;
        GCM_NODISCARD bool GetAudioStreamToggle() const GCM_NOEXCEPT;
        GCM_NODISCARD u8 GetAudioStreamBufferSize() const GCM_NOEXCEPT;
        GCM_NODISCARD EConsole GetTargetConsole() const GCM_NOEXCEPT;
        GCM_NODISCARD std::string GetGameName() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetDebugMonitorOffset() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetDebugMonitorVirtualAddress() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetMainDOLOffset() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetFSTOffset() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetFSTSize() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetFSTCapacity() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetVirtualAddress() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetFirstFileOffset() const GCM_NOEXCEPT;

        void SetGameCode(u32) GCM_NOEXCEPT;
        void SetMakerCode(u16) GCM_NOEXCEPT;
        void SetDiskID(u8) GCM_NOEXCEPT;
        void SetDiskVersion(u8) GCM_NOEXCEPT;
        void SetAudioStreamToggle(bool) GCM_NOEXCEPT;
        void SetAudioStreamBufferSize(u8) GCM_NOEXCEPT;
        void SetTargetConsole(EConsole) GCM_NOEXCEPT;
        void SetGameName(const std::string &) GCM_NOEXCEPT;
        void SetDebugMonitorOffset(u32) GCM_NOEXCEPT;
        void SetDebugMonitorVirtualAddress(u32) GCM_NOEXCEPT;
        void SetMainDOLOffset(u32) GCM_NOEXCEPT;
        void SetFSTOffset(u32) GCM_NOEXCEPT;
        void SetFSTSize(u32) GCM_NOEXCEPT;
        void SetFSTCapacity(u32) GCM_NOEXCEPT;
        void SetVirtualAddress(u32) GCM_NOEXCEPT;
        void SetFirstFileOffset(u32) GCM_NOEXCEPT;

    public:
        BootSector();
        ~BootSector() = default;

        _GCM_CLASS_COPYABLE(BootSector)
        _GCM_CLASS_MOVEABLE(BootSector)

#if GCM_HAS_CXX20
        _GCM_CLASS_EQUIVALENCE(BootSector)
#else
        GCM_NODISCARD bool operator==(const BootSector& other) const GCM_NOEXCEPT {
            return m_data == other.m_data;
        }
#endif

    private:
        enum HardwareMagic {
            MAGIC_GAMECUBE = 0xC2339F3D,
            MAGIC_WII = 0x5D1C9EA3,
        };

        std::array<u8, FORMAT_SIZE> m_data;
    };

    class FSTSector {
    public:
        enum class EntryType {
            FILE = 0,
            DIRECTORY = 1,
            UNKNOWN = -1,
        };

#if GCM_HAS_CXX17
        static GCM_CONSTEXPR const u32 INVALID_ENTRYNUM = 0xFFFFFFFF;
#else
        enum : u32 { INVALID_ENTRYNUM = 0xFFFFFFFF };
#endif

        GCM_NODISCARD static std::unique_ptr<FSTSector> FromData(ByteView _in);
        GCM_NODISCARD static std::unique_ptr<FSTSector> FromFile(const std::string& _path);
        GCM_NODISCARD bool ToData(std::vector<u8>& _out) const;
        GCM_NODISCARD bool ToFile(const std::string& _path) const;

        GCM_NODISCARD bool IsValid() const GCM_NOEXCEPT;

        GCM_NODISCARD u32 GetEntryPositionMin() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntryPositionMax() const GCM_NOEXCEPT;
        void SetEntryPositionMin(u32 min) GCM_NOEXCEPT;
        void SetEntryPositionMax(u32 max) GCM_NOEXCEPT;

        GCM_NODISCARD u32 GetRootEntryNum() const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntryNum(u32 cwd_entrynum, const std::string& path) const GCM_NOEXCEPT;
        GCM_NODISCARD std::string GetEntryPath(u32 cwd_entrynum, u32 entrynum) const GCM_NOEXCEPT;
        
        GCM_NODISCARD EntryType GetEntryType(u32 entrynum) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntryPosition(u32 entrynum) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntrySize(u32 entrynum) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetEntryParent(u32 entrynum) const GCM_NOEXCEPT;

        void SetEntryPosition(u32 entrynum, u32) GCM_NOEXCEPT;
        void SetEntrySize(u32 entrynum, u32) GCM_NOEXCEPT;

        GCM_NODISCARD u32 CreateEntry(u32 cwd_entrynum, EntryType type, const std::string& name) GCM_NOEXCEPT;
        GCM_NODISCARD bool RemoveEntry(u32 entrynum, bool recursive = false) GCM_NOEXCEPT;

        // First child, if any.
        GCM_NODISCARD u32 GetFirst(u32 entrynum) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetLast(u32 entrynum) const GCM_NOEXCEPT;
        
        // Next sibling. If recursive, next child before next sibling.
        GCM_NODISCARD u32 GetNext(u32 entrynum, bool recursive = false) const GCM_NOEXCEPT;
        GCM_NODISCARD u32 GetPrev(u32 entrynum, bool recursive = false) const GCM_NOEXCEPT;

        struct FileRuleset {
            char m_extension[8];
            u32 m_alignment;
        };
        GCM_NODISCARD bool RecalculatePositions(const std::vector<FileRuleset>& rulesets) GCM_NOEXCEPT;

    public:
        FSTSector();
        ~FSTSector() = default;

        _GCM_CLASS_COPYABLE(FSTSector)
        _GCM_CLASS_MOVEABLE(FSTSector)

#if GCM_HAS_CXX20
        _GCM_CLASS_EQUIVALENCE(FSTSector)
#else
        GCM_NODISCARD bool operator==(const FSTSector& other) const GCM_NOEXCEPT {
            return m_file_nodes == other.m_file_nodes && m_str_table == other.m_str_table;
        }
#endif

    private:
        struct LowFileNode {
            u8 m_type;
            u8 m_str_ofs[3];
            union {
                u32 m_position; // File
                u32 m_parent; // Dir
            };
            union {
                u32 m_size; // File
                u32 m_next; // Dir
            };

            GCM_NODISCARD GCM_CONSTEXPR bool operator==(const LowFileNode& other) const GCM_NOEXCEPT {
                return *(u32*)&m_type == *(u32*)&other.m_type && m_position == other.m_position && m_size == other.m_size;
            }
        };

        u32 m_entry_pos_min = 0;
        u32 m_entry_pos_max = 0xFFFFFFFF;
        std::vector<LowFileNode> m_file_nodes;
        std::vector<char> m_str_table;
    };

    class GCM {
    public:
#if GCM_HAS_CXX17
        static GCM_CONSTEXPR const u32 FORMAT_SIZE = 0x57058000;
#else
        enum : u32 { FORMAT_SIZE = 0x57058000 };
#endif

        GCM_NODISCARD static std::unique_ptr<GCM> FromData(ByteView _in);
        GCM_NODISCARD static std::unique_ptr<GCM> FromISO(const std::string& _path);
        GCM_NODISCARD static std::unique_ptr<GCM> FromDirectory(const std::string& _path);
        GCM_NODISCARD bool ToData(std::vector<u8>& _out) const GCM_NOEXCEPT;
        GCM_NODISCARD bool ToISO(const std::string& _path) const;
        GCM_NODISCARD bool ToDirectory(const std::string& _path) const;

    public:
        GCM() = default;
        ~GCM() = default;

        _GCM_CLASS_MOVEABLE(GCM)

#if GCM_HAS_CXX20
        _GCM_CLASS_EQUIVALENCE(GCM)
#else
        GCM_NODISCARD bool operator==(const GCM& other) const GCM_NOEXCEPT {
            return m_apploader == other.m_apploader
                && m_bi2 == other.m_bi2
                && m_boot == other.m_boot
                && m_fst == other.m_fst;
        }
#endif

    private:
        std::unique_ptr<Apploader> m_apploader;
        std::unique_ptr<BI2Sector> m_bi2;
        std::unique_ptr<BootSector> m_boot;
        std::unique_ptr<DOLExecutable> m_dol;
        std::unique_ptr<FSTSector> m_fst;
    };

#undef _GCM_CLASS_COPYABLE
#undef _GCM_CLASS_MOVEABLE

#if GCM_HAS_CXX20
#undef _GCM_CLASS_EQUIVALENCE
#endif

}
