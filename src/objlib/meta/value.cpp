#include "objlib/meta/value.hpp"
#include "color.hpp"
#include "objlib/transform.hpp"
#include "strutil.hpp"
#include <format>
#include <glm/glm.hpp>
#include <string>

namespace Toolbox::Object {

    size_t MetaValue::computeSize() const {
        switch (m_type) {
        case MetaType::BOOL:
        case MetaType::S8:
        case MetaType::U8:
            return 1;
        case MetaType::S16:
        case MetaType::U16:
            return 2;
        case MetaType::S32:
        case MetaType::U32:
            return 4;
        case MetaType::F32:
            return 4;
        case MetaType::F64:
            return 8;
        case MetaType::STRING:
            return strnlen(m_value_buf.buf<const char>(), m_value_buf.size());
        case MetaType::VEC3:
            return 12;
        case MetaType::TRANSFORM:
            return 36;  // 3 for translation, 3 for rotation, 3 for scale
        case MetaType::MTX34:
            return 48;
        case MetaType::RGB:
            return 3;  // RGB is 3 bytes
        case MetaType::RGBA:
            return 4;  // RGBA is 4 bytes
        default:
            /*TOOLBOX_WARN("[MetaValue] Unknown type for size computation: {}",
                         magic_enum::enum_name(m_type));*/
            return m_value_buf.size();  // Unknown type or no value
        }
    }

    bool MetaValue::setVariant(const std::any &variant) {
        try {
            switch (type()) {
            case MetaType::BOOL:
                return set<bool>(std::any_cast<bool>(variant));
            case MetaType::S8:
                return set<s8>(std::any_cast<s8>(variant));
            case MetaType::U8:
                return set<u8>(std::any_cast<u8>(variant));
            case MetaType::S16:
                return set<s16>(std::any_cast<s16>(variant));
            case MetaType::U16:
                return set<u16>(std::any_cast<u16>(variant));
            case MetaType::S32:
                return set<s32>(std::any_cast<s32>(variant));
            case MetaType::U32:
                return set<u32>(std::any_cast<u32>(variant));
            case MetaType::F32:
                return set<f32>(std::any_cast<f32>(variant));
            case MetaType::F64:
                return set<f64>(std::any_cast<f64>(variant));
            case MetaType::STRING:
                return set<std::string>(std::any_cast<std::string>(variant));
            case MetaType::VEC3:
                return set<glm::vec3>(std::any_cast<glm::vec3>(variant));
            case MetaType::TRANSFORM:
                return set<Transform>(std::any_cast<Transform>(variant));
            case MetaType::MTX34:
                return set<glm::mat3x4>(std::any_cast<glm::mat3x4>(variant));
            case MetaType::RGB:
                return set<Color::RGB24>(std::any_cast<Color::RGB24>(variant));
            case MetaType::RGBA:
                return set<Color::RGBA32>(std::any_cast<Color::RGBA32>(variant));
            case MetaType::UNKNOWN:
                return set<Buffer>(std::any_cast<Buffer>(variant));
            }
        } catch (const std::bad_any_cast &e) {
            TOOLBOX_DEBUG_LOG_V("[META_VALUE] Bad variant setter: {}", e.what());
            return false;
        }
    }

    Result<void, JSONError> MetaValue::loadJSON(const nlohmann::json &json_value) {
        return tryJSON(json_value, [this](const json &j) {
            switch (m_type) {
            case MetaType::BOOL:
                set(j.get<bool>());
                break;
            case MetaType::S8:
                set(j.get<s8>());
                break;
            case MetaType::U8:
                set(j.get<u8>());
                break;
            case MetaType::S16:
                set(j.get<s16>());
                break;
            case MetaType::U16:
                set(j.get<u16>());
                break;
            case MetaType::S32:
                set(j.get<s32>());
                break;
            case MetaType::U32:
                set(j.get<u32>());
                break;
            case MetaType::F32:
                set(j.get<f32>());
                break;
            case MetaType::F64:
                set(j.get<f64>());
                break;
            case MetaType::STRING: {
                set(j.get<std::string>());
                break;
            }
            case MetaType::VEC3: {
                glm::vec3 new_value;
                new_value.x = j[0].get<f32>();
                new_value.y = j[1].get<f32>();
                new_value.z = j[2].get<f32>();
                set(new_value);
                break;
            }
            case MetaType::TRANSFORM: {
                Transform new_value;
                new_value.m_translation.x = j[0][0].get<f32>();
                new_value.m_translation.y = j[0][1].get<f32>();
                new_value.m_translation.z = j[0][2].get<f32>();
                new_value.m_rotation.x    = j[1][0].get<f32>();
                new_value.m_rotation.y    = j[1][1].get<f32>();
                new_value.m_rotation.z    = j[1][2].get<f32>();
                new_value.m_scale.x       = j[2][0].get<f32>();
                new_value.m_scale.y       = j[2][1].get<f32>();
                new_value.m_scale.z       = j[2][2].get<f32>();
                set(new_value);
                break;
            }
            case MetaType::RGB: {
                Color::RGB24 new_value(j[0].get<u8>(), j[1].get<u8>(), j[2].get<u8>());
                set(new_value);
                break;
            }
            case MetaType::RGBA: {
                Color::RGBA32 new_value(j[0].get<u8>(), j[1].get<u8>(), j[2].get<u8>(),
                                        j[3].get<u8>());
                set(new_value);
                break;
            }
            default:
                break;
            }
        });
    }

