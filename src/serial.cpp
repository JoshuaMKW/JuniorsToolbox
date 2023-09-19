#include "serial.hpp"

namespace Toolbox {

    void Serializer::pushBreakpoint() { m_breakpoints.push(m_out.tellp()); }
    std::expected<void, SerialError> Serializer::popBreakpoint() {
        if (m_breakpoints.empty()) {
            auto err = make_serial_error(
                *this, "No breakpoints to pop! (Proper serialization shouldn't have this happen)");
            return std::unexpected(err);
        }
        m_out.seekp(m_breakpoints.front());
        m_breakpoints.pop();
        return {};
    }

    void Deserializer::pushBreakpoint() { m_breakpoints.push(m_in.tellg()); }
    std::expected<void, SerialError> Deserializer::popBreakpoint() {
        if (m_breakpoints.empty()) {
            auto err = make_serial_error(
                *this,
                "No breakpoints to pop! (Proper deserialization shouldn't have this happen)");
            return std::unexpected(err);
        }
        m_in.seekg(m_breakpoints.front());
        m_breakpoints.pop();
        return {};
    }

}  // namespace Toolbox