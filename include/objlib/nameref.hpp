#pragma once

#include "qualname.hpp"
#include "serial.hpp"
#include "strutil.hpp"
#include <optional>
#include <string>
#include <string_view>

using namespace Toolbox::String;

namespace Toolbox::Object {

    class NameRef : public ISerializable {
    public:
        NameRef() = default;
        NameRef(std::string_view name) { setName(name); }
        virtual ~NameRef() = default;

        [[nodiscard]] std::string_view name() const { return m_name; }
        [[nodiscard]] u16 code() const { return m_name_hash; }

        std::expected<void, EncodingError> setName(std::string_view name) {
            auto result = String::toGameEncoding(name);
            if (!result) {
                return std::unexpected(result.error());
            }
            m_name_hash = calcKeyCode(result.value());
            m_name      = name;
            return {};
        }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            auto result = String::toGameEncoding(m_name);
            if (!result) {
                return make_serial_error<void>(out, result.error().m_message[0]);
            }

            out.write<u16, std::endian::big>(m_name_hash);
            out.writeString<std::endian::big>(result.value());
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            m_name_hash     = in.read<u16, std::endian::big>();
            auto str_result = String::fromGameEncoding(in.readString<std::endian::big>());
            if (!str_result) {
                return make_serial_error<void>(in, str_result.error().m_message[0]);
            }
            m_name      = str_result.value();
            return {};
        }

        [[nodiscard]] static constexpr u16 calcKeyCode(std::string_view str) {
            u32 code = 0;
            for (auto &ch : str) {
                code = ch + (code * 3);
            }
            return code & 0xFFFF;
        }

    private:
        u16 m_name_hash    = calcKeyCode("(null)");
        std::string m_name = "(null)";
    };

}  // namespace Toolbox::Object