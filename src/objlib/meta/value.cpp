#include "objlib/meta/value.hpp"
#include "color.hpp"
#include "objlib/transform.hpp"
#include <format>
#include <glm/glm.hpp>
#include <string>

namespace Toolbox::Object {

    std::expected<void, JSONError> MetaValue::loadJSON(const nlohmann::json &json_value) {
        return tryJSON(json_value, [this](const json &j) {
            switch (m_type) {
            case MetaType::BOOL:
                m_value = j.get<bool>();
                break;
            case MetaType::S8:
                m_value = j.get<s8>();
                break;
            case MetaType::U8:
                m_value = j.get<u8>();
                break;
            case MetaType::S16:
                m_value = j.get<s16>();
                break;
            case MetaType::U16:
                m_value = j.get<u16>();
                break;
            case MetaType::S32:
                m_value = j.get<s32>();
                break;
            case MetaType::U32:
                m_value = j.get<u32>();
                break;
            case MetaType::F32:
                m_value = j.get<f32>();
                break;
            case MetaType::F64:
                m_value = j.get<f64>();
                break;
            case MetaType::STRING:
                m_value = j.get<std::string>();
                break;
            case MetaType::VEC3: {
                glm::vec3 new_value;
                new_value.x = j[0].get<f32>();
                new_value.y = j[1].get<f32>();
                new_value.z = j[2].get<f32>();
                m_value     = new_value;
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
                m_value                   = new_value;
                break;
            }
            case MetaType::RGB: {
                Color::RGB24 new_value(j[0].get<u8>(), j[1].get<u8>(), j[2].get<u8>());
                m_value = new_value;
                break;
            }
            case MetaType::RGBA: {
                Color::RGBA32 new_value(j[0].get<u8>(), j[1].get<u8>(), j[2].get<u8>(),
                                        j[3].get<u8>());
                m_value = new_value;
                break;
            }
            }
        });
    }

    std::string MetaValue::toString() const {
        switch (m_type) {
        case MetaType::BOOL:
            return std::format("{}", std::get<bool>(m_value));
        case MetaType::S8:
            return std::format("{}", std::get<s8>(m_value));
        case MetaType::U8:
            return std::format("{}", std::get<u8>(m_value));
        case MetaType::S16:
            return std::format("{}", std::get<s16>(m_value));
        case MetaType::U16:
            return std::format("{}", std::get<u16>(m_value));
        case MetaType::S32:
            return std::format("{}", std::get<s32>(m_value));
        case MetaType::U32:
            return std::format("{}", std::get<u32>(m_value));
        case MetaType::F32:
            return std::format("{}", std::get<f32>(m_value));
        case MetaType::F64:
            return std::format("{}", std::get<f64>(m_value));
        case MetaType::STRING:
            return std::format("\"{}\"", std::get<std::string>(m_value));
        case MetaType::VEC3:
            return std::format("{}", std::get<glm::vec3>(m_value));
        case MetaType::TRANSFORM:
            return std::format("{}", std::get<Transform>(m_value));
        case MetaType::RGB:
            return std::format("{}", std::get<Color::RGB24>(m_value));
        case MetaType::RGBA:
            return std::format("{}", std::get<Color::RGBA32>(m_value));
        case MetaType::COMMENT:
            //return std::get<std::string>(m_value);
        default:
            return "null";
        }
    }

    bool MetaValue::operator==(const MetaValue &other) const {
        return m_type == other.m_type && m_value == other.m_value;
    }

    std::expected<void, SerialError> MetaValue::serialize(Serializer &out) const {
        try {
            switch (m_type) {
            case MetaType::BOOL:
                out.write(std::get<bool>(m_value));
                break;
            case MetaType::S8:
                out.write(std::get<s8>(m_value));
                break;
            case MetaType::U8:
                out.write(std::get<u8>(m_value));
                break;
            case MetaType::S16:
                out.write<s16, std::endian::big>(std::get<s16>(m_value));
                break;
            case MetaType::U16:
                out.write<u16, std::endian::big>(std::get<u16>(m_value));
                break;
            case MetaType::S32:
                out.write<s32, std::endian::big>(std::get<s32>(m_value));
                break;
            case MetaType::U32:
                out.write<u32, std::endian::big>(std::get<u32>(m_value));
                break;
            case MetaType::F32:
                out.write<f32, std::endian::big>(std::get<f32>(m_value));
                break;
            case MetaType::F64:
                out.write<f64, std::endian::big>(std::get<f64>(m_value));
                break;
            case MetaType::STRING:
                out.writeString<std::endian::big>(std::get<std::string>(m_value));
                break;
            case MetaType::VEC3: {
                auto vec = std::get<glm::vec3>(m_value);
                out.write<f32, std::endian::big>(vec.x);
                out.write<f32, std::endian::big>(vec.y);
                out.write<f32, std::endian::big>(vec.z);
                break;
            }
            case MetaType::TRANSFORM: {
                auto transform = std::get<Transform>(m_value);
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
                auto rgb = std::get<Color::RGB24>(m_value);
                rgb.serialize(out);
                break;
            }
            case MetaType::RGBA: {
                auto rgba = std::get<Color::RGBA32>(m_value);
                rgba.serialize(out);
                break;
            }
            case MetaType::COMMENT:
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
                m_value = in.read<bool, std::endian::big>();
                break;
            case MetaType::S8:
                m_value = in.read<s8, std::endian::big>();
                break;
            case MetaType::U8:
                m_value = in.read<u8, std::endian::big>();
                break;
            case MetaType::S16:
                m_value = in.read<s16, std::endian::big>();
                break;
            case MetaType::U16:
                m_value = in.read<u16, std::endian::big>();
                break;
            case MetaType::S32:
                m_value = in.read<s32, std::endian::big>();
                break;
            case MetaType::U32:
                m_value = in.read<u32, std::endian::big>();
                break;
            case MetaType::F32:
                m_value = in.read<f32, std::endian::big>();
                break;
            case MetaType::F64:
                m_value = in.read<f64, std::endian::big>();
                break;
            case MetaType::STRING:
                m_value = in.readString<std::endian::big>();
                break;
            case MetaType::VEC3:
                glm::vec3 vec;
                vec.x   = in.read<f32, std::endian::big>();
                vec.y   = in.read<f32, std::endian::big>();
                vec.z   = in.read<f32, std::endian::big>();
                m_value = vec;
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
                m_value                   = transform;
                break;
            }
            case MetaType::RGB: {
                Color::RGB24 rgb;
                rgb.deserialize(in);
                m_value = rgb;
                break;
            }
            case MetaType::RGBA: {
                Color::RGBA32 rgba;
                rgba.deserialize(in);
                m_value = rgba;
                break;
            }
            case MetaType::COMMENT:
            default:
                break;
            }
            return {};
        } catch (std::exception &e) {
            return make_serial_error<void>(in, e.what());
        }
    }

}  // namespace Toolbox::Object