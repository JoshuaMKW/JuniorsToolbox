#include "scene/rail.hpp"

#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"

using namespace Toolbox::Object;

namespace Toolbox::Scene {

    RailNode::RailNode() : RailNode(0, 0, 0, 0) {}
    RailNode::RailNode(u32 flags) : RailNode(0, 0, 0, flags) {}
    RailNode::RailNode(s16 x, s16 y, s16 z) : RailNode(x, y, z, 0) {}

    RailNode::RailNode(s16 x, s16 y, s16 z, u32 flags) {
        m_pos_x = std::make_shared<MetaMember>("PositionX", MetaValue(x));
        m_pos_y = std::make_shared<MetaMember>("PositionY", MetaValue(y));
        m_pos_z = std::make_shared<MetaMember>("PositionZ", MetaValue(z));

        m_flags = std::make_shared<MetaMember>("Flags", MetaValue(flags));

        std::vector<MetaValue> values;
        values.push_back(MetaValue(static_cast<s16>(-1)));
        values.push_back(MetaValue(static_cast<s16>(-1)));
        values.push_back(MetaValue(static_cast<s16>(-1)));
        values.push_back(MetaValue(static_cast<s16>(-1)));

        m_values = std::make_shared<MetaMember>("Values", values);

        m_connection_count =
            std::make_shared<MetaMember>("ConnectionCount", MetaValue(static_cast<s16>(0)));
        m_connections =
            std::make_shared<MetaMember>("Connections", *m_connection_count->value<MetaValue>(0));
        m_distances =
            std::make_shared<MetaMember>("Periods", *m_connection_count->value<MetaValue>(0));
    }

    glm::vec3 RailNode::getPosition() const {
        std::shared_ptr<MetaValue> value_x = m_pos_x->value<MetaValue>(0).value();
        std::shared_ptr<MetaValue> value_y = m_pos_y->value<MetaValue>(0).value();
        std::shared_ptr<MetaValue> value_z = m_pos_z->value<MetaValue>(0).value();
        return glm::vec3(*value_x->get<s16>(), *value_y->get<s16>(), *value_z->get<s16>());
    }

    void RailNode::getPosition(s16 &x, s16 &y, s16 &z) const {
        std::shared_ptr<MetaValue> value_x = m_pos_x->value<MetaValue>(0).value();
        std::shared_ptr<MetaValue> value_y = m_pos_y->value<MetaValue>(0).value();
        std::shared_ptr<MetaValue> value_z = m_pos_z->value<MetaValue>(0).value();
        x                                  = *value_x->get<s16>();
        y                                  = *value_y->get<s16>();
        z                                  = *value_z->get<s16>();
    }

    void RailNode::setPosition(const glm::vec3 &position) {
        std::shared_ptr<MetaValue> value_x = m_pos_x->value<MetaValue>(0).value();
        std::shared_ptr<MetaValue> value_y = m_pos_y->value<MetaValue>(0).value();
        std::shared_ptr<MetaValue> value_z = m_pos_z->value<MetaValue>(0).value();
        value_x->set(static_cast<s16>(position.x));
        value_y->set(static_cast<s16>(position.y));
        value_z->set(static_cast<s16>(position.z));
    }

    u32 RailNode::getFlags() const {
        std::shared_ptr<MetaValue> value = m_flags->value<MetaValue>(0).value();
        return *value->get<u32>();
    }

    void RailNode::setFlags(u32 flags) {
        std::shared_ptr<MetaValue> value = m_flags->value<MetaValue>(0).value();
        value->set<u32>(flags);
    }

    std::expected<s16, MetaError> RailNode::getValue(int index) const {
        auto value = m_values->value<MetaValue>(index);
        if (!value) {
            return std::unexpected(value.error());
        }
        return *value.value()->get<s16>();
    }

    std::expected<void, MetaError> RailNode::setValue(int index, s16 value) {
        auto meta_value = m_values->value<MetaValue>(index);
        if (!meta_value) {
            return std::unexpected(meta_value.error());
        }
        meta_value.value()->set<s16>(value);
        return {};
    }

    s16 RailNode::getConnectionCount() const {
        auto value_count = m_connection_count->value<MetaValue>(0).value();
        return *value_count->get<s16>();
    }

    std::expected<s16, MetaError> RailNode::getConnectionValue(int index) const {
        auto value = m_connections->value<MetaValue>(index);
        if (!value) {
            return std::unexpected(value.error());
        }
        return *value.value()->get<s16>();
    }

    std::expected<f32, MetaError> RailNode::getConnectionDistance(int index) const {
        auto value = m_distances->value<MetaValue>(index);
        if (!value) {
            return std::unexpected(value.error());
        }
        return *value.value()->get<f32>();
    }

