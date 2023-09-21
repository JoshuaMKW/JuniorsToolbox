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
        Rail(std::string_view name) : m_name(name) {}
        Rail(std::string_view name, std::vector<RailNode> nodes) : m_name(name), m_nodes(nodes) {}
        Rail(const Rail &other) = default;
        Rail(Rail &&other)      = default;
        ~Rail()                 = default;

        Rail &operator=(const Rail &other) = default;
        Rail &operator=(Rail &&other)      = default;

        [[nodiscard]] bool isSpline() const { return m_name.starts_with("S_"); }

        [[nodiscard]] std::string_view name() const { return m_name; }
        [[nodiscard]] const std::vector<RailNode> &nodes() const { return m_nodes; }
        [[nodiscard]] std::vector<RailNode> &nodes() { return m_nodes; }

        [[nodiscard]] glm::vec3 getCenteroid() const;
        [[nodiscard]] BoundingBox getBoundingBox() const;

        [[nodiscard]] size_t getDataSize() const { return 68 * m_nodes.size(); }

        void addNode(const RailNode &node);

        std::expected<void, MetaError> insertNode(size_t index, const RailNode &node);

        std::expected<void, MetaError> removeNode(size_t index);
        bool removeNode(const RailNode &node);

        std::expected<void, MetaError> replaceNode(size_t index, const RailNode &node);
        bool replaceNode(const RailNode &oldNode, const RailNode &newNode);

        std::expected<void, MetaError> swapNodes(size_t index1, size_t index2);
        bool swapNodes(const RailNode &node1, const RailNode &node2);

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

        [[nodiscard]] bool isNodeConnectedToOther(size_t node_a, size_t node_b) const;
        [[nodiscard]] bool isNodeConnectedToOther(const RailNode &node_a,
                                                  const RailNode &node_b) const;

        std::optional<size_t> getNodeIndex(const RailNode &node) const;
        std::vector<const RailNode> getNodeConnections(const RailNode &node) const;

        std::expected<void, MetaError> setNodePosition(size_t node, s16 x, s16 y, s16 z);
        std::expected<void, MetaError> setNodePosition(size_t node, const glm::vec3 &pos);
        std::expected<void, MetaError> setNodePosition(const RailNode &node, s16 x, s16 y, s16 z);
        std::expected<void, MetaError> setNodePosition(const RailNode &node, const glm::vec3 &pos);

        std::expected<void, MetaError> setNodeFlag(size_t node, u32 flag);
        std::expected<void, MetaError> setNodeFlag(const RailNode &node, u32 flag);

        std::expected<void, MetaError> setNodeValue(size_t node, size_t index, s16 value);
        std::expected<void, MetaError> setNodeValue(const RailNode &node, size_t index, s16 value);

        std::expected<void, MetaError> addConnection(size_t node, size_t to);
        std::expected<void, MetaError> addConnection(const RailNode &node, const RailNode &to);

        std::expected<void, MetaError> insertConnection(size_t node, size_t index, size_t to);
        std::expected<void, MetaError> insertConnection(const RailNode &node, size_t index,
                                                        const RailNode &to);

        std::expected<void, MetaError> removeConnection(size_t node, size_t index);
        std::expected<void, MetaError> removeConnection(const RailNode &node, size_t index);

        std::expected<void, MetaError> replaceConnection(size_t node, size_t index, size_t to);
        std::expected<void, MetaError> replaceConnection(const RailNode &node, size_t index,
                                                         const RailNode &to);

        // Destructive connection algorithms

        std::expected<void, MetaError> connectNodeToNearest(size_t node, size_t count);
        std::expected<void, MetaError> connectNodeToNearest(const RailNode &node, size_t count);
        std::expected<void, MetaError> connectNodeToNearest(size_t node) {
            return connectNodeToNearest(node, 1);
        }
        std::expected<void, MetaError> connectNodeToNearest(const RailNode &node) {
            return connectNodeToNearest(node, 1);
        }

        std::expected<void, MetaError> connectNodeToPrev(size_t node);
        std::expected<void, MetaError> connectNodeToPrev(const RailNode &node);

        std::expected<void, MetaError> connectNodeToNext(size_t node);
        std::expected<void, MetaError> connectNodeToNext(const RailNode &node);

        std::expected<void, MetaError> connectNodeToNeighbors(size_t node);
        std::expected<void, MetaError> connectNodeToNeighbors(const RailNode &node);

        std::expected<void, MetaError> connectNodeToReferrers(size_t node);
        std::expected<void, MetaError> connectNodeToReferrers(const RailNode &node);

        [[nodiscard]] std::vector<RailNode>::const_iterator begin() const {
            return m_nodes.begin();
        }
        [[nodiscard]] std::vector<RailNode>::const_iterator end() const { return m_nodes.end(); }

        [[nodiscard]] std::vector<RailNode>::iterator begin() { return m_nodes.begin(); }
        [[nodiscard]] std::vector<RailNode>::iterator end() { return m_nodes.end(); }

        [[nodiscard]] const RailNode &operator[](size_t index) const { return m_nodes[index]; }
        [[nodiscard]] RailNode &operator[](size_t index) { return m_nodes[index]; }

        std::unique_ptr<IClonable> clone(bool deep) const override;

    private:
        std::string m_name;
        std::vector<RailNode> m_nodes = {};
    };

}  // namespace Toolbox::Rail