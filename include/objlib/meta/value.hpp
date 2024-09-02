#pragma once

#include "color.hpp"
#include "core/types.hpp"
#include "errors.hpp"
#include "jsonlib.hpp"
#include "objlib/transform.hpp"
#include "serial.hpp"
#include <expected>
#include <functional>
#include <glm/glm.hpp>
#include <magic_enum.hpp>
#include <string>
#include <string_view>
#include <variant>

#include "core/memory.hpp"

template <> struct std::formatter<glm::vec3> : std::formatter<string_view> {
    template <typename FormatContext> auto format(const glm::vec3 &obj, FormatContext &ctx) const {
        std::string outstr;
        std::format_to(std::back_inserter(outstr), "(x: {}, y: {}, z: {})", obj.x, obj.y, obj.z);
        return std::formatter<string_view>::format(outstr, ctx);
    }
};

namespace Toolbox::Object {

    enum class MetaType {
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

    template <> struct map_to_type_enum<Color::RGB24> {
        static constexpr MetaType value = MetaType::RGB;
    };

    template <> struct map_to_type_enum<Color::RGBA32> {
        static constexpr MetaType value = MetaType::RGBA;
    };

    template <typename T, bool comment = false>
    static constexpr MetaType template_type_v = map_to_type_enum<T>::value;

    template <MetaType T> struct meta_type_info {
        static constexpr std::string_view name = "unknown";
        static constexpr size_t size           = 0;
        static constexpr size_t alignment      = 0;
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
        static constexpr size_t alignment      = 16;
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
        case MetaType::TRANSFORM:
            return meta_type_info<MetaType::TRANSFORM>::size;
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
        case MetaType::RGB:
            return meta_type_info<MetaType::RGB>::alignment;
        case MetaType::RGBA:
            return meta_type_info<MetaType::RGBA>::alignment;
        case MetaType::UNKNOWN:
        default:
            return meta_type_info<MetaType::UNKNOWN>::alignment;
        }
    }

    class MetaValue : public ISerializable {
    public:
        MetaValue() = delete;
        template <typename T> explicit MetaValue(T value) : m_value_buf() {
            m_value_buf.alloc(128);
            m_value_buf.initTo(0);
            set<T>(value);
        }
        explicit MetaValue(MetaType type) : m_type(type), m_value_buf() {
            m_value_buf.alloc(128);
            m_value_buf.initTo(0);
            switch (type) {
            case MetaType::TRANSFORM:
                m_value_buf.set<Transform>(0, Transform());
                break;
            default:
                break;
            }
        }
        MetaValue(const MetaValue &other) = default;
        MetaValue(MetaValue &&other)      = default;

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

        template <typename T> [[nodiscard]] Result<T, std::string> get() const {
            return getBuf<T>(m_type, m_value_buf);
        }

        template <typename T> bool set(const T &value) {
            return setBuf(m_type, m_value_buf, value);
        }

        Result<void, JSONError> loadJSON(const nlohmann::json &json_value);

        [[nodiscard]] std::string toString() const;

        bool operator==(const MetaValue &other) const;

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

    private:
        Buffer m_value_buf;
        MetaType m_type = MetaType::UNKNOWN;
    };

    template <typename T> [[nodiscard]]
    inline Result<T, std::string> getBuf(const MetaType &m_type, const Buffer &m_value_buf) {
        if (m_type != map_to_type_enum<T>::value)
            return std::unexpected("Type record mismatch");
        return m_value_buf.get<T>(0);
    }
    template <> [[nodiscard]]
    inline Result<std::string, std::string> getBuf(const MetaType &m_type,
                                                  const Buffer &m_value_buf) {
        std::string out;
        for (size_t i = 0; i < m_value_buf.size(); ++i) {
            char ch = m_value_buf.get<char>(i);
            if (ch == '\0')
                break;

            out.push_back(ch);
        }
        return out;
    }
    template <typename T> [[nodiscard]]
    inline bool setBuf(MetaType &m_type, Buffer &m_value_buf, const T &value) {
        m_type = map_to_type_enum<T>::value;
        m_value_buf.set<T>(0, value);
        return true;
    }
    template <> [[nodiscard]]
    inline bool setBuf(MetaType &m_type,
                       Buffer &m_value_buf,
                       const std::string &value) {
        m_type = MetaType::STRING;
        if (value.size() > m_value_buf.size() + 1) {
            m_value_buf.resize(static_cast<size_t>(value.size() * 1.5f));
        }
        m_value_buf.initTo('\0');
        for (size_t i = 0; i < value.size(); ++i) {
            m_value_buf.set<char>(i, value[i]);
        }
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

}  // namespace Toolbox::Object