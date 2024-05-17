#pragma once

#include "boundbox.hpp"
#include "smart_resource.hpp"
#include "node.hpp"
#include "unique.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "serial.hpp"
#include <string>
#include <vector>

using namespace Toolbox::Object;

namespace Toolbox::Rail {

    // NOTE: Serialization is for Toolbox UI only. Use RalData for actual game data.
    class Rail : public ISerializable, public ISmartResource, public IUnique {
    public:
        using node_ptr_t = RefPtr<RailNode>;

        Rail() = delete;
        explicit Rail(std::string_view name) : m_name(name) {}
        Rail(std::string_view name, std::vector<node_ptr_t> nodes) : m_name(name), m_nodes(nodes) {
            for (auto &node : m_nodes) {
                node->m_rail = this;
            }
        }
        Rail(const Rail &other) = default;
        Rail(Rail &&other)      = default;

        ~Rail() = default;

        Rail &operator=(const Rail &other) = default;
        Rail &operator=(Rail &&other)      = default;

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        [[nodiscard]] UUID64 getUUID() const override { return m_UUID64; }

        [[nodiscard]] u32 getSiblingID() const { return m_sibling_id; }
        void setSiblingID(u32 id) { m_sibling_id = id; }

        [[nodiscard]] bool isSpline() const { return m_name.starts_with("S_"); }

        [[nodiscard]] std::string name() const { return m_name; }
        void setName(std::string_view name) { m_name = name; }

        [[nodiscard]] const std::vector<node_ptr_t> &nodes() const { return m_nodes; }
        [[nodiscard]] std::vector<node_ptr_t> &nodes() { return m_nodes; }

        [[nodiscard]] glm::vec3 getCenteroid() const;
        [[nodiscard]] BoundingBox getBoundingBox() const;

        [[nodiscard]] size_t getDataSize() const { return 68 * m_nodes.size(); }

        Rail &translate(s16 x, s16 y, s16 z) { return translate(glm::vec3(x, y, z)); }
        Rail &translate(const glm::vec3 &t);

        Rail &rotate(f32 x, f32 y, f32 z) { return rotate(glm::vec3(x, y, z)); }
        Rail &rotate(const glm::vec3 &r) { return rotate(glm::quat(r)); }
        Rail &rotate(const glm::quat &r);

        Rail &scale(f32 x, f32 y, f32 z) { return scale(glm::vec3(x, y, z)); }
        Rail &scale(const glm::vec3 &s);

        Rail &invert(bool x, bool y, bool z);

        void subdivide(size_t iterations);

        // Node API

        [[nodiscard]] size_t getNodeCount() const { return m_nodes.size(); }

        void addNode(node_ptr_t node);

        Result<void, MetaError> insertNode(size_t index, node_ptr_t node);

        Result<void, MetaError> removeNode(size_t index);
        bool removeNode(node_ptr_t node);

        Result<void, MetaError> swapNodes(size_t index1, size_t index2);
        bool swapNodes(node_ptr_t node1, node_ptr_t node2);

        [[nodiscard]] bool isNodeConnectedToOther(size_t node_a, size_t node_b) const;
        [[nodiscard]] bool isNodeConnectedToOther(node_ptr_t node_a, node_ptr_t node_b) const;

        std::optional<size_t> getNodeIndex(node_ptr_t node) const;
        std::vector<node_ptr_t> getNodeConnections(size_t node) const;
        std::vector<node_ptr_t> getNodeConnections(node_ptr_t node) const;

        Result<void, MetaError> setNodePosition(size_t node, s16 x, s16 y, s16 z);
        Result<void, MetaError> setNodePosition(size_t node, const glm::vec3 &pos);
        Result<void, MetaError> setNodePosition(node_ptr_t node, s16 x, s16 y, s16 z);
        Result<void, MetaError> setNodePosition(node_ptr_t node, const glm::vec3 &pos);

        Result<void, MetaError> setNodeFlag(size_t node, u32 flag);
        Result<void, MetaError> setNodeFlag(node_ptr_t node, u32 flag);

        Result<void, MetaError> setNodeValue(size_t node, size_t index, s16 value);
        Result<void, MetaError> setNodeValue(node_ptr_t node, size_t index, s16 value);

        Result<void, MetaError> addConnection(size_t node, size_t to);
        Result<void, MetaError> addConnection(node_ptr_t node, node_ptr_t to);

        Result<void, MetaError> insertConnection(size_t node, size_t index, size_t to);
        Result<void, MetaError> insertConnection(node_ptr_t node, size_t index,
                                                        node_ptr_t to);

        Result<void, MetaError> removeConnection(size_t node, size_t index);
        Result<void, MetaError> removeConnection(node_ptr_t node, size_t index);

        Result<void, MetaError> replaceConnection(size_t node, size_t index, size_t to);
        Result<void, MetaError> replaceConnection(node_ptr_t node, size_t index,
                                                         node_ptr_t to);

        // Destructive connection algorithms

        Result<void, MetaError> connectNodeToNearest(size_t node, size_t count);
        Result<void, MetaError> connectNodeToNearest(node_ptr_t node, size_t count);
        Result<void, MetaError> connectNodeToNearest(size_t node) { return connectNodeToNearest(node, 1);}
        Result<void, MetaError> connectNodeToNearest(node_ptr_t node) { return connectNodeToNearest(node, 1);}

        Result<void, MetaError> connectNodeToPrev(size_t node);
        Result<void, MetaError> connectNodeToPrev(node_ptr_t node);

        Result<void, MetaError> connectNodeToNext(size_t node);
        Result<void, MetaError> connectNodeToNext(node_ptr_t node);

        Result<void, MetaError> connectNodeToNeighbors(size_t node, bool loop_ok);
        Result<void, MetaError> connectNodeToNeighbors(node_ptr_t node, bool loop_ok);

        Result<void, MetaError> connectNodeToReferrers(size_t node);
        Result<void, MetaError> connectNodeToReferrers(node_ptr_t node);

        [[nodiscard]] std::vector<node_ptr_t>::const_iterator begin() const {
            return m_nodes.begin();
        }
        [[nodiscard]] std::vector<node_ptr_t>::const_iterator end() const { return m_nodes.end(); }

        [[nodiscard]] std::vector<node_ptr_t>::iterator begin() { return m_nodes.begin(); }
        [[nodiscard]] std::vector<node_ptr_t>::iterator end() { return m_nodes.end(); }

        [[nodiscard]] node_ptr_t operator[](size_t index) const { return m_nodes[index]; }

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        ScopePtr<ISmartResource> clone(bool deep) const override;

    protected:
        Result<void, MetaError> calcDistancesWithNode(node_ptr_t node);

        void chaikinSubdivide();

    private:
        UUID64 m_UUID64;
        u32 m_sibling_id = 0;
        std::string m_name;
        std::vector<node_ptr_t> m_nodes = {};
    };

}  // namespace Toolbox::Rail