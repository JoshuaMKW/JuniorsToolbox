#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include <iostream>
#include <string>

namespace Toolbox::Object {

    Result<void, JSONError> MetaEnum::loadJSON(const nlohmann::json &json_value) {
        return tryJSON(json_value, [this](const json &j) {
            switch (m_type) {
            case MetaType::S8:
                m_cur_value = make_referable<MetaValue>(j.get<s8>());
                break;
            case MetaType::U8:
                m_cur_value = make_referable<MetaValue>(j.get<u8>());
                break;
            case MetaType::S16:
                m_cur_value = make_referable<MetaValue>(j.get<s16>());
                break;
            case MetaType::U16:
                m_cur_value = make_referable<MetaValue>(j.get<u16>());
                break;
            case MetaType::S32:
                m_cur_value = make_referable<MetaValue>(j.get<s32>());
                break;
            case MetaType::U32:
                m_cur_value = make_referable<MetaValue>(j.get<u32>());
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

    bool MetaEnum::operator==(const MetaEnum &other) const {
        return m_type == other.m_type && m_name == other.m_name && m_values == other.m_values &&
               m_cur_value == other.m_cur_value && m_bit_mask == other.m_bit_mask;
    }

    Result<void, SerialError> MetaEnum::serialize(Serializer &out) const {
        return gameSerialize(out);
    }

    Result<void, SerialError> MetaEnum::deserialize(Deserializer &in) {
        return gameDeserialize(in);
    }

    Result<void, SerialError> MetaEnum::gameSerialize(Serializer &out) const {
        try {
            switch (m_type) {
            case MetaType::S8:
                out.write(m_cur_value->get<s8>().value());
                break;
            case MetaType::U8:
                out.write(m_cur_value->get<u8>().value());
                break;
            case MetaType::S16:
                out.write<s16, std::endian::big>(m_cur_value->get<s16>().value());
                break;
            case MetaType::U16:
                out.write<u16, std::endian::big>(m_cur_value->get<u16>().value());
                break;
            case MetaType::S32:
                out.write<s32, std::endian::big>(m_cur_value->get<s32>().value());
                break;
            case MetaType::U32:
                out.write<u32, std::endian::big>(m_cur_value->get<u32>().value());
                break;
            }
            return {};
        } catch (const std::exception &e) {
            return make_serial_error<void>(out, e.what());
        }
    }

    Result<void, SerialError> MetaEnum::gameDeserialize(Deserializer &in) {
        try {
            switch (m_type) {
            case MetaType::S8:
                m_cur_value->set(in.read<s8>());
                break;
            case MetaType::U8:
                m_cur_value->set(in.read<u8>());
                break;
            case MetaType::S16:
                m_cur_value->set(in.read<s16, std::endian::big>());
                break;
            case MetaType::U16:
                m_cur_value->set(in.read<u16, std::endian::big>());
                break;
            case MetaType::S32:
                m_cur_value->set(in.read<s32, std::endian::big>());
                break;
            case MetaType::U32:
                m_cur_value->set(in.read<u32, std::endian::big>());
                break;
            }
            return {};
        } catch (const std::exception &e) {
            return make_serial_error<void>(in, e.what());
        }
    }

    ScopePtr<ISmartResource> MetaEnum::clone(bool deep) const {
        MetaEnum enum_(std::string_view(m_name), m_type, m_values, m_bit_mask);
        enum_.m_cur_value = make_referable<MetaValue>(*m_cur_value);
        return make_scoped<MetaEnum>(enum_);
    }

}  // namespace Toolbox::Object