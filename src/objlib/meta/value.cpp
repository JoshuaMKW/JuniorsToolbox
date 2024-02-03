#include "objlib/meta/value.hpp"
#include "color.hpp"
#include "objlib/transform.hpp"
#include "strutil.hpp"
#include <format>
#include <glm/glm.hpp>
#include <string>

namespace Toolbox::Object {

    std::expected<void, JSONError> MetaValue::loadJSON(const nlohmann::json &json_value) {
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

    std::string MetaValue::toString() const {
        switch (m_type) {
        case MetaType::BOOL:
            return std::format("{}", get<bool>().value());
        case MetaType::S8:
            return std::format("{}", get<s8>().value());
        case MetaType::U8:
            return std::format("{}", get<u8>().value());
        case MetaType::S16:
            return std::format("{}", get<s16>().value());
        case MetaType::U16:
            return std::format("{}", get<u16>().value());
        case MetaType::S32:
            return std::format("{}", get<s32>().value());
        case MetaType::U32:
            return std::format("{}", get<u32>().value());
        case MetaType::F32:
            return std::format("{}", get<f32>().value());
        case MetaType::F64:
            return std::format("{}", get<f64>().value());
        case MetaType::STRING:
            return std::format("\"{}\"", get<std::string>().value());
        case MetaType::VEC3:
            return std::format("{}", get<glm::vec3>().value());
        case MetaType::TRANSFORM:
            return std::format("{}", get<Transform>().value());
        case MetaType::RGB:
            return std::format("{}", get<Color::RGB24>().value());
        case MetaType::RGBA:
            return std::format("{}", get<Color::RGBA32>().value());
        default:
            return "null";
        }
    }

    bool MetaValue::operator==(const MetaValue &other) const {
        return m_type == other.m_type && m_value_buf == other.m_value_buf;
    }

    std::expected<void, SerialError> MetaValue::serialize(Serializer &out) const {
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

    std::expected<void, SerialError> MetaValue::deserialize(Deserializer &in) {
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
                vec.x   = in.read<f32, std::endian::big>();
                vec.y   = in.read<f32, std::endian::big>();
                vec.z   = in.read<f32, std::endian::big>();
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