    std::string MetaValue::toString(int radix) const {
        switch (m_type) {
        case MetaType::BOOL:
            return std::format("{}", get<bool>().value());
        case MetaType::S8:
            if (radix == 2) {
                return std::format("0b{:b}", get<s8>().value());
            } else if (radix == 8) {
                return std::format("0o{:o}", get<s8>().value());
            } else if (radix == 10) {
                return std::format("{}", get<s8>().value());
            } else {
                return std::format("0x{:X}", get<s8>().value());
            }
        case MetaType::U8:
            if (radix == 2) {
                return std::format("0b{:b}", get<u8>().value());
            } else if (radix == 8) {
                return std::format("0o{:o}", get<u8>().value());
            } else if (radix == 10) {
                return std::format("{}", get<u8>().value());
            } else {
                return std::format("0x{:X}", get<u8>().value());
            }
        case MetaType::S16:
            if (radix == 2) {
                return std::format("0b{:b}", get<s16>().value());
            } else if (radix == 8) {
                return std::format("0o{:o}", get<s16>().value());
            } else if (radix == 10) {
                return std::format("{}", get<s16>().value());
            } else {
                return std::format("0x{:X}", get<s16>().value());
            }
        case MetaType::U16:
            if (radix == 2) {
                return std::format("0b{:b}", get<u16>().value());
            } else if (radix == 8) {
                return std::format("0o{:o}", get<u16>().value());
            } else if (radix == 10) {
                return std::format("{}", get<u16>().value());
            } else {
                return std::format("0x{:X}", get<u16>().value());
            }
        case MetaType::S32:
            if (radix == 2) {
                return std::format("0b{:b}", get<s32>().value());
            } else if (radix == 8) {
                return std::format("0o{:o}", get<s32>().value());
            } else if (radix == 10) {
                return std::format("{}", get<s32>().value());
            } else {
                return std::format("0x{:X}", get<s32>().value());
            }
        case MetaType::U32:
            if (radix == 2) {
                return std::format("0b{:b}", get<u32>().value());
            } else if (radix == 8) {
                return std::format("0o{:o}", get<u32>().value());
            } else if (radix == 10) {
                return std::format("{}", get<u32>().value());
            } else {
                return std::format("0x{:X}", get<u32>().value());
            }
        case MetaType::F32:
            return std::format("{}", get<f32>().value());
        case MetaType::F64:
            return std::format("{}", get<f64>().value());
        case MetaType::STRING:
            return std::format("{}", get<std::string>().value());
        case MetaType::VEC3:
            return std::format("{}", get<glm::vec3>().value());
        case MetaType::TRANSFORM:
            return std::format("{}", get<Transform>().value());
        case MetaType::RGB:
            return std::format("{}", get<Color::RGB24>().value());
        case MetaType::RGBA:
            return std::format("{}", get<Color::RGBA32>().value());
        case MetaType::UNKNOWN: {
            const char *byte_buf = buf().buf<char>();
            size_t byte_len      = computeSize();

            std::string out_str;
            out_str.reserve(byte_len * 4);

            size_t i = 0;
            for (i = 0; i < byte_len; ++i) {
                uint8_t ch = ((uint8_t *)byte_buf)[i];
                out_str.append(std::format("{:02X} ", ch));
            }

            if (i > 0) {
                out_str.pop_back();
            }
            return out_str;
        }
        default:
            return "null";
        }
    }

    bool MetaValue::operator==(const MetaValue &other) const {
        return m_type == other.m_type && m_value_buf == other.m_value_buf;
    }

