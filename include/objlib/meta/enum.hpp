#pragma once

#include "types.hpp"
#include "value.hpp"
#include "serial.hpp"
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Toolbox::Object {

    class MetaEnum : public ISerializable {
    public:
        using enum_type = std::pair<std::string, MetaValue>;

        using iterator               = std::vector<enum_type>::iterator;
        using const_iterator         = std::vector<enum_type>::const_iterator;
        using reverse_iterator       = std::vector<enum_type>::reverse_iterator;
        using const_reverse_iterator = std::vector<enum_type>::const_reverse_iterator;

        constexpr MetaEnum() = delete;
        constexpr MetaEnum(std::string_view name, std::vector<enum_type> values,
                           bool bit_mask = false)
            : m_type(MetaType::S32), m_name(name), m_values(std::move(values)),
              m_bit_mask(bit_mask) {
            switch (m_type) {
            case MetaType::S8:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            case MetaType::U8:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            case MetaType::S16:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            case MetaType::U16:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            case MetaType::S32:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            case MetaType::U32:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            default:
                break;
            }
        }
        constexpr MetaEnum(std::string_view name, MetaType type, std::vector<enum_type> values,
                           bool bit_mask = false)
            : MetaEnum(name, values, bit_mask) {
            m_type = type;
            switch (type) {
            case MetaType::S8:
                m_cur_value = MetaValue(static_cast<s8>(0));
                break;
            case MetaType::U8:
                m_cur_value = MetaValue(static_cast<u8>(0));
                break;
            case MetaType::S16:
                m_cur_value = MetaValue(static_cast<s16>(0));
                break;
            case MetaType::U16:
                m_cur_value = MetaValue(static_cast<u16>(0));
                break;
            case MetaType::S32:
                m_cur_value = MetaValue(static_cast<s32>(0));
                break;
            case MetaType::U32:
                m_cur_value = MetaValue(static_cast<u32>(0));
                break;
            default:
                break;
            }
        }
        constexpr MetaEnum(const MetaEnum &other) = default;
        constexpr MetaEnum(MetaEnum &&other)      = default;
        constexpr ~MetaEnum()                     = default;

        [[nodiscard]] constexpr MetaType type() const { return m_type; }
        [[nodiscard]] constexpr std::string_view name() const { return m_name; }
        [[nodiscard]] constexpr MetaValue value() const { return m_cur_value; }
        [[nodiscard]] constexpr std::vector<enum_type> enums() const { return m_values; }

        [[nodiscard]] constexpr bool isBitMasked() const { return m_bit_mask; }

        [[nodiscard]] constexpr bool getFlag(std::string_view name) const {
            auto flag = find(name);
            if (!flag)
                return false;

            return getFlag(*flag);
        }

        [[nodiscard]] constexpr bool getFlag(int index) const {
            if (index < 0 || index >= m_values.size())
                return false;

            auto flag = m_values[index];
            return getFlag(flag);
        }

        [[nodiscard]] constexpr bool getFlag(enum_type value) const {
            switch (m_type) {
            case MetaType::S8:
                return *m_cur_value.get<s8>() & *value.second.get<s8>();
            case MetaType::U8:
                return *m_cur_value.get<u8>() & *value.second.get<u8>();
            case MetaType::S16:
                return *m_cur_value.get<s16>() & *value.second.get<s16>();
            case MetaType::U16:
                return *m_cur_value.get<u16>() & *value.second.get<u16>();
            case MetaType::S32:
                return *m_cur_value.get<s32>() & *value.second.get<s32>();
            case MetaType::U32:
                return *m_cur_value.get<u32>() & *value.second.get<u32>();
            default:
                return false;
            }
        }

        constexpr void setFlag(std::string_view name, bool value) {
            auto flag = find(name);
            if (!flag)
                return;

            setFlag(*flag, value);
        }

        constexpr void setFlag(int index, bool value) {
            if (index < 0 || index >= m_values.size())
                return;

            auto flag = m_values[index];
            setFlag(flag, value);
        }

        constexpr void setFlag(enum_type flag, bool value) {
            switch (m_type) {
            case MetaType::S8: {
                if (value) {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() | *flag.second.get<s8>());
                } else {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() & ~(*flag.second.get<s8>()));
                }
                break;
            }
            case MetaType::U8: {
                if (value) {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() | *flag.second.get<s8>());
                } else {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() & ~(*flag.second.get<s8>()));
                }
                break;
            }
            case MetaType::S16: {
                if (value) {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() | *flag.second.get<s8>());
                } else {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() & ~(*flag.second.get<s8>()));
                }
                break;
            }
            case MetaType::U16: {
                if (value) {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() | *flag.second.get<s8>());
                } else {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() & ~(*flag.second.get<s8>()));
                }
                break;
            }
            case MetaType::S32: {
                if (value) {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() | *flag.second.get<s8>());
                } else {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() & ~(*flag.second.get<s8>()));
                }
                break;
            }
            case MetaType::U32: {
                if (value) {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() | *flag.second.get<s8>());
                } else {
                    m_cur_value.set<s8>(*m_cur_value.get<s8>() & ~(*flag.second.get<s8>()));
                }
                break;
            }
            }
        }

        [[nodiscard]] constexpr iterator begin() { return m_values.begin(); }
        [[nodiscard]] constexpr const_iterator begin() const { return m_values.begin(); }
        [[nodiscard]] constexpr const_iterator cbegin() const { return m_values.cbegin(); }

        [[nodiscard]] constexpr iterator end() { return m_values.end(); }
        [[nodiscard]] constexpr const_iterator end() const { return m_values.end(); }
        [[nodiscard]] constexpr const_iterator cend() const { return m_values.cend(); }

        [[nodiscard]] constexpr reverse_iterator rbegin() { return m_values.rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const { return m_values.rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const {
            return m_values.crbegin();
        }

        [[nodiscard]] constexpr reverse_iterator rend() { return m_values.rend(); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const { return m_values.rend(); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const { return m_values.crend(); }

        [[nodiscard]] constexpr std::optional<enum_type> find(std::string_view name) const {
            for (const auto &v : m_values) {
                if (v.first == name)
                    return v;
            }
            return {};
        }

        template <typename T>
        [[nodiscard]] constexpr std::optional<enum_type> vfind(T value) const {
            return {};
        }

        template <> [[nodiscard]] constexpr std::optional<enum_type> vfind<s8>(s8 value) const {
            for (const auto &v : m_values) {
                auto _v = v.second.get<s8>();
                if (_v.has_value() && *_v == value)
                    return v;
            }
            return {};
        }

        template <> [[nodiscard]] constexpr std::optional<enum_type> vfind<u8>(u8 value) const {
            for (const auto &v : m_values) {
                auto _v = v.second.get<u8>();
                if (_v.has_value() && *_v == value)
                    return v;
            }
            return {};
        }

        template <> [[nodiscard]] constexpr std::optional<enum_type> vfind<s16>(s16 value) const {
            for (const auto &v : m_values) {
                auto _v = v.second.get<s16>();
                if (_v.has_value() && *_v == value)
                    return v;
            }
            return {};
        }

        template <> [[nodiscard]] constexpr std::optional<enum_type> vfind<u16>(u16 value) const {
            for (const auto &v : m_values) {
                auto _v = v.second.get<u16>();
                if (_v.has_value() && *_v == value)
                    return v;
            }
            return {};
        }

        template <> [[nodiscard]] constexpr std::optional<enum_type> vfind<s32>(s32 value) const {
            for (const auto &v : m_values) {
                auto _v = v.second.get<s32>();
                if (_v.has_value() && *_v == value)
                    return v;
            }
            return {};
        }

        template <> [[nodiscard]] constexpr std::optional<enum_type> vfind<u32>(u32 value) const {
            for (const auto &v : m_values) {
                auto _v = v.second.get<u32>();
                if (_v.has_value() && *_v == value)
                    return v;
            }
            return {};
        }

        std::expected<void, JSONError> loadJSON(const nlohmann::json &json);

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        constexpr bool operator==(const MetaEnum &other) const;

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

    private:
        MetaType m_type;
        std::string m_name;
        std::vector<enum_type> m_values = {};
        MetaValue m_cur_value           = MetaValue(static_cast<s32>(0));
        bool m_bit_mask                 = false;
    };

}  // namespace Toolbox::Object