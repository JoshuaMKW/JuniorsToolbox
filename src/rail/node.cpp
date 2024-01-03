#include "rail/rail.hpp"

#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"

using namespace Toolbox::Object;

namespace Toolbox::Rail {

    RailNode::RailNode() : RailNode(0, 0, 0, 0) {}
    RailNode::RailNode(u32 flags) : RailNode(0, 0, 0, flags) {}
    RailNode::RailNode(s16 x, s16 y, s16 z) : RailNode(x, y, z, 0) {}

    RailNode::RailNode(s16 x, s16 y, s16 z, u32 flags) {
        m_pos_x = std::make_shared<MetaMember>("PositionX", MetaValue(x));
        m_pos_y = std::make_shared<MetaMember>("PositionY", MetaValue(y));
        m_pos_z = std::make_shared<MetaMember>("PositionZ", MetaValue(z));

        m_flags = std::make_shared<MetaMember>("Flags", MetaValue(flags));

        MetaValue values_default = MetaValue(static_cast<s16>(-1));

        std::vector<MetaValue> values;
        values.push_back(values_default);
        values.push_back(values_default);
        values.push_back(values_default);
        values.push_back(values_default);

        m_values = std::make_shared<MetaMember>("Values", values,
                                                std::make_shared<MetaValue>(values_default));

        m_connection_count =
            std::make_shared<MetaMember>("ConnectionCount", MetaValue(static_cast<u32>(0)));

        auto connection_value          = m_connection_count->value<MetaValue>(0).value();
        MetaMember::ReferenceInfo info = {connection_value, "ConnectionCount"};

        m_connections = std::make_shared<MetaMember>(
            "Connections", info, std::make_shared<MetaValue>(MetaValue(static_cast<s16>(0))));
        m_distances = std::make_shared<MetaMember>(
            "Distances", info, std::make_shared<MetaValue>(MetaValue(static_cast<f32>(0))));
    }

