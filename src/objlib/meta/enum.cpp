#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include <iostream>
#include <string>

namespace Toolbox::Object {

    std::expected<void, JSONError> MetaEnum::loadJSON(const nlohmann::json &json_value) {
        return tryJSON(json_value, [this](const json &j) {
            switch (m_type) {
            case MetaType::S8:
                m_cur_value = j.get<s8>();
                break;
            case MetaType::U8:
                m_cur_value = j.get<u8>();
                break;
            case MetaType::S16:
                m_cur_value = j.get<s16>();
                break;
            case MetaType::U16:
                m_cur_value = j.get<u16>();
                break;
            case MetaType::S32:
                m_cur_value = j.get<s32>();
                break;
            case MetaType::U32:
                m_cur_value = j.get<u32>();
                break;
            default:
                break;
            }
        });
    }

    void MetaEnum::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        indention_width          = std::min(indention_width, size_t(8));
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        out << self_indent << "enum " << m_name << "<" << meta_type_name(m_type) << "> {\n";
        for (const auto &v : m_values) {
            out << value_indent << v.first << " = " << v.second.toString() << ",\n";
        }
        out << self_indent << "}\n";
    }

    constexpr bool MetaEnum::operator==(const MetaEnum &other) const {
        return m_type == other.m_type && m_name == other.m_name && m_values == other.m_values &&
               m_cur_value == other.m_cur_value && m_bit_mask == other.m_bit_mask;
    }

    std::expected<void, SerialError> MetaEnum::serialize(Serializer &out) const {
        try {
            switch (m_type) {
            case MetaType::S8:
                out.write(m_cur_value.get<s8>());
                break;
            case MetaType::U8:
                out.write(m_cur_value.get<u8>());
                break;
            case MetaType::S16:
                out.write(m_cur_value.get<s16>());
                break;
            case MetaType::U16:
                out.write(m_cur_value.get<u16>());
                break;
            case MetaType::S32:
                out.write(m_cur_value.get<s32>());
                break;
            case MetaType::U32:
                out.write(m_cur_value.get<u32>());
                break;
            }
            return {};
        } catch (const std::exception &e) {
            auto err = make_serial_error(out, e.what());
            return std::unexpected(err);
        }
    }

    std::expected<void, SerialError> MetaEnum::deserialize(Deserializer &in) {
        try {
            switch (m_type) {
            case MetaType::S8:
                m_cur_value.set(in.read<s8>());
                break;
            case MetaType::U8:
                m_cur_value.set(in.read<u8>());
                break;
            case MetaType::S16:
                m_cur_value.set(in.read<s16>());
                break;
            case MetaType::U16:
                m_cur_value.set(in.read<u16>());
                break;
            case MetaType::S32:
                m_cur_value.set(in.read<s32>());
                break;
            case MetaType::U32:
                m_cur_value.set(in.read<u32>());
                break;
            }
            return {};
        } catch (const std::exception &e) {
            auto err = make_serial_error(in, e.what());
            return std::unexpected(err);
        }
    }

}  // namespace Toolbox::Object