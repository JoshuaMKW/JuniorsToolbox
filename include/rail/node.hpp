#pragma once

#include "smart_resource.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "serial.hpp"
#include "unique.hpp"
#include <string>
#include <vector>

using namespace Toolbox::Object;

namespace Toolbox::Rail {

    class Rail;

    class RailNode : public ISerializable, public ISmartResource, public IUnique {
    protected:
        friend class Rail;

    public:
        RailNode();
        RailNode(u32 flags);
        RailNode(s16 x, s16 y, s16 z);
        RailNode(s16 x, s16 y, s16 z, u32 flags);
        RailNode(glm::vec3 pos);
        RailNode(glm::vec3 pos, u32 flags);
        RailNode(const RailNode &other) = default;
        RailNode(RailNode &&other)      = default;
        ~RailNode()                     = default;

        [[nodiscard]] UUID64 getUUID() const override { return m_UUID64; }

        [[nodiscard]] UUID64 getRailUUID() const { return m_rail_uuid; }

        [[nodiscard]] glm::vec3 getPosition() const;
        [[nodiscard]] void getPosition(s16 &x, s16 &y, s16 &z) const;

        [[nodiscard]] u32 getFlags() const;

        [[nodiscard]] Result<s16, MetaError> getValue(size_t index) const;

        [[nodiscard]] size_t getDataSize() const { return 68; }

        [[nodiscard]] u16 getConnectionCount() const;
        [[nodiscard]] Result<s16, MetaError> getConnectionValue(size_t index) const;
        [[nodiscard]] Result<f32, MetaError> getConnectionDistance(size_t index) const;

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        ScopePtr<ISmartResource> clone(bool deep) const override;

        RailNode &operator=(const RailNode &other) = default;
        RailNode &operator=(RailNode &&other)      = default;

        bool operator==(const RailNode &other) const;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

    protected:
        void setFlags(u32 flags);

        Result<void, MetaError> setValue(size_t index, s16 value);

        void setPosition(const glm::vec3 &position);
        void setPosition(s16 x, s16 y, s16 z) { setPosition({x, y, z}); }

        void setConnectionCount(u16 count);
        Result<void, MetaError> setConnectionValue(size_t index, s16 value);

        Result<void, MetaError> setConnectionDistance(size_t connection,
                                                             const glm::vec3 &to_pos);
        Result<void, MetaError> setConnectionDistance(size_t connection, f32 distance);

    private:
        UUID64 m_rail_uuid;

        UUID64 m_UUID64;

        RefPtr<MetaMember> m_pos_x;
        RefPtr<MetaMember> m_pos_y;
        RefPtr<MetaMember> m_pos_z;

        RefPtr<MetaMember> m_flags;
        RefPtr<MetaMember> m_values;

        RefPtr<MetaMember> m_connection_count;
        RefPtr<MetaMember> m_connections;
        RefPtr<MetaMember> m_distances;
    };

}  // namespace Toolbox::Rail