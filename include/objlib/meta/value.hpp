#pragma once

#include <expected>
#include <functional>
#include <glm/glm.hpp>
#include <magic_enum.hpp>
#include <string>
#include <string_view>
#include <variant>

#include "color.hpp"
#include "core/types.hpp"
#include "errors.hpp"
#include "gameio.hpp"
#include "jsonlib.hpp"
#include "objlib/transform.hpp"

template <> struct std::formatter<glm::vec3> : std::formatter<string_view> {
    template <typename FormatContext> auto format(const glm::vec3 &obj, FormatContext &ctx) const {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(x: {}, y: {}, z: {})", obj.x, obj.y, obj.z);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

template <> struct std::formatter<glm::vec4> : std::formatter<string_view> {
    template <typename FormatContext> auto format(const glm::vec4 &obj, FormatContext &ctx) const {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(x: {}, y: {}, z: {}, w: {})", obj.x, obj.y,
                       obj.z, obj.w);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

namespace Toolbox::Object {

    enum class MetaType : u8 {
        BOOL,
        S8,
        U8,
        S16,
        U16,
        S32,
        U32,
        F32,
        F64,
        STRING,
        VEC3,
        TRANSFORM,
        MTX34,
        RGB,
        RGBA,
        UNKNOWN,
    };

    template <typename T> struct map_to_type_enum {
        static constexpr MetaType value = MetaType::UNKNOWN;
    };

    template <> struct map_to_type_enum<bool> {
        static constexpr MetaType value = MetaType::BOOL;
    };

    template <> struct map_to_type_enum<s8> {
        static constexpr MetaType value = MetaType::S8;
    };

    template <> struct map_to_type_enum<u8> {
        static constexpr MetaType value = MetaType::U8;
    };

    template <> struct map_to_type_enum<s16> {
        static constexpr MetaType value = MetaType::S16;
    };

    template <> struct map_to_type_enum<u16> {
        static constexpr MetaType value = MetaType::U16;
    };

    template <> struct map_to_type_enum<s32> {
        static constexpr MetaType value = MetaType::S32;
    };

    template <> struct map_to_type_enum<u32> {
        static constexpr MetaType value = MetaType::U32;
    };

    template <> struct map_to_type_enum<f32> {
        static constexpr MetaType value = MetaType::F32;
    };

    template <> struct map_to_type_enum<f64> {
        static constexpr MetaType value = MetaType::F64;
    };

    template <> struct map_to_type_enum<std::string> {
        static constexpr MetaType value = MetaType::STRING;
    };

    template <> struct map_to_type_enum<glm::vec3> {
        static constexpr MetaType value = MetaType::VEC3;
    };

    template <> struct map_to_type_enum<Transform> {
        static constexpr MetaType value = MetaType::TRANSFORM;
    };

    template <> struct map_to_type_enum<glm::mat3x4> {
        static constexpr MetaType value = MetaType::MTX34;
    };

    template <> struct map_to_type_enum<Color::RGB24> {
        static constexpr MetaType value = MetaType::RGB;
    };

    template <> struct map_to_type_enum<Color::RGBA32> {
        static constexpr MetaType value = MetaType::RGBA;
    };

    template <typename T, bool comment = false>
    static constexpr MetaType template_type_v = map_to_type_enum<T>::value;

    template <MetaType T> struct meta_type_info {
        static constexpr std::string_view name = "bytes";
        static constexpr size_t size           = 0;
        static constexpr size_t alignment      = alignof(char);
    };

    template <> struct meta_type_info<MetaType::BOOL> {
        static constexpr std::string_view name = "bool";
        static constexpr size_t size           = sizeof(bool);
        static constexpr size_t alignment      = alignof(bool);
    };

    template <> struct meta_type_info<MetaType::S8> {
        static constexpr std::string_view name = "s8";
        static constexpr size_t size           = sizeof(s8);
        static constexpr size_t alignment      = alignof(s8);
    };

    template <> struct meta_type_info<MetaType::U8> {
        static constexpr std::string_view name = "u8";
        static constexpr size_t size           = sizeof(u8);
        static constexpr size_t alignment      = alignof(u8);
    };

    template <> struct meta_type_info<MetaType::S16> {
        static constexpr std::string_view name = "s16";
        static constexpr size_t size           = sizeof(s16);
        static constexpr size_t alignment      = alignof(s16);
    };

    template <> struct meta_type_info<MetaType::U16> {
        static constexpr std::string_view name = "u16";
        static constexpr size_t size           = sizeof(u16);
        static constexpr size_t alignment      = alignof(u16);
    };

    template <> struct meta_type_info<MetaType::S32> {
        static constexpr std::string_view name = "s32";
        static constexpr size_t size           = sizeof(s32);
        static constexpr size_t alignment      = alignof(s32);
    };

    template <> struct meta_type_info<MetaType::U32> {
        static constexpr std::string_view name = "u32";
        static constexpr size_t size           = sizeof(u32);
        static constexpr size_t alignment      = alignof(u32);
    };

    template <> struct meta_type_info<MetaType::F32> {
        static constexpr std::string_view name = "f32";
        static constexpr size_t size           = sizeof(f32);
        static constexpr size_t alignment      = alignof(f32);
    };

    template <> struct meta_type_info<MetaType::F64> {
        static constexpr std::string_view name = "f64";
        static constexpr size_t size           = sizeof(f64);
        static constexpr size_t alignment      = alignof(f64);
    };

    template <> struct meta_type_info<MetaType::STRING> {
        static constexpr std::string_view name = "string";
        static constexpr size_t size           = 2;
        static constexpr size_t alignment      = 4;
    };

    template <> struct meta_type_info<MetaType::VEC3> {
        static constexpr std::string_view name = "vec3";
        static constexpr size_t size           = 12;
        static constexpr size_t alignment      = 4;
    };

    template <> struct meta_type_info<MetaType::TRANSFORM> {
        static constexpr std::string_view name = "transform";
        static constexpr size_t size           = 36;
        static constexpr size_t alignment      = 4;
    };

    template <> struct meta_type_info<MetaType::MTX34> {
        static constexpr std::string_view name = "mtx34";
        static constexpr size_t size           = 48;
        static constexpr size_t alignment      = 4;
    };

    template <> struct meta_type_info<MetaType::RGB> {
        static constexpr std::string_view name = "rgb";
        static constexpr size_t size           = 3;
        static constexpr size_t alignment      = 1;
    };

    template <> struct meta_type_info<MetaType::RGBA> {
        static constexpr std::string_view name = "rgba";
        static constexpr size_t size           = 4;
        static constexpr size_t alignment      = 1;
    };

    constexpr std::string_view meta_type_name(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return meta_type_info<MetaType::BOOL>::name;
        case MetaType::S8:
            return meta_type_info<MetaType::S8>::name;
        case MetaType::U8:
            return meta_type_info<MetaType::U8>::name;
        case MetaType::S16:
            return meta_type_info<MetaType::S16>::name;
        case MetaType::U16:
            return meta_type_info<MetaType::U16>::name;
        case MetaType::S32:
            return meta_type_info<MetaType::S32>::name;
        case MetaType::U32:
            return meta_type_info<MetaType::U32>::name;
        case MetaType::F32:
            return meta_type_info<MetaType::F32>::name;
        case MetaType::F64:
            return meta_type_info<MetaType::F64>::name;
        case MetaType::STRING:
            return meta_type_info<MetaType::STRING>::name;
        case MetaType::VEC3:
            return meta_type_info<MetaType::VEC3>::name;
        case MetaType::TRANSFORM:
            return meta_type_info<MetaType::TRANSFORM>::name;
        case MetaType::MTX34:
            return meta_type_info<MetaType::MTX34>::name;
        case MetaType::RGB:
            return meta_type_info<MetaType::RGB>::name;
        case MetaType::RGBA:
            return meta_type_info<MetaType::RGBA>::name;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::name;
        }
    }

    constexpr size_t meta_type_size(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return meta_type_info<MetaType::BOOL>::size;
        case MetaType::S8:
            return meta_type_info<MetaType::S8>::size;
        case MetaType::U8:
            return meta_type_info<MetaType::U8>::size;
        case MetaType::S16:
            return meta_type_info<MetaType::S16>::size;
        case MetaType::U16:
            return meta_type_info<MetaType::U16>::size;
        case MetaType::S32:
            return meta_type_info<MetaType::S32>::size;
        case MetaType::U32:
            return meta_type_info<MetaType::U32>::size;
        case MetaType::F32:
            return meta_type_info<MetaType::F32>::size;
        case MetaType::F64:
            return meta_type_info<MetaType::F64>::size;
        case MetaType::STRING:
            return meta_type_info<MetaType::STRING>::size;
        case MetaType::VEC3:
            return meta_type_info<MetaType::VEC3>::size;
        case MetaType::TRANSFORM:
            return meta_type_info<MetaType::TRANSFORM>::size;
        case MetaType::MTX34:
            return meta_type_info<MetaType::MTX34>::size;
        case MetaType::RGB:
            return meta_type_info<MetaType::RGB>::size;
        case MetaType::RGBA:
            return meta_type_info<MetaType::RGBA>::size;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::size;
        }
    }

    constexpr size_t meta_type_alignment(MetaType type) {
        switch (type) {
        case MetaType::BOOL:
            return meta_type_info<MetaType::BOOL>::alignment;
        case MetaType::S8:
            return meta_type_info<MetaType::S8>::alignment;
        case MetaType::U8:
            return meta_type_info<MetaType::U8>::alignment;
        case MetaType::S16:
            return meta_type_info<MetaType::S16>::alignment;
        case MetaType::U16:
            return meta_type_info<MetaType::U16>::alignment;
        case MetaType::S32:
            return meta_type_info<MetaType::S32>::alignment;
        case MetaType::U32:
            return meta_type_info<MetaType::U32>::alignment;
        case MetaType::F32:
            return meta_type_info<MetaType::F32>::alignment;
        case MetaType::F64:
            return meta_type_info<MetaType::F64>::alignment;
        case MetaType::STRING:
            return meta_type_info<MetaType::STRING>::alignment;
        case MetaType::TRANSFORM:
            return meta_type_info<MetaType::TRANSFORM>::alignment;
        case MetaType::MTX34:
            return meta_type_info<MetaType::MTX34>::alignment;
        case MetaType::RGB:
            return meta_type_info<MetaType::RGB>::alignment;
        case MetaType::RGBA:
            return meta_type_info<MetaType::RGBA>::alignment;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::alignment;
        }
    }

    class MetaValue : public IGameSerializable {
    public:
        MetaValue() {
            m_value_buf.initTo(0);
            m_type = MetaType::UNKNOWN;
        }

        template <typename T>
        explicit MetaValue(T value, T v_min = std::numeric_limits<T>::min(),
                           T v_max = std::numeric_limits<T>::max())
            : m_value_buf() {
            m_type = map_to_type_enum<T>::value;

            m_min_buf = Buffer();
            m_min_buf.setBuf(&m_uint_min, sizeof(m_uint_min));
            m_max_buf = Buffer();
            m_max_buf.setBuf(&m_uint_max, sizeof(m_uint_max));
            restoreMinMax();

            set<T>(value);
            setMin<T>(v_min);
            setMax<T>(v_max);
        }

        explicit MetaValue(MetaType type) : m_type(type), m_value_buf() {
            m_min_buf = Buffer();
            m_min_buf.setBuf(&m_uint_min, sizeof(m_uint_min));
            m_max_buf = Buffer();
            m_max_buf.setBuf(&m_uint_max, sizeof(m_uint_max));

            m_value_buf.alloc(static_cast<uint32_t>(meta_type_size(type)));
            m_value_buf.initTo(0);
            switch (type) {
            case MetaType::TRANSFORM:
                m_value_buf.set<Transform>(0, Transform());
                break;
            default:
                break;
            }

            restoreMinMax();
        }

        MetaValue(MetaType type, Buffer &&value_buf)
            : m_type(type), m_value_buf(std::move(value_buf)) {
            m_min_buf = Buffer();
            m_min_buf.setBuf(&m_uint_min, sizeof(m_uint_min));
            m_max_buf = Buffer();
            m_max_buf.setBuf(&m_uint_max, sizeof(m_uint_max));
            restoreMinMax();
        }

        MetaValue(MetaType type, const Buffer &value_buf) : m_type(type), m_value_buf(value_buf) {
            m_min_buf = Buffer();
            m_min_buf.setBuf(&m_uint_min, sizeof(m_uint_min));
            m_max_buf = Buffer();
            m_max_buf.setBuf(&m_uint_max, sizeof(m_uint_max));
            restoreMinMax();
        }

        MetaValue(const MetaValue &other) {
            m_type      = other.m_type;
            m_value_buf = other.m_value_buf;
            m_min_buf   = Buffer();
            m_min_buf.setBuf(&m_uint_min, sizeof(m_uint_min));
            m_max_buf = Buffer();
            m_max_buf.setBuf(&m_uint_max, sizeof(m_uint_max));
            m_uint_min = other.m_uint_min;
            m_uint_max = other.m_uint_max;
        }

        MetaValue(MetaValue &&other) noexcept {
            m_type      = other.m_type;
            m_value_buf = std::move(other.m_value_buf);
            m_min_buf   = Buffer();
            m_min_buf.setBuf(&m_uint_min, sizeof(m_uint_min));
            m_max_buf = Buffer();
            m_max_buf.setBuf(&m_uint_max, sizeof(m_uint_max));
            m_uint_min = other.m_uint_min;
            m_uint_max = other.m_uint_max;
        }

        ~MetaValue() override { m_value_buf.free(); }

        MetaValue &operator=(const MetaValue &other) = default;
        MetaValue &operator=(MetaValue &&other)      = default;
        template <typename T> MetaValue &operator=(T &&value) {
            set<T>(value);
            return *this;
        }
        template <typename T> MetaValue &operator=(const T &value) {
            set<T>(value);
            return *this;
        }

        [[nodiscard]] MetaType type() const { return m_type; }

        [[nodiscard]] size_t computeSize() const;

        [[nodiscard]] const Buffer &buf() const { return m_value_buf; }

        template <typename T> [[nodiscard]] Result<T, std::string> min() const {
            return std::unexpected("Unsupported type for min()");
        }

        template <typename T> [[nodiscard]] Result<T, std::string> max() const {
            return std::unexpected("Unsupported type for max()");
        }

        template <typename T> [[nodiscard]] Result<T, std::string> get() const {
            return getBuf<T>(m_value_buf);
        }

        template <typename T> inline [[nodiscard]] bool setMin(T value) { return false; }

        template <typename T> inline [[nodiscard]] bool setMax(T value) { return false; }

        void restoreMinMax();

        template <typename T> bool set(const T &value) {
            m_type = map_to_type_enum<T>::value;
            if constexpr (!std::is_integral_v<T> && !std::is_floating_point_v<T>) {
                return setBuf<T>(m_value_buf, value);
            } else {
                Result<T, std::string> maybe_min = this->min<T>();
                Result<T, std::string> maybe_max = this->max<T>();
                if (!maybe_min && !maybe_max) {
                    return setBuf<T>(m_value_buf, value);
                }
                T clamped_value = value;
                if (maybe_min) {
                    clamped_value = std::max<T>(clamped_value, maybe_min.value());
                }
                if (maybe_max) {
                    clamped_value = std::min<T>(clamped_value, maybe_max.value());
                }
                return setBuf<T>(m_value_buf, value);
            }
        }

        // NOTE: This does not change the underlying type, it only attempts
        // to assign the variant value as the existing type, or it returns false.
        bool setVariant(const std::any &variant);

        Result<void, JSONError> loadJSON(const nlohmann::json &json_value);

        [[nodiscard]] std::string toString(int radix = 10) const;

        bool operator==(const MetaValue &other) const;

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, SerialError> gameSerialize(Serializer &out) const override;
        Result<void, SerialError> gameDeserialize(Deserializer &in) override;

    private:
        Buffer m_value_buf;
        Buffer m_min_buf;
        Buffer m_max_buf;
        MetaType m_type = MetaType::UNKNOWN;

        union {
            int64_t m_sint_min;
            uint64_t m_uint_min;
            float m_float_min;
            double m_double_min;
        };

        union {
            int64_t m_sint_max;
            uint64_t m_uint_max;
            float m_float_max;
            double m_double_max;
        };
    };

    template <typename T>
    [[nodiscard]]
    inline Result<T, std::string> getBuf(const Buffer &m_value_buf) {
        T value{};
        if (Deserializer::BytesToObject(m_value_buf, value, 0).has_value()) {
            return value;
        } else {
            return std::unexpected("Failed to deserialize data into higher type");
        }
    }

    template <>
    [[nodiscard]]
    inline Result<std::string, std::string> getBuf(const Buffer &m_value_buf) {
        std::string out;
        for (size_t i = 0; i < m_value_buf.size(); ++i) {
            char ch = m_value_buf.get<char>(static_cast<uint32_t>(i));
            if (ch == '\0')
                break;

            out.push_back(ch);
        }
        return out;
    }

    template <typename T>
    [[nodiscard]]
    inline bool setBuf(Buffer &m_value_buf, const T &value) {
        bool ret = Serializer::ObjectToBytes(value, m_value_buf, 0).has_value();
        return ret;
    }

    template <>
    [[nodiscard]]
    inline bool setBuf(Buffer &m_value_buf, const std::string &value) {
        m_value_buf.resize(static_cast<uint32_t>(value.size() + 1));
        for (size_t i = 0; i < value.size(); ++i) {
            m_value_buf.set<char>(static_cast<uint32_t>(i), value[i]);
        }
        m_value_buf.set<char>(static_cast<uint32_t>(value.size()), '\0');
        return true;
    }

    template <>
    [[nodiscard]]
    inline bool setBuf(Buffer &m_value_buf, const Buffer &value) {
        value.copyTo(m_value_buf);
        return true;
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value, bool value,
                                                MetaType type) {
        try {
            switch (type) {
            case MetaType::BOOL:
                return meta_value->set<bool>(value);
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value, s64 value,
                                                MetaType type) {
        try {
            switch (type) {
            case MetaType::S8:
                return meta_value->set(static_cast<s8>(value));
            case MetaType::U8:
                return meta_value->set(static_cast<u8>(value));
            case MetaType::S16:
                return meta_value->set(static_cast<s16>(value));
            case MetaType::U16:
                return meta_value->set(static_cast<u16>(value));
            case MetaType::S32:
                return meta_value->set(static_cast<s32>(value));
            case MetaType::U32:
                return meta_value->set(static_cast<u32>(value));
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value, f64 value,
                                                MetaType type) {
        try {
            switch (type) {
            case MetaType::F32:
                return meta_value->set(static_cast<f32>(value));
            case MetaType::F64:
                return meta_value->set(static_cast<f64>(value));
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value,
                                                const std::string &value, MetaType type) {
        try {
            switch (type) {
            case MetaType::STRING:
                return meta_value->set<std::string>(value);
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value,
                                                std::string_view value, MetaType type) {
        try {
            switch (type) {
            case MetaType::STRING:
                return meta_value->set(std::string(value));
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value, glm::vec3 value,
                                                MetaType type) {
        try {
            switch (type) {
            case MetaType::VEC3:
                return meta_value->set<glm::vec3>(value);
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value,
                                                const Transform &value, MetaType type) {
        try {
            switch (type) {
            case MetaType::TRANSFORM:
                return meta_value->set<Transform>(value);
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value,
                                                const glm::mat3x4 &value, MetaType type) {
        try {
            switch (type) {
            case MetaType::MTX34:
                return meta_value->set<glm::mat3x4>(value);
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value,
                                                const Color::RGB24 &value, MetaType type) {
        try {
            switch (type) {
            case MetaType::RGB:
                return meta_value->set<Color::RGB24>(value);
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    inline Result<bool, MetaError> setMetaValue(RefPtr<MetaValue> meta_value,
                                                const Color::RGBA32 &value, MetaType type) {
        try {
            switch (type) {
            case MetaType::RGBA:
                return meta_value->set<Color::RGBA32>(value);
            default:
                return false;
            }
        } catch (std::exception &e) {
            return make_meta_error<bool>(e.what(), "T", magic_enum::enum_name(type));
        }
    }

    template <> inline Result<s8, std::string> MetaValue::min<s8>() const {
        return static_cast<s8>(m_sint_min);
    }

    template <> inline Result<u8, std::string> MetaValue::min<u8>() const {
        return static_cast<u8>(m_uint_min);
    }

    template <> inline Result<s16, std::string> MetaValue::min<s16>() const {
        return static_cast<s16>(m_sint_min);
    }

    template <> inline Result<u16, std::string> MetaValue::min<u16>() const {
        return static_cast<u16>(m_uint_min);
    }

    template <> inline Result<s32, std::string> MetaValue::min<s32>() const {
        return static_cast<s32>(m_sint_min);
    }

    template <> inline Result<u32, std::string> MetaValue::min<u32>() const {
        return static_cast<u32>(m_uint_min);
    }

    template <> inline Result<s64, std::string> MetaValue::min<s64>() const {
        return static_cast<s64>(m_uint_min);
    }

    template <> inline Result<u64, std::string> MetaValue::min<u64>() const {
        return static_cast<u64>(m_uint_min);
    }

    template <> inline Result<float, std::string> MetaValue::min<float>() const {
        return static_cast<float>(m_float_min);
    }

    template <> inline Result<double, std::string> MetaValue::min<double>() const {
        return static_cast<double>(m_double_min);
    }

    template <> inline Result<s8, std::string> MetaValue::max<s8>() const {
        return static_cast<s8>(m_sint_max);
    }

    template <> inline Result<u8, std::string> MetaValue::max<u8>() const {
        return static_cast<u8>(m_uint_max);
    }

    template <> inline Result<s16, std::string> MetaValue::max<s16>() const {
        return static_cast<s16>(m_sint_max);
    }

    template <> inline Result<u16, std::string> MetaValue::max<u16>() const {
        return static_cast<u16>(m_uint_max);
    }

    template <> inline Result<s32, std::string> MetaValue::max<s32>() const {
        return static_cast<s32>(m_sint_max);
    }

    template <> inline Result<u32, std::string> MetaValue::max<u32>() const {
        return static_cast<u32>(m_uint_max);
    }

    template <> inline Result<s64, std::string> MetaValue::max<s64>() const {
        return static_cast<s64>(m_uint_max);
    }

    template <> inline Result<u64, std::string> MetaValue::max<u64>() const {
        return static_cast<u64>(m_uint_max);
    }

    template <> inline Result<float, std::string> MetaValue::max<float>() const {
        return static_cast<float>(m_float_max);
    }

    template <> inline Result<double, std::string> MetaValue::max<double>() const {
        return static_cast<double>(m_double_max);
    }

    template <> inline bool MetaValue::setMin<s8>(s8 min) {
        return m_sint_min = static_cast<s64>(min);
    }

    template <> inline bool MetaValue::setMin<u8>(u8 min) {
        return m_uint_min = static_cast<u64>(min);
    }

    template <> inline bool MetaValue::setMin<s16>(s16 min) {
        return m_sint_min = static_cast<s64>(min);
    }

    template <> inline bool MetaValue::setMin<u16>(u16 min) {
        return m_uint_min = static_cast<u64>(min);
    }

    template <> inline bool MetaValue::setMin<s32>(s32 min) {
        return m_sint_min = static_cast<s64>(min);
    }

    template <> inline bool MetaValue::setMin<u32>(u32 min) {
        return m_uint_min = static_cast<u64>(min);
    }

    template <> inline bool MetaValue::setMin<s64>(s64 min) {
        return m_sint_min = static_cast<s64>(min);
    }

    template <> inline bool MetaValue::setMin<u64>(u64 min) {
        return m_uint_min = static_cast<u64>(min);
    }

    template <> inline bool MetaValue::setMin<float>(float min) {
        return m_float_min = static_cast<float>(min);
    }

    template <> inline bool MetaValue::setMin<double>(double min) {
        return m_double_min = static_cast<double>(min);
    }

    template <> inline bool MetaValue::setMax<s8>(s8 max) {
        return m_sint_max = static_cast<s64>(max);
    }

    template <> inline bool MetaValue::setMax<u8>(u8 max) {
        return m_uint_max = static_cast<u64>(max);
    }

    template <> inline bool MetaValue::setMax<s16>(s16 max) {
        return m_sint_max = static_cast<s64>(max);
    }

    template <> inline bool MetaValue::setMax<u16>(u16 max) {
        return m_uint_max = static_cast<u64>(max);
    }

    template <> inline bool MetaValue::setMax<s32>(s32 max) {
        return m_sint_max = static_cast<s64>(max);
    }

    template <> inline bool MetaValue::setMax<u32>(u32 max) {
        return m_uint_max = static_cast<u64>(max);
    }

    template <> inline bool MetaValue::setMax<s64>(s64 max) {
        return m_sint_max = static_cast<s64>(max);
    }

    template <> inline bool MetaValue::setMax<u64>(u64 max) {
        return m_uint_max = static_cast<u64>(max);
    }

    template <> inline bool MetaValue::setMax<float>(float max) {
        return m_float_max = static_cast<float>(max);
    }

    template <> inline bool MetaValue::setMax<double>(double max) {
        return m_double_max = static_cast<double>(max);
    }

}  // namespace Toolbox::Object