    RailNode::RailNode(glm::vec3 pos)
        : RailNode(static_cast<s16>(pos.x), static_cast<s16>(pos.y), static_cast<s16>(pos.z)) {}
    RailNode::RailNode(glm::vec3 pos, u32 flags)
        : RailNode(static_cast<s16>(pos.x), static_cast<s16>(pos.y), static_cast<s16>(pos.z),
                   flags) {}

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
        value_x->set(std::clamp<s16>(position.x, -32768, 32767));
        value_y->set(std::clamp<s16>(position.y, -32768, 32767));
        value_z->set(std::clamp<s16>(position.z, -32768, 32767));
    }

    u32 RailNode::getFlags() const {
        std::shared_ptr<MetaValue> value = m_flags->value<MetaValue>(0).value();
        return *value->get<u32>();
    }

    bool RailNode::operator==(const RailNode &other) const {
        return getPosition() == other.getPosition() && getFlags() == other.getFlags();
    }

    void RailNode::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        std::string indention_str(indention * indention_width, ' ');
        std::string value_indention_str((indention + 1) * indention_width, ' ');
        out << indention_str << "RailNode {" << std::endl;
        out << value_indention_str << std::format("Position: {}", getPosition()) << std::endl;
        out << value_indention_str << "Flags: " << getFlags() << std::endl;
        out << value_indention_str << "Values: [";
        for (int i = 0; i < 4; i++) {
            auto value = getValue(i);
            out << *value;
            if (i < 3) {
                out << ", ";
            }
        }
        out << "]" << std::endl;
        out << value_indention_str << "Connections: [";
        for (int i = 0; i < getConnectionCount(); i++) {
            auto value = getConnectionValue(i);
            out << *value;
            if (i < getConnectionCount() - 1) {
                out << ", ";
            }
        }
        out << "]" << std::endl;
        out << value_indention_str << "Distances: [";
        for (int i = 0; i < getConnectionCount(); i++) {
            auto value = getConnectionDistance(i);
            out << *value;
            if (i < getConnectionCount() - 1) {
                out << ", ";
            }
        }
        out << "]" << std::endl;
        out << indention_str << "}" << std::endl;
    }

    void RailNode::setFlags(u32 flags) {
        std::shared_ptr<MetaValue> value = m_flags->value<MetaValue>(0).value();
        value->set<u32>(flags);
    }

    std::expected<s16, MetaError> RailNode::getValue(size_t index) const {
        auto value = m_values->value<MetaValue>(index);
        if (!value) {
            return std::unexpected(value.error());
        }
        return *value.value()->get<s16>();
    }

    std::expected<void, MetaError> RailNode::setValue(size_t index, s16 value) {
        auto meta_value = m_values->value<MetaValue>(index);
        if (!meta_value) {
            return std::unexpected(meta_value.error());
        }
        meta_value.value()->set<s16>(value);
        return {};
    }

    u16 RailNode::getConnectionCount() const {
        auto value_count = m_connection_count->value<MetaValue>(0).value();
        return static_cast<u16>(*value_count->get<u32>());
    }

    std::expected<s16, MetaError> RailNode::getConnectionValue(size_t index) const {
        auto value = m_connections->value<MetaValue>(index);
        if (!value) {
            return std::unexpected(value.error());
        }
        return *value.value()->get<s16>();
    }

    std::expected<f32, MetaError> RailNode::getConnectionDistance(size_t index) const {
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
                return make_serial_error<void>(
                    out, std::format(
                             "Unexpected end of values in RailNode (expected 4 but only {} exist).",
                             i + 1));
            }
            out.write<s16, std::endian::big>(*value);
        }

        size_t connection_skip = (8 - connection_count) * 2;
        size_t distance_skip   = connection_skip * 2;

        for (int i = 0; i < connection_count; ++i) {
            auto connection = getConnectionValue(i);
            if (!connection) {
                return make_serial_error<void>(
                    out, std::format(
                             "Unexpected end of connections in RailNode (expected {} but only {} "
                             "exist).",
                             connection_count, i + 1));
            }
            out.write<s16, std::endian::big>(*connection);
        }

        for (int i = 0; i < connection_skip; ++i) {
            out.write<u8>(0);
        }

        for (int i = 0; i < connection_count; ++i) {
            auto distance = getConnectionDistance(i);
            if (!distance) {
                return make_serial_error<void>(
                    out,
                    std::format("Unexpected end of distances in RailNode (expected {} but only {} "
                                "exist).",
                                connection_count, i + 1));
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

    void RailNode::setConnectionCount(u16 count) {
        auto value_count = m_connection_count->value<MetaValue>(0).value();
        value_count->set<u32>(count);
        m_connections->syncArray();
        m_distances->syncArray();
    }

    std::expected<void, MetaError> RailNode::setConnectionValue(size_t index, s16 value) {
        auto connection = m_connections->value<MetaValue>(index);
        if (!connection) {
            return std::unexpected(connection.error());
        }
        connection.value()->set<s16>(value);
        return {};
    }

    std::expected<void, MetaError> RailNode::setConnectionDistance(size_t connection,
                                                                   const glm::vec3 &to_pos) {
        glm::vec3 position = getPosition();
        glm::vec3 delta    = to_pos - position;
        return setConnectionDistance(connection, glm::length(delta));
    }

    std::expected<void, MetaError> RailNode::setConnectionDistance(size_t connection,
                                                                   f32 distance) {
        auto meta_distance = m_distances->value<MetaValue>(connection);
        if (!meta_distance) {
            return std::unexpected(meta_distance.error());
        }
        meta_distance.value()->set<f32>(distance);
        return {};
    }

    std::unique_ptr<IClonable> RailNode::clone(bool deep) const {
        auto node = std::make_unique<RailNode>();
        if (deep) {
            node->m_pos_x            = make_deep_clone<MetaMember>(m_pos_x);
            node->m_pos_y            = make_deep_clone<MetaMember>(m_pos_y);
            node->m_pos_z            = make_deep_clone<MetaMember>(m_pos_z);
            node->m_flags            = make_deep_clone<MetaMember>(m_flags);
            node->m_values           = make_deep_clone<MetaMember>(m_values);
            node->m_connection_count = make_deep_clone<MetaMember>(m_connection_count);
            node->m_connections      = make_deep_clone<MetaMember>(m_connections);
            node->m_distances        = make_deep_clone<MetaMember>(m_distances);
        } else {
            node->m_pos_x            = make_clone<MetaMember>(m_pos_x);
            node->m_pos_y            = make_clone<MetaMember>(m_pos_y);
            node->m_pos_z            = make_clone<MetaMember>(m_pos_z);
            node->m_flags            = make_clone<MetaMember>(m_flags);
            node->m_values           = make_clone<MetaMember>(m_values);
            node->m_connection_count = make_clone<MetaMember>(m_connection_count);
            node->m_connections      = make_clone<MetaMember>(m_connections);
            node->m_distances        = make_clone<MetaMember>(m_distances);
        }
        return node;
    }

}  // namespace Toolbox::Rail