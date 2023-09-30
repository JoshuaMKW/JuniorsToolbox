#include "rail/rail.hpp"
#include "rail/node.hpp"

#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"

using namespace Toolbox::Object;

namespace Toolbox::Rail {
    void Rail::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        out << std::string(indention * indention_width, ' ') << "Rail: " << m_name << std::endl;
        for (const auto &node : m_nodes) {
            node->dump(out, indention + 1, indention_width);
        }
    }

    std::unique_ptr<IClonable> Rail::clone(bool deep) const {
        auto clone = std::make_unique<Rail>(m_name);

        if (deep) {
            for (const auto &node : m_nodes) {
                clone->m_nodes.push_back(make_deep_clone<RailNode>(node));
            }
        } else {
            clone->m_nodes.clear();
            for (auto &node : m_nodes) {
                clone->m_nodes.push_back(node);
            }
        }

        return clone;
    }

    glm::vec3 Rail::getCenteroid() const {
        size_t node_count = m_nodes.size();
        if (node_count == 0)
            return glm::vec3();
        glm::vec3 accum =
            std::accumulate(m_nodes.begin(), m_nodes.end(), glm::vec3(),
                            [](glm::vec3 a, node_ptr_t node) { return a + node->getPosition(); });
        return accum / static_cast<float>(node_count);
    }

    Rail &Rail::translate(const glm::vec3 &t) {
        for (auto &node : m_nodes) {
            node->setPosition(t);
        }
        return *this;
    }

    Rail &Rail::rotate(const glm::quat &r) {
        glm::vec3 center = getCenteroid();
        for (auto &node : m_nodes) {
            glm::vec3 rotatedPos = r * (node->getPosition() - center) + center;
            node->setPosition(rotatedPos);
        }
    }

    Rail &Rail::scale(const glm::vec3 &s) {
        glm::vec3 center = getCenteroid();
        for (auto &node : m_nodes) {
            glm::vec3 scaledPos = s * (node->getPosition() - center) + center;
            node->setPosition(scaledPos);
        }
    }

    Rail &Rail::invert(bool x, bool y, bool z) {
        if (!x && !y && !z)
            return *this;
        glm::vec3 center = getCenteroid();
        for (auto &node : m_nodes) {
            glm::vec3 normalPos = node->getPosition() - center;
            if (x)
                normalPos.x = -normalPos.x;
            if (y)
                normalPos.y = -normalPos.y;
            if (z)
                normalPos.z = -normalPos.z;
            node->setPosition(normalPos + center);
        }
    }

    void Rail::addNode(node_ptr_t node) {
        node->m_rail = this;
        m_nodes.push_back(node);
    }

    std::expected<void, MetaError> Rail::insertNode(size_t index, node_ptr_t node) {
        if (index > m_nodes.size()) {
            return make_meta_error<void>("Error inserting node", index, m_nodes.size());
        }
        node->m_rail = this;
        m_nodes.insert(m_nodes.begin() + index, node);
        return {};
    }

    std::expected<void, MetaError> Rail::removeNode(size_t index) {
        if (index >= m_nodes.size()) {
            return make_meta_error<void>("Error removing node", index, m_nodes.size());
        }
        m_nodes.erase(m_nodes.begin() + index);
        return {};
    }

    bool Rail::removeNode(node_ptr_t node) {
        auto it = std::find(m_nodes.begin(), m_nodes.end(), node);
        if (it == m_nodes.end()) {
            return false;
        }
        (*it)->m_rail = nullptr;
        m_nodes.erase(it);
        return true;
    }

    std::expected<void, MetaError> Rail::swapNodes(size_t index1, size_t index2) {
        if (index1 >= m_nodes.size()) {
            return make_meta_error<void>("Error swapping node (1)", index1, m_nodes.size());
        }

        if (index2 >= m_nodes.size()) {
            return make_meta_error<void>("Error swapping node (2)", index2, m_nodes.size());
        }

        std::iter_swap(m_nodes.begin() + index1, m_nodes.begin() + index2);
        return {};
    }

    bool Rail::swapNodes(node_ptr_t node1, node_ptr_t node2) {
        auto it1 = std::find(m_nodes.begin(), m_nodes.end(), node1);
        if (it1 == m_nodes.end()) {
            return false;
        }

        auto it2 = std::find(m_nodes.begin(), m_nodes.end(), node2);
        if (it2 == m_nodes.end()) {
            return false;
        }

        std::iter_swap(it1, it2);
        return true;
    }

    bool Rail::isNodeConnectedToOther(size_t node_a, size_t node_b) const {
        if (node_a >= m_nodes.size() || node_b >= m_nodes.size()) {
            return false;
        }

        return isNodeConnectedToOther(m_nodes[node_a], m_nodes[node_b]);
    }

    bool Rail::isNodeConnectedToOther(node_ptr_t node_a, node_ptr_t node_b) const {
        if (node_a == node_b) {
            return false;
        }

        for (size_t i = 0; i < node_a->getConnectionCount(); ++i) {
            if (node_a->getConnectionValue(i).value() == getNodeIndex(node_b)) {
                return true;
            }
        }
    }

    std::optional<size_t> Rail::getNodeIndex(node_ptr_t node) const {
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes[i] == node) {
                return i;
            }
        }
        return {};
    }

    std::vector<Rail::node_ptr_t> Rail::getNodeConnections(size_t node) const {
        if (node >= m_nodes.size()) {
            return {};
        }
        return getNodeConnections(m_nodes[node]);
    }

    std::vector<Rail::node_ptr_t> Rail::getNodeConnections(node_ptr_t node) const {
        std::vector<Rail::node_ptr_t> connections;
        for (size_t i = 0; i < node->getConnectionCount(); ++i) {
            connections.push_back(m_nodes[node->getConnectionValue(i).value()]);
        }
        return connections;
    }

    std::expected<void, MetaError> Rail::setNodePosition(size_t node, s16 x, s16 y, s16 z) {
        return setNodePosition(node, glm::vec3(x, y, z));
    }

    std::expected<void, MetaError> Rail::setNodePosition(size_t node, const glm::vec3 &pos) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error setting node position", node, m_nodes.size());
        }

        return setNodePosition(m_nodes[node], pos);
    }

    std::expected<void, MetaError> Rail::setNodePosition(node_ptr_t node, s16 x, s16 y, s16 z) {
        node->setPosition(glm::vec3(x, y, z));
        return {};
    }

    std::expected<void, MetaError> Rail::setNodePosition(node_ptr_t node, const glm::vec3 &pos) {
        node->setPosition(pos);
        return {};
    }

    std::expected<void, MetaError> Rail::setNodeFlag(size_t node, u32 flag) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error setting node flag", node, m_nodes.size());
        }
        return setNodeFlag(m_nodes[node], flag);
    }

    std::expected<void, MetaError> Rail::setNodeFlag(node_ptr_t node, u32 flag) {
        node->setFlags(flag);
        return {};
    }

    std::expected<void, MetaError> Rail::setNodeValue(size_t node, size_t index, s16 value) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error setting node value", node, m_nodes.size());
        }
        return setNodeValue(m_nodes[node], index, value);
    }

    std::expected<void, MetaError> Rail::setNodeValue(node_ptr_t node, size_t index, s16 value) {
        return node->setValue(static_cast<int>(index), value);
    }

    std::expected<void, MetaError> Rail::addConnection(size_t node, size_t to) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error adding connection (from)", node, m_nodes.size());
        }
        if (to >= m_nodes.size()) {
            return make_meta_error<void>("Error adding connection (to)", to, m_nodes.size());
        }
        return addConnection(m_nodes[node], m_nodes[to]);
    }

    std::expected<void, MetaError> Rail::addConnection(node_ptr_t node, node_ptr_t to) {
        auto connectionCount = node->getConnectionCount();
        if (connectionCount >= 8) {
            return make_meta_error<void>("Error adding connection (max)", connectionCount, 8);
        }
        node->setConnectionCount(connectionCount + 1);
        node->setConnectionValue(connectionCount, getNodeIndex(to).value());
    }

    std::expected<void, MetaError> Rail::insertConnection(size_t node, size_t index, size_t to) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error inserting connection (from)", node, m_nodes.size());
        }
        if (to >= m_nodes.size()) {
            return make_meta_error<void>("Error inserting connection (to)", to, m_nodes.size());
        }
        return insertConnection(m_nodes[node], index, m_nodes[to]);
    }

    std::expected<void, MetaError> Rail::insertConnection(node_ptr_t node, size_t index,
                                                          node_ptr_t to) {
        auto connectionCount = node->getConnectionCount();
        if (connectionCount >= 8) {
            return make_meta_error<void>("Error adding connection (max)", connectionCount, 8);
        }
        node->setConnectionCount(connectionCount + 1);
        for (size_t i = connectionCount; i > index; --i) {
            auto result = node->setConnectionValue(i, node->getConnectionValue(i - 1).value());
            if (!result) {
                return result;
            }
        }
        return node->setConnectionValue(index, getNodeIndex(to).value());
    }

    std::expected<void, MetaError> Rail::removeConnection(size_t node, size_t index) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error removing connection (from)", node, m_nodes.size());
        }
        return removeConnection(m_nodes[node], index);
    }

    std::expected<void, MetaError> Rail::removeConnection(node_ptr_t node, size_t index) {
        auto connectionCount = node->getConnectionCount();
        if (index >= connectionCount) {
            return make_meta_error<void>("Error removing connection (index)", index,
                                         connectionCount);
        }
        for (size_t i = index; i < connectionCount - 1; ++i) {
            auto result = node->setConnectionValue(i, node->getConnectionValue(i + 1).value());
            if (!result) {
                return result;
            }
        }
        node->setConnectionCount(connectionCount - 1);
        return {};
    }

    std::expected<void, MetaError> Rail::replaceConnection(size_t node, size_t index, size_t to) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error replacing connection (from)", node, m_nodes.size());
        }
        if (to >= m_nodes.size()) {
            return make_meta_error<void>("Error replacing connection (to)", to, m_nodes.size());
        }
        return replaceConnection(m_nodes[node], index, m_nodes[to]);
    }

    std::expected<void, MetaError> Rail::replaceConnection(node_ptr_t node, size_t index,
                                                           node_ptr_t to) {
        if (index >= node->getConnectionCount()) {
            return make_meta_error<void>("Error replacing connection (index)", index,
                                         node->getConnectionCount());
        }
        return node->setConnectionValue(index, getNodeIndex(to).value());
    }

}  // namespace Toolbox::Rail