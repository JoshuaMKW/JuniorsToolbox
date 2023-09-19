#pragma once

#include "qualname.hpp"
#include "serial.hpp"
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

        [[nodiscard]] virtual std::optional<NameRef> find(std::string_view name) const {
            return {};
        }

        /*[[nodiscard]] std::optional<NameRef> qfind(const Toolbox::Object::QualifiedName &qname)
        const { NameRef current = *this; for (auto &scope : qname) { auto result =
        find(std::string_view(scope.begin(), scope.end())); if (!result) return {}; current =
        *result;
            }
            return current;
        }*/

        std::expected<void, SerialError> serialize(Serializer &serializer) const override {
            serializer.write<u16, std::endian::big>(m_name_hash);
            serializer.writeString<std::endian::big>(m_name);
            return {};
        }

        std::expected<void, SerialError> deserialize(Deserializer &deserializer) override {
            m_name_hash = deserializer.read<u16, std::endian::big>();
            m_name      = deserializer.readString<std::endian::big>();
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