#pragma once

#include "boundbox.hpp"
#include "clone.hpp"
#include "node.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "serial.hpp"
#include <string>
#include <vector>

using namespace Toolbox::Object;

namespace Toolbox::Rail {

    class Rail : public IClonable {
    public:
        using node_ptr_t = std::shared_ptr<RailNode>;

        Rail() = delete;
        explicit Rail(std::string_view name) : m_name(name) {}
        Rail(std::string_view name, std::vector<node_ptr_t> nodes) : m_name(name), m_nodes(nodes) {}
        Rail(const Rail &other) = default;
        Rail(Rail &&other)      = default;

        ~Rail() = default;

        Rail &operator=(const Rail &other) = default;
        Rail &operator=(Rail &&other)      = default;

        [[nodiscard]] bool isSpline() const { return m_name.starts_with("S_"); }

        [[nodiscard]] std::string_view name() const { return m_name; }
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

        std::expected<void, MetaError> insertNode(size_t index, node_ptr_t node);

        std::expected<void, MetaError> removeNode(size_t index);
        bool removeNode(node_ptr_t node);

        std::expected<void, MetaError> swapNodes(size_t index1, size_t index2);
        bool swapNodes(node_ptr_t node1, node_ptr_t node2);

        [[nodiscard]] bool isNodeConnectedToOther(size_t node_a, size_t node_b) const;
        [[nodiscard]] bool isNodeConnectedToOther(node_ptr_t node_a,
                                                  node_ptr_t node_b) const;

        std::optional<size_t> getNodeIndex(node_ptr_t node) const;
        std::vector<const RailNode> getNodeConnections(node_ptr_t node) const;

        std::expected<void, MetaError> setNodePosition(size_t node, s16 x, s16 y, s16 z);
        std::expected<void, MetaError> setNodePosition(size_t node, const glm::vec3 &pos);
        std::expected<void, MetaError> setNodePosition(node_ptr_t node, s16 x, s16 y, s16 z);
        std::expected<void, MetaError> setNodePosition(node_ptr_t node, const glm::vec3 &pos);

        std::expected<void, MetaError> setNodeFlag(size_t node, u32 flag);
        std::expected<void, MetaError> setNodeFlag(node_ptr_t node, u32 flag);

        std::expected<void, MetaError> setNodeValue(size_t node, size_t index, s16 value);
        std::expected<void, MetaError> setNodeValue(node_ptr_t node, size_t index, s16 value);

        std::expected<void, MetaError> addConnection(size_t node, size_t to);
        std::expected<void, MetaError> addConnection(node_ptr_t node, node_ptr_t to);

        std::expected<void, MetaError> insertConnection(size_t node, size_t index, size_t to);
        std::expected<void, MetaError> insertConnection(node_ptr_t node, size_t index,
                                                        node_ptr_t to);

        std::expected<void, MetaError> removeConnection(size_t node, size_t index);
        std::expected<void, MetaError> removeConnection(node_ptr_t node, size_t index);

        std::expected<void, MetaError> replaceConnection(size_t node, size_t index, size_t to);
        std::expected<void, MetaError> replaceConnection(node_ptr_t node, size_t index,
                                                         node_ptr_t to);

        // Destructive connection algorithms

        std::expected<void, MetaError> connectNodeToNearest(size_t node, size_t count);
        std::expected<void, MetaError> connectNodeToNearest(node_ptr_t node, size_t count);
        std::expected<void, MetaError> connectNodeToNearest(size_t node) {
            return connectNodeToNearest(node, 1);
        }
        std::expected<void, MetaError> connectNodeToNearest(node_ptr_t node) {
            return connectNodeToNearest(node, 1);
        }

        std::expected<void, MetaError> connectNodeToPrev(size_t node);
        std::expected<void, MetaError> connectNodeToPrev(node_ptr_t node);

        std::expected<void, MetaError> connectNodeToNext(size_t node);
        std::expected<void, MetaError> connectNodeToNext(node_ptr_t node);

        std::expected<void, MetaError> connectNodeToNeighbors(size_t node);
        std::expected<void, MetaError> connectNodeToNeighbors(node_ptr_t node);

        std::expected<void, MetaError> connectNodeToReferrers(size_t node);
        std::expected<void, MetaError> connectNodeToReferrers(node_ptr_t node);

        [[nodiscard]] std::vector<node_ptr_t>::const_iterator begin() const {
            return m_nodes.begin();
        }
        [[nodiscard]] std::vector<node_ptr_t>::const_iterator end() const { return m_nodes.end(); }

        [[nodiscard]] std::vector<node_ptr_t>::iterator begin() { return m_nodes.begin(); }
        [[nodiscard]] std::vector<node_ptr_t>::iterator end() { return m_nodes.end(); }

        [[nodiscard]] node_ptr_t operator[](size_t index) const { return m_nodes[index]; }

        std::unique_ptr<IClonable> clone(bool deep) const override;

    private:
        std::string m_name;
        std::vector<node_ptr_t> m_nodes = {};
    };

}  // namespace Toolbox::Rail