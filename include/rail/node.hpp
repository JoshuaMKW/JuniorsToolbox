#pragma once

#include "clone.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "serial.hpp"
#include <string>
#include <vector>

using namespace Toolbox::Object;

namespace Toolbox::Rail {

    class Rail;

    class RailNode : public ISerializable, public IClonable {
        friend class Rail;

    public:
        RailNode();
        RailNode(u32 flags);
        RailNode(s16 x, s16 y, s16 z);
        RailNode(s16 x, s16 y, s16 z, u32 flags);
        RailNode(const RailNode &other) = default;
        RailNode(RailNode &&other)      = default;
        ~RailNode()                     = default;

        [[nodiscard]] Rail *rail() const { return m_rail; }

        [[nodiscard]] glm::vec3 getPosition() const;
        [[nodiscard]] void getPosition(s16 &x, s16 &y, s16 &z) const;

        [[nodiscard]] u32 getFlags() const;

        [[nodiscard]] std::expected<s16, MetaError> getValue(int index) const;

        [[nodiscard]] size_t getDataSize() const { return 68; }

        [[nodiscard]] s16 getConnectionCount() const;
        [[nodiscard]] std::expected<s16, MetaError> getConnectionValue(int index) const;
        [[nodiscard]] std::expected<f32, MetaError> getConnectionDistance(int index) const;

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override;

    protected:
        RailNode &operator=(const RailNode &other) = default;

        void setFlags(u32 flags);

        std::expected<void, MetaError> setValue(int index, s16 value);

        void setPosition(const glm::vec3 &position);
        void setPosition(s16 x, s16 y, s16 z) { setPosition({x, y, z}); }

        void setConnectionCount(s16 count);
        std::expected<void, MetaError> setConnectionValue(int index, s16 value);

        std::expected<void, MetaError> setConnectionDistance(int connection,
                                                             const glm::vec3 &to_pos);
        std::expected<void, MetaError> setConnectionDistance(int connection, f32 distance);

    private:
        Rail *m_rail;

        std::shared_ptr<MetaMember> m_pos_x;
        std::shared_ptr<MetaMember> m_pos_y;
        std::shared_ptr<MetaMember> m_pos_z;

        std::shared_ptr<MetaMember> m_flags;
        std::shared_ptr<MetaMember> m_values;

        std::shared_ptr<MetaMember> m_connection_count;
        std::shared_ptr<MetaMember> m_connections;
        std::shared_ptr<MetaMember> m_distances;
    };

}  // namespace Toolbox::Rail