    Result<void, SerialError> MetaValue::serialize(Serializer &out) const {
        out.write<u8>(static_cast<u8>(m_type));
        try {
            switch (m_type) {
            case MetaType::BOOL:
                out.write(get<bool>().value());
                break;
            case MetaType::S8:
                out.write(get<s8>().value());
                break;
            case MetaType::U8:
                out.write(get<u8>().value());
                break;
            case MetaType::S16:
                out.write<s16, std::endian::big>(get<s16>().value());
                break;
            case MetaType::U16:
                out.write<u16, std::endian::big>(get<u16>().value());
                break;
            case MetaType::S32:
                out.write<s32, std::endian::big>(get<s32>().value());
                break;
            case MetaType::U32:
                out.write<u32, std::endian::big>(get<u32>().value());
                break;
            case MetaType::F32:
                out.write<f32, std::endian::big>(get<f32>().value());
                break;
            case MetaType::F64:
                out.write<f64, std::endian::big>(get<f64>().value());
                break;
            case MetaType::STRING: {
                auto str_result = String::toGameEncoding(get<std::string>().value());
                if (!str_result) {
                    return make_serial_error<void>(out, str_result.error().m_message[0]);
                }
                out.writeString<std::endian::big>(str_result.value());
                break;
            }
            case MetaType::VEC3: {
                auto vec = get<glm::vec3>().value();
                out.write<f32, std::endian::big>(vec.x);
                out.write<f32, std::endian::big>(vec.y);
                out.write<f32, std::endian::big>(vec.z);
                break;
            }
            case MetaType::TRANSFORM: {
                auto transform = get<Transform>().value();
                out.write<f32, std::endian::big>(transform.m_translation.x);
                out.write<f32, std::endian::big>(transform.m_translation.y);
                out.write<f32, std::endian::big>(transform.m_translation.z);
                out.write<f32, std::endian::big>(transform.m_rotation.x);
                out.write<f32, std::endian::big>(transform.m_rotation.y);
                out.write<f32, std::endian::big>(transform.m_rotation.z);
                out.write<f32, std::endian::big>(transform.m_scale.x);
                out.write<f32, std::endian::big>(transform.m_scale.y);
                out.write<f32, std::endian::big>(transform.m_scale.z);
                break;
            }
            case MetaType::RGB: {
                auto rgb = get<Color::RGB24>().value();
                rgb.serialize(out);
                break;
            }
            case MetaType::RGBA: {
                auto rgba = get<Color::RGBA32>().value();
                rgba.serialize(out);
                break;
            }
            default:
                break;
            }
            return {};
        } catch (std::exception &e) {
            return make_serial_error<void>(out, e.what());
        }
    }

    Result<void, SerialError> MetaValue::deserialize(Deserializer &in) {
        m_type = static_cast<MetaType>(in.read<u8>());
        try {
            switch (m_type) {
            case MetaType::BOOL:
                set(in.read<bool, std::endian::big>());
                break;
            case MetaType::S8:
                set(in.read<s8, std::endian::big>());
                break;
            case MetaType::U8:
                set(in.read<u8, std::endian::big>());
                break;
            case MetaType::S16:
                set(in.read<s16, std::endian::big>());
                break;
            case MetaType::U16:
                set(in.read<u16, std::endian::big>());
                break;
            case MetaType::S32:
                set(in.read<s32, std::endian::big>());
                break;
            case MetaType::U32:
                set(in.read<u32, std::endian::big>());
                break;
            case MetaType::F32:
                set(in.read<f32, std::endian::big>());
                break;
            case MetaType::F64:
                set(in.read<f64, std::endian::big>());
                break;
            case MetaType::STRING: {
                auto str_result = String::fromGameEncoding(in.readString<std::endian::big>());
                if (!str_result) {
                    return make_serial_error<void>(in, str_result.error().m_message[0]);
                }
                set(str_result.value());
                break;
            }
            case MetaType::VEC3:
                glm::vec3 vec;
                vec.x = in.read<f32, std::endian::big>();
                vec.y = in.read<f32, std::endian::big>();
                vec.z = in.read<f32, std::endian::big>();
                set(vec);
                break;
            case MetaType::TRANSFORM: {
                Transform transform;
                transform.m_translation.x = in.read<f32, std::endian::big>();
                transform.m_translation.y = in.read<f32, std::endian::big>();
                transform.m_translation.z = in.read<f32, std::endian::big>();
                transform.m_rotation.x    = in.read<f32, std::endian::big>();
                transform.m_rotation.y    = in.read<f32, std::endian::big>();
                transform.m_rotation.z    = in.read<f32, std::endian::big>();
                transform.m_scale.x       = in.read<f32, std::endian::big>();
                transform.m_scale.y       = in.read<f32, std::endian::big>();
                transform.m_scale.z       = in.read<f32, std::endian::big>();
                set(transform);
                break;
            }
            case MetaType::RGB: {
                Color::RGB24 rgb;
                rgb.deserialize(in);
                set(rgb);
                break;
            }
            case MetaType::RGBA: {
                Color::RGBA32 rgba;
                rgba.deserialize(in);
                set(rgba);
                break;
            }
            default:
                break;
            }
            return {};
        } catch (std::exception &e) {
            return make_serial_error<void>(in, e.what());
        }
    }

}  // namespace Toolbox::Object