#pragma once

#include "core/types.hpp"
#include <random>
#include <format>

namespace Toolbox {

    class UUID64 {
    public:
        using device      = std::random_device;
        using engine      = std::mt19937_64;
        using distributor = std::uniform_int_distribution<u64>;

        UUID64() { _generate(); }
        UUID64(u64 _UUID64) : m_UUID64(_UUID64) {}

        operator u64() const { return m_UUID64; }

    protected:
        u64 _generate();

    private:
        u64 m_UUID64;
    };

    class IUnique {
    public:
        [[nodiscard]] virtual UUID64 getUUID() const = 0;
    };

}  // namespace Toolbox

namespace std {

    template <> struct hash<Toolbox::UUID64> {
        Toolbox::u64 operator()(const Toolbox::UUID64 &UUID64) const {
            return hash<Toolbox::u64>()((Toolbox::u64)UUID64);
        }
    };

    template <> struct formatter<Toolbox::UUID64> : formatter<Toolbox::u64> {
        template <typename FormatContext>
        auto format(const Toolbox::UUID64 &obj, FormatContext &ctx) const {
            return formatter<Toolbox::u64>::format((Toolbox::u64)obj, ctx);
        }
    };

}  // namespace std