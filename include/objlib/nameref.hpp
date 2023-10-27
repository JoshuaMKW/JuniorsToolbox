#pragma once

#include "qualname.hpp"
#include "serial.hpp"
#include "strutil.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace Toolbox::Object {

    class NameRef : public ISerializable {
    public:
        NameRef() = default;
        NameRef(std::string_view name) : m_name(name) {}
        virtual ~NameRef() = default;

        [[nodiscard]] std::string_view name() const { return m_name; }
        [[nodiscard]] u16 code() const { return m_name_hash; }

        void setName(std::string_view name) {
            m_name_hash = calcKeyCode(name);
            m_name      = name;
        }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            out.write<u16, std::endian::big>(m_name_hash);
            out.writeString<std::endian::big>(m_name);
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            m_name_hash     = in.read<u16, std::endian::big>();
            auto str_result = String::toShiftJIS(in.readString<std::endian::big>());
            if (!str_result) {
                return make_serial_error<void>(in, "Failed to deserialize string as UTF-8");
            }
            m_name      = str_result.value();
            return {};
        }

        [[nodiscard]] static constexpr u16 calcKeyCode(std::string_view str) {
            u32 code = 0;
            for (auto &ch : str) {
                code = ch + (code * 3);
            }
            return code;
        }

    private:
        u16 m_name_hash    = calcKeyCode("(null)");
        std::string m_name = "(null)";
    };

}  // namespace Toolbox::Object