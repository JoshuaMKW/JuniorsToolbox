#include "objlib/meta/value.hpp"
#include <format>
#include <string>

namespace Toolbox::Object {

    std::string MetaValue::toString() const {
        switch (m_type) {
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
            return std::get<std::string>(m_value);
        case MetaType::COMMENT:
            return std::get<std::string>(m_value);
        default:
            return "null";
        }
    }

}  // namespace Toolbox::Object