    std::expected<void, SerialError> RailNode::serialize(Serializer &out) const {
        auto connection_count = getConnectionCount();

        {
            s16 x, y, z;
            getPosition(x, y, z);

            out.write<s16, std::endian::big>(x);
            out.write<s16, std::endian::big>(y);
            out.write<s16, std::endian::big>(z);
        }

        out.write<s16, std::endian::big>(connection_count);
        out.write<u32, std::endian::big>(getFlags());

        for (int i = 0; i < 4; ++i) {
            auto value = getValue(i);
            if (!value) {
                auto err = make_serial_error(
                    out, std::format(
                             "Unexpected end of values in RailNode (expected 4 but only {} exist).",
                             i + 1));
                return std::unexpected(err);
            }
            out.write<s16, std::endian::big>(*value);
        }

        size_t connection_skip = (8 - connection_count) * 2;
        size_t distance_skip   = connection_skip * 2;

        for (int i = 0; i < connection_count; ++i) {
            auto connection = getConnectionValue(i);
            if (!connection) {
                auto err = make_serial_error(
                    out, std::format(
                             "Unexpected end of connections in RailNode (expected {} but only {} "
                             "exist).",
                             connection_count, i + 1));
                return std::unexpected(err);
            }
            out.write<s16, std::endian::big>(*connection);
        }

        for (int i = 0; i < connection_skip; ++i) {
            out.write<u8>(0);
        }

        for (int i = 0; i < connection_count; ++i) {
            auto distance = getConnectionDistance(i);
            if (!distance) {
                auto err = make_serial_error(
                    out,
                    std::format("Unexpected end of distances in RailNode (expected {} but only {} "
                                "exist).",
                                connection_count, i + 1));
                return std::unexpected(err);
            }
            out.write<f32, std::endian::big>(*distance);
        }

        for (size_t i = 0; i < distance_skip; ++i) {
            out.write<u8>(0);
        }

        return {};
    }

    std::expected<void, SerialError> RailNode::deserialize(Deserializer &in) {
        auto x = in.read<s16, std::endian::big>();
        auto y = in.read<s16, std::endian::big>();
        auto z = in.read<s16, std::endian::big>();
        setPosition(x, y, z);

        auto connection_count = in.read<s16, std::endian::big>();
        setConnectionCount(connection_count);

        auto flags = in.read<u32, std::endian::big>();
        setFlags(flags);

        for (int i = 0; i < 4; ++i) {
            auto value = in.read<s16, std::endian::big>();
            setValue(i, value);
        }

        size_t connection_skip = (8 - connection_count) * 2;
        size_t distance_skip   = connection_skip * 2;

        for (int i = 0; i < connection_count; ++i) {
            auto connection = in.read<s16, std::endian::big>();
            setConnectionValue(i, connection);
        }

        in.stream().seekg(connection_skip, std::ios_base::cur);

        for (int i = 0; i < connection_count; ++i) {
            auto distance = in.read<f32, std::endian::big>();
            setConnectionDistance(i, distance);
        }

        in.stream().seekg(distance_skip, std::ios_base::cur);

        return {};
    }

    void RailNode::setConnectionCount(s16 count) {
        auto value_count = m_connection_count->value<MetaValue>(0).value();
        value_count->set<s16>(count);
    }

    std::expected<void, MetaError> RailNode::setConnectionValue(int index, s16 value) {
        auto connection = m_connections->value<MetaValue>(index);
        if (!connection) {
            return std::unexpected(connection.error());
        }
        connection.value()->set<s16>(value);
    }

    std::expected<void, MetaError> RailNode::setConnectionDistance(int connection,
                                                                   const glm::vec3 &to_pos) {
        glm::vec3 position = getPosition();
        glm::vec3 delta    = to_pos - position;
        return setConnectionDistance(connection, glm::length(delta));
    }

    std::expected<void, MetaError> RailNode::setConnectionDistance(int connection, f32 distance) {
        auto meta_distance = m_distances->value<MetaValue>(connection);
        if (!meta_distance) {
            return std::unexpected(meta_distance.error());
        }
        meta_distance.value()->set<f32>(distance);
        return {};
    }

    std::unique_ptr<IClonable> RailNode::clone(bool deep) const {
        auto node     = std::make_unique<RailNode>();
        node->m_pos_x = std::reinterpret_pointer_cast<MetaMember, IClonable>(m_pos_x->clone(deep));
        node->m_pos_y = std::reinterpret_pointer_cast<MetaMember, IClonable>(m_pos_y->clone(deep));
        node->m_pos_z = std::reinterpret_pointer_cast<MetaMember, IClonable>(m_pos_z->clone(deep));
        node->m_flags = std::reinterpret_pointer_cast<MetaMember, IClonable>(m_flags->clone(deep));
        node->m_values =
            std::reinterpret_pointer_cast<MetaMember, IClonable>(m_values->clone(deep));
        node->m_connection_count =
            std::reinterpret_pointer_cast<MetaMember, IClonable>(m_connection_count->clone(deep));
        node->m_connections =
            std::reinterpret_pointer_cast<MetaMember, IClonable>(m_connections->clone(deep));
        node->m_distances =
            std::reinterpret_pointer_cast<MetaMember, IClonable>(m_distances->clone(deep));
        return node;
    }

}  // namespace Toolbox::Scene