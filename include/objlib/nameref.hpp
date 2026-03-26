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

        Result<void, EncodingError> setName(std::string_view name) {
            auto result = String::toGameEncoding(name);
            if (!result) {
                return std::unexpected(result.error());
            }
            m_name_hash = calcKeyCode(result.value());
            m_name      = name;
            return {};
        }

        Result<void, SerialError> serialize(Serializer &out) const override {
            auto result = String::toGameEncoding(m_name);
            if (!result) {
                return make_serial_error<void>(out, result.error().m_message[0]);
            }

            out.write<u16, std::endian::big>(m_name_hash);
            out.writeString<std::endian::big>(result.value());
            return {};
        }



        Result<void, SerialError> deserialize(Deserializer &in) override {
            const u16 proposed_hash        = in.read<u16, std::endian::big>();
            const std::string encoded_name = in.readString<std::endian::big>();
            const u16 calculated_hash      = calcKeyCode(encoded_name);

            auto str_result = String::fromGameEncoding(encoded_name);
            if (!str_result) {
                return make_serial_error<void>(in, str_result.error().m_message[0]);
            }

            m_name      = str_result.value();
            m_name_hash = calculated_hash;

            if (proposed_hash != calculated_hash) {
                TOOLBOX_WARN_V(
                    "[NameRef] Proposed hash {} did not match calculated hash {} for string {}",
                    proposed_hash, calculated_hash, m_name);
            }

            return {};
        }

        [[nodiscard]] static constexpr u16 calcKeyCode(std::string_view str) {
            u32 code = 0;

            for (size_t i = 0; i < str.length(); ++i) {
                const u8 ch = static_cast<u8>(str[i]);
                if (ch == '\0') {
                    break;
                }
                code = ch + (code * 3);
            }

            return code & 0xFFFF;
        }

        NameRef &operator=(const NameRef &other) {
            m_name_hash = other.m_name_hash;
            m_name      = other.m_name;
            return *this;
        }

        NameRef &operator=(const std::string &name) {
            setName(name);
            return *this;
        }

        NameRef &operator=(std::string_view name) {
            setName(name);
            return *this;
        }

        bool operator==(const std::string &other) const { return m_name == other; }

        bool operator==(const NameRef &other) const {
            return m_name_hash == other.m_name_hash || m_name == other.m_name;
        }

        bool operator!=(const NameRef &other) const { return !(*this == other); }

    private:
        u16 m_name_hash    = calcKeyCode("(null)");
        std::string m_name = "(null)";
    };

}  // namespace Toolbox::Object