#include <unordered_set>

#include "rail/node.hpp"
#include "rail/rail.hpp"

#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

using namespace Toolbox::Object;

static bool isTranslationOnly(const glm::mat4 &m, float epsilon = 0.0001f) {
    // A Translation-Only matrix looks like this:
    // 1  0  0  Tx
    // 0  1  0  Ty
    // 0  0  1  Tz
    // 0  0  0  1

    if (!glm::all(glm::epsilonEqual(m[0], glm::vec4(1.f, 0.f, 0.f, 0.f), epsilon)))
        return false;

    if (!glm::all(glm::epsilonEqual(m[1], glm::vec4(0.f, 1.f, 0.f, 0.f), epsilon)))
        return false;

    if (!glm::all(glm::epsilonEqual(m[2], glm::vec4(0.f, 0.f, 1.f, 0.f), epsilon)))
        return false;

    if (glm::abs(m[3].w - 1.0f) > epsilon)
        return false;

    return true;
}

namespace Toolbox::Rail {

    Result<void, SerialError> Rail::serialize(Serializer &out) const {
        out.writeString<std::endian::big>(m_name);
        out.write<u16, std::endian::big>(static_cast<u16>(m_nodes.size()));
        for (const auto &node : m_nodes) {
            auto result = node->serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }
        return {};
    }

    Result<void, SerialError> Rail::deserialize(Deserializer &in) {
        m_name = in.readString<std::endian::big>();
        m_nodes.clear();
        u16 node_count = in.read<u16, std::endian::big>();
        for (u16 i = 0; i < node_count; ++i) {
            auto node   = make_referable<RailNode>();
            auto result = node->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            addNode(node);
        }
        return {};
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

    Rail &Rail::transform(const glm::mat4 &delta_matrix) {
        if (isTranslationOnly(delta_matrix)) {
            for (auto &node : m_nodes) {
                glm::vec3 pos = node->getPosition();
                pos += glm::vec3(delta_matrix[3]);
                node->setPosition(pos);
            }
            return *this;
        }

        glm::vec3 center = getCenteroid();

        glm::mat4 T_inv = glm::translate(glm::mat4(1.0f), -center);
        glm::mat4 T     = glm::translate(glm::mat4(1.0f), center);

        glm::mat4 rs_mtx = delta_matrix;
        rs_mtx[3]        = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        glm::mat4 final_transform = T * rs_mtx * T_inv;

        for (auto &node : m_nodes) {
            glm::vec4 original_pos = glm::vec4(node->getPosition(), 1.0f);
            glm::vec4 new_pos      = final_transform * original_pos;
            node->setPosition(glm::vec3(new_pos));
        }

        return *this;
    }

    Rail &Rail::translate(const glm::vec3 &t) {
        for (auto &node : m_nodes) {
            glm::vec3 pos = node->getPosition();
            node->setPosition(pos + t);
        }
        return *this;
    }

    Rail &Rail::rotate(const glm::quat &r) {
        glm::vec3 center = getCenteroid();
        for (auto &node : m_nodes) {
            glm::vec3 rotatedPos = r * (node->getPosition() - center) + center;
            node->setPosition(rotatedPos);
        }
        return *this;
    }

    Rail &Rail::scale(const glm::vec3 &s) {
        glm::vec3 center = getCenteroid();
        for (auto &node : m_nodes) {
            glm::vec3 scaledPos = s * (node->getPosition() - center) + center;
            node->setPosition(scaledPos);
        }
        return *this;
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
        return *this;
    }

    void Rail::decimate(size_t iterations) {
        for (size_t i = 0; i < iterations; ++i) {
            decimateImpl();
        }
    }

    void Rail::subdivide(size_t iterations) {
        for (size_t i = 0; i < iterations; ++i) {
            chaikinSubdivide();
        }
    }

    void Rail::addNode(node_ptr_t node) {
        node->m_rail_uuid = getUUID();
        m_nodes.push_back(node);
    }

    Result<void, MetaError> Rail::insertNode(size_t index, node_ptr_t node) {
        if (index > m_nodes.size()) {
            return make_meta_error<void>("Error inserting node", index, m_nodes.size());
        }
        node->m_rail_uuid = getUUID();
        m_nodes.insert(m_nodes.begin() + index, node);
        return {};
    }

    Result<void, MetaError> Rail::removeNode(size_t index) {
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
        (*it)->m_rail_uuid = 0;
        m_nodes.erase(it);
        return true;
    }

    Result<void, MetaError> Rail::swapNodes(size_t index1, size_t index2) {
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

        return false;
    }

    std::optional<size_t> Rail::getNodeIndex(node_ptr_t node) const {
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes[i] == node) {
                return i;
            }
        }
        return {};
    }

    Rail::node_ptr_t Rail::getNodeConnection(size_t node, size_t slot) const {
        if (node >= m_nodes.size()) {
            return {};
        }
        return getNodeConnection(m_nodes[node], slot);
    }

    Rail::node_ptr_t Rail::getNodeConnection(node_ptr_t node, size_t slot) const {
        if (slot >= node->getConnectionCount()) {
            return nullptr;
        }

        size_t node_index = static_cast<size_t>(node->getConnectionValue(slot).value());
        if (node_index >= m_nodes.size()) {
            return nullptr;
        }

        return m_nodes[node_index];
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
            size_t node_index = static_cast<size_t>(node->getConnectionValue(i).value());
            if (node_index < m_nodes.size())
                connections.push_back(m_nodes[node->getConnectionValue(i).value()]);
        }
        return connections;
    }

    Result<void, MetaError> Rail::setNodePosition(size_t node, s16 x, s16 y, s16 z) {
        return setNodePosition(node, glm::vec3(x, y, z));
    }

    Result<void, MetaError> Rail::setNodePosition(size_t node, const glm::vec3 &pos) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error setting node position", node, m_nodes.size());
        }

        return setNodePosition(m_nodes[node], pos);
    }

    Result<void, MetaError> Rail::setNodePosition(node_ptr_t node, s16 x, s16 y, s16 z) {
        node->setPosition(glm::vec3(x, y, z));
        return calcDistancesWithNode(node);
    }

    Result<void, MetaError> Rail::setNodePosition(node_ptr_t node, const glm::vec3 &pos) {
        node->setPosition(pos);
        return calcDistancesWithNode(node);
    }

    Result<void, MetaError> Rail::setNodeFlag(size_t node, u32 flag) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error setting node flag", node, m_nodes.size());
        }
        return setNodeFlag(m_nodes[node], flag);
    }

    Result<void, MetaError> Rail::setNodeFlag(node_ptr_t node, u32 flag) {
        node->setFlags(flag);
        return {};
    }

    Result<void, MetaError> Rail::setNodeValue(size_t node, size_t index, s16 value) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error setting node value", node, m_nodes.size());
        }
        return setNodeValue(m_nodes[node], index, value);
    }

    Result<void, MetaError> Rail::setNodeValue(node_ptr_t node, size_t index, s16 value) {
        return node->setValue(static_cast<int>(index), value);
    }

    Result<void, MetaError> Rail::addConnection(size_t node, size_t to) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error adding connection (from)", node, m_nodes.size());
        }
        if (to >= m_nodes.size()) {
            return make_meta_error<void>("Error adding connection (to)", to, m_nodes.size());
        }
        return addConnection(m_nodes[node], m_nodes[to]);
    }

    Result<void, MetaError> Rail::addConnection(node_ptr_t node, node_ptr_t to) {
        auto connectionCount = node->getConnectionCount();
        if (connectionCount >= 8) {
            return make_meta_error<void>("Error adding connection (max)", connectionCount, 8);
        }
        node->setConnectionCount(connectionCount + 1);
        auto result =
            node->setConnectionValue(connectionCount, static_cast<s16>(getNodeIndex(to).value()));
        if (!result) {
            return result;
        }
        f32 distance = glm::distance(node->getPosition(), to->getPosition());
        node->setConnectionDistance(connectionCount, distance);
        return {};
    }

    Result<void, MetaError> Rail::insertConnection(size_t node, size_t index, size_t to) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error inserting connection (from)", node, m_nodes.size());
        }
        if (to >= m_nodes.size()) {
            return make_meta_error<void>("Error inserting connection (to)", to, m_nodes.size());
        }
        return insertConnection(m_nodes[node], index, m_nodes[to]);
    }

    Result<void, MetaError> Rail::insertConnection(node_ptr_t node, size_t index, node_ptr_t to) {
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
        auto result = node->setConnectionValue(index, static_cast<s16>(getNodeIndex(to).value()));
        if (!result) {
            return result;
        }
        f32 distance = glm::distance(node->getPosition(), to->getPosition());
        return node->setConnectionDistance(index, distance);
    }

    Result<void, MetaError> Rail::removeConnection(size_t node, size_t index) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error removing connection (from)", node, m_nodes.size());
        }
        return removeConnection(m_nodes[node], index);
    }

    Result<void, MetaError> Rail::removeConnection(node_ptr_t node, size_t index) {
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

    Result<void, MetaError> Rail::replaceConnection(size_t node, size_t index, size_t to) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error replacing connection (from)", node, m_nodes.size());
        }
        if (to >= m_nodes.size()) {
            return make_meta_error<void>("Error replacing connection (to)", to, m_nodes.size());
        }
        return replaceConnection(m_nodes[node], index, m_nodes[to]);
    }

    Result<void, MetaError> Rail::replaceConnection(node_ptr_t node, size_t index, node_ptr_t to) {
        if (index >= node->getConnectionCount()) {
            return make_meta_error<void>("Error replacing connection (index)", index,
                                         node->getConnectionCount());
        }
        auto result = node->setConnectionValue(index, static_cast<s16>(getNodeIndex(to).value()));
        if (!result) {
            return result;
        }
        f32 distance = glm::distance(node->getPosition(), to->getPosition());
        return node->setConnectionDistance(index, distance);
    }

    Result<void, MetaError> Rail::connectNodeToNearest(size_t node, size_t count) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error connecting node to nearest", node, m_nodes.size());
        }
        return connectNodeToNearest(m_nodes[node], count);
    }

    Result<void, MetaError> Rail::connectNodeToNearest(node_ptr_t node, size_t count) {
        std::vector<std::pair<f32, node_ptr_t>> nearest_nodes;
        for (auto &n : m_nodes) {
            if (n == node) {
                continue;
            }
            f32 distance = glm::distance(node->getPosition(), n->getPosition());
            nearest_nodes.emplace_back(distance, n);
        }
        std::sort(nearest_nodes.begin(), nearest_nodes.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });

        node->setConnectionCount(static_cast<u16>(count));
        for (size_t i = 0; i < count && i < nearest_nodes.size(); ++i) {
            auto result = node->setConnectionValue(
                i, static_cast<u16>(getNodeIndex(nearest_nodes[i].second).value()));
            if (!result) {
                return result;
            }
            f32 distance =
                glm::distance(node->getPosition(), nearest_nodes[i].second->getPosition());
            node->setConnectionDistance(i, distance);
        }
        return {};
    }

    Result<void, MetaError> Rail::connectNodeToPrev(size_t node) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error connecting node to prev", node, m_nodes.size());
        }
        return connectNodeToPrev(m_nodes[node]);
    }

    Result<void, MetaError> Rail::connectNodeToPrev(node_ptr_t node) {
        auto result = getNodeIndex(node);
        if (!result) {
            return make_meta_error<void>("Error connecting node to prev (not from rail)",
                                         std::numeric_limits<size_t>::max(), 0);
        }
        auto node_index = result.value();
        if (node_index == 0) {
            return make_meta_error<void>("Error connecting node to prev (first node)",
                                         std::numeric_limits<size_t>::max(), 0);
        }
        node->setConnectionCount(1);
        {
            auto vresult = node->setConnectionValue(0, static_cast<s16>(node_index) - 1);
            if (!vresult) {
                return vresult;
            }
            auto distance =
                glm::distance(node->getPosition(), m_nodes[node_index - 1]->getPosition());
            node->setConnectionDistance(0, distance);
        }
        return {};
    }

    Result<void, MetaError> Rail::connectNodeToNext(size_t node) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error connecting node to next", node, m_nodes.size());
        }
        return connectNodeToNext(m_nodes[node]);
    }

    Result<void, MetaError> Rail::connectNodeToNext(node_ptr_t node) {
        auto result = getNodeIndex(node);
        if (!result) {
            return make_meta_error<void>("Error connecting node to next (not from rail)",
                                         std::numeric_limits<size_t>::max(), 0);
        }
        auto node_index = result.value();
        if (node_index == m_nodes.size() - 1) {
            return make_meta_error<void>("Error connecting node to next (last node)",
                                         node_index + 1, m_nodes.size());
        }
        node->setConnectionCount(1);
        {
            auto vresult = node->setConnectionValue(0, static_cast<s16>(node_index) + 1);
            if (!vresult) {
                return vresult;
            }
            auto distance =
                glm::distance(node->getPosition(), m_nodes[node_index + 1]->getPosition());
            node->setConnectionDistance(0, distance);
        }
        return {};
    }

    Result<void, MetaError> Rail::connectNodeToNeighbors(size_t node, bool loop_ok) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error connecting node to neighbors", node,
                                         m_nodes.size());
        }
        return connectNodeToNeighbors(m_nodes[node], loop_ok);
    }

    Result<void, MetaError> Rail::connectNodeToNeighbors(node_ptr_t node, bool loop_ok) {
        auto result = getNodeIndex(node);
        if (!result) {
            return make_meta_error<void>("Error connecting node to neighbors (not from rail)",
                                         std::numeric_limits<size_t>::max(), 0);
        }
        auto node_index = result.value();
        if (node_index == 0 && !loop_ok) {
            return make_meta_error<void>("Error connecting node to neighbors (first node)",
                                         std::numeric_limits<size_t>::max(), 0);
        }
        if (node_index == m_nodes.size() - 1 && !loop_ok) {
            return make_meta_error<void>("Error connecting node to neighbors (last node)",
                                         node_index + 1, m_nodes.size());
        }

        s16 last_node_index = static_cast<s16>(m_nodes.size()) - 1;
        s16 prev_node_index = node_index == 0 ? last_node_index : static_cast<s16>(node_index) - 1;
        s16 prev_connection_index = node_index == 0 ? 1 : 0;
        s16 next_node_index = node_index == last_node_index ? 0 : static_cast<s16>(node_index) + 1;
        s16 next_connection_index = node_index == 0 ? 0 : 1;

        node->setConnectionCount(2);
        {
            auto vresult = node->setConnectionValue(prev_connection_index, prev_node_index);
            if (!vresult) {
                return vresult;
            }
            auto distance =
                glm::distance(node->getPosition(), m_nodes[prev_node_index]->getPosition());
            node->setConnectionDistance(prev_connection_index, distance);
        }

        {
            auto vresult = node->setConnectionValue(next_connection_index, next_node_index);
            if (!vresult) {
                return vresult;
            }
            auto distance =
                glm::distance(node->getPosition(), m_nodes[next_node_index]->getPosition());
            node->setConnectionDistance(next_connection_index, distance);
        }

        return {};
    }

    Result<void, MetaError> Rail::connectNodeToReferrers(size_t node) {
        if (node >= m_nodes.size()) {
            return make_meta_error<void>("Error connecting node to referrers", node,
                                         m_nodes.size());
        }
        return connectNodeToReferrers(m_nodes[node]);
    }

    Result<void, MetaError> Rail::connectNodeToReferrers(node_ptr_t node) {
        auto result = getNodeIndex(node);
        if (!result) {
            return make_meta_error<void>("Error connecting node to referrers (not from rail)",
                                         std::numeric_limits<size_t>::max(), 0);
        }
        size_t node_index = result.value();
        std::vector<node_ptr_t> referrers;
        for (auto &n : m_nodes) {
            for (size_t i = 0; i < n->getConnectionCount(); ++i) {
                if (n->getConnectionValue(i) == getNodeIndex(node).value()) {
                    referrers.push_back(n);
                    break;
                }
            }
        }
        node->setConnectionCount(static_cast<u16>(referrers.size()));
        for (size_t i = 0; i < referrers.size(); ++i) {
            auto result =
                node->setConnectionValue(i, static_cast<s16>(getNodeIndex(referrers[i]).value()));
            if (!result) {
                return result;
            }
            auto distance = glm::distance(node->getPosition(), referrers[i]->getPosition());
            node->setConnectionDistance(i, distance);
        }
        return {};
    }

    void Rail::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        out << std::string(indention * indention_width, ' ') << "Rail: " << m_name << std::endl;
        for (const auto &node : m_nodes) {
            node->dump(out, indention + 1, indention_width);
        }
    }

    ScopePtr<ISmartResource> Rail::clone(bool deep) const {
        auto clone = make_scoped<Rail>(name());

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

    int Rail::getSlotForNodeConnection(node_ptr_t src, node_ptr_t conn) {
        int conn_index = 0;
        for (node_ptr_t node : m_nodes) {
            if (node == conn) {
                break;
            }
            conn_index += 1;
        }

        for (int slot = 0; slot < (int)src->getConnectionCount(); ++slot) {
            if (src->getConnectionValue(slot).value() == conn_index) {
                return slot;
            }
        }
        return -1;
    }

    Result<void, MetaError> Rail::calcDistancesWithNode(node_ptr_t node) {
        const s16 node_index = static_cast<s16>(
            std::distance(m_nodes.begin(), std::find(m_nodes.begin(), m_nodes.end(), node)));

        for (u16 i = 0; i < node->getConnectionCount(); ++i) {
            auto result = node->getConnectionValue(i);
            if (!result) {
                return std::unexpected(result.error());
            }
            node_ptr_t other = m_nodes[result.value()];
            f32 distance     = glm::distance(other->getPosition(), node->getPosition());
            other->setConnectionDistance(i, distance);
        }

        for (auto &other : m_nodes) {
            for (u16 i = 0; i < other->getConnectionCount(); ++i) {
                auto result = other->getConnectionValue(i);
                if (!result) {
                    return std::unexpected(result.error());
                }
                if (result.value() == node_index) {
                    f32 distance = glm::distance(other->getPosition(), node->getPosition());
                    other->setConnectionDistance(i, distance);
                }
            }
        }
        return {};
    }

    void Rail::decimateImpl() {
        if (m_nodes.size() <= 2) {
            return;
        }

        // We check if the last node connects to the first node to determine if it's a loop
        bool is_closed_loop = isNodeConnectedToOther(m_nodes.back(), m_nodes.front());

        // Select the Survivors (Double Buffer Approach)
        std::vector<node_ptr_t> new_nodes;
        new_nodes.reserve((m_nodes.size() / 2) + 2);

        // Always keep the first node
        new_nodes.push_back(m_nodes[0]);

        // Iterate through the middle, skipping every other node
        for (size_t i = 2; i < m_nodes.size(); i += 2) {
            // If this is an open ended rail, and we are at the very last node,
            // we handle it specifically after the loop to ensure we don't duplicate it
            // or miss it due to the stride.
            if (!is_closed_loop && i == m_nodes.size() - 1) {
                break;
            }
            new_nodes.push_back(m_nodes[i]);
        }

        // Handle endpoint preservation
        if (!is_closed_loop) {
            node_ptr_t last_original = m_nodes.back();
            node_ptr_t last_added    = new_nodes.back();

            // This prevents the rail from shrinking.
            if (last_added != last_original) {
                new_nodes.push_back(last_original);
            }
        }

        if (new_nodes.size() < 2) {
            return;
        }

        // Reset connections because the old indices are now invalid.
        for (size_t i = 0; i < new_nodes.size(); ++i) {
            node_ptr_t current = new_nodes[i];

            // Wipe old connections
            current->setConnectionCount(0);

            // Connect to Previous (i-1)
            if (i > 0) {
                node_ptr_t prev = new_nodes[i - 1];
                f32 dist        = glm::distance(current->getPosition(), prev->getPosition());

                // Standard slot 0 for Previous
                current->setConnectionCount(current->getConnectionCount() + 1);
                current->setConnectionValue(0, static_cast<s16>(i - 1));
                current->setConnectionDistance(0, dist);
            }

            // Connect to next (i+1)
            if (i < new_nodes.size() - 1) {
                node_ptr_t next = new_nodes[i + 1];
                f32 dist        = glm::distance(current->getPosition(), next->getPosition());

                // Standard slot 1 for Next
                u16 slot = current->getConnectionCount();
                current->setConnectionCount(slot + 1);
                current->setConnectionValue(slot, static_cast<s16>(i + 1));
                current->setConnectionDistance(slot, dist);
            }
        }

        // Reclose the loop (if originally closed)
        if (is_closed_loop) {
            node_ptr_t first = new_nodes.front();
            node_ptr_t last  = new_nodes.back();
            f32 dist         = glm::distance(first->getPosition(), last->getPosition());

            // Link Last -> First
            u16 l_slot = last->getConnectionCount();
            last->setConnectionCount(l_slot + 1);
            last->setConnectionValue(l_slot, 0);
            last->setConnectionDistance(l_slot, dist);

            // Link First -> Last
            u16 f_slot = first->getConnectionCount();
            first->setConnectionCount(f_slot + 1);
            first->setConnectionValue(f_slot, static_cast<s16>(new_nodes.size() - 1));
            first->setConnectionDistance(f_slot, dist);
        }

        m_nodes = std::move(new_nodes);
    }

    void Rail::chaikinSubdivide() {
        if (m_nodes.size() < 2)
            return;

        std::vector<node_ptr_t> new_nodes;
        new_nodes.reserve(m_nodes.size() * 2);

        // Helpers for Chaikin math
        auto get_q_point = [](glm::vec3 p0, glm::vec3 p1) { return glm::mix(p0, p1, 0.25f); };
        auto get_r_point = [](glm::vec3 p0, glm::vec3 p1) { return glm::mix(p0, p1, 0.75f); };

        // Check if the rail loops (last node connected to first node)
        bool is_closed_loop = isNodeConnectedToOther(m_nodes.back(), m_nodes.front());
        size_t count        = is_closed_loop ? m_nodes.size() : m_nodes.size() - 1;

        for (size_t i = 0; i < count; ++i) {
            // Get the current segment endpoints
            node_ptr_t p0 = m_nodes[i];
            node_ptr_t p1 = m_nodes[(i + 1) % m_nodes.size()];

            // Create the two new nodes
            auto node_q = make_referable<RailNode>();
            auto node_r = make_referable<RailNode>();

            node_q->m_rail_uuid = getUUID();
            node_r->m_rail_uuid = getUUID();

            node_q->setPosition(get_q_point(p0->getPosition(), p1->getPosition()));
            node_r->setPosition(get_r_point(p0->getPosition(), p1->getPosition()));

            new_nodes.push_back(node_q);
            new_nodes.push_back(node_r);
        }

        // Handle endpoints for open ended rails
        if (!is_closed_loop) {
            node_ptr_t first_node = make_deep_clone<RailNode>(m_nodes.front());
            node_ptr_t last_node  = make_deep_clone<RailNode>(m_nodes.back());

            // Insert Original Start at the beginning
            new_nodes.insert(new_nodes.begin(), first_node);
            // Add Original End at the end
            new_nodes.push_back(last_node);
        }

        // Rebuild connections
        for (size_t i = 0; i < new_nodes.size(); ++i) {
            node_ptr_t current = new_nodes[i];
            current->setConnectionCount(0);  // Clear old connections

            // Connect to previous (if not first)
            if (i > 0) {
                node_ptr_t prev = new_nodes[i - 1];
                f32 dist        = glm::distance(current->getPosition(), prev->getPosition());

                current->setConnectionCount(current->getConnectionCount() + 1);
                current->setConnectionValue(0, static_cast<s16>(i - 1));
                current->setConnectionDistance(0, dist);
            }

            // Connect to next (if not last)
            if (i < new_nodes.size() - 1) {
                node_ptr_t next = new_nodes[i + 1];
                f32 dist        = glm::distance(current->getPosition(), next->getPosition());

                u16 slot = current->getConnectionCount();
                current->setConnectionCount(slot + 1);
                current->setConnectionValue(slot, static_cast<s16>(i + 1));
                current->setConnectionDistance(slot, dist);
            }
        }

        // Handle loop closure connection
        if (is_closed_loop && new_nodes.size() >= 2) {
            node_ptr_t first = new_nodes.front();
            node_ptr_t last  = new_nodes.back();
            f32 dist         = glm::distance(first->getPosition(), last->getPosition());

            // Connect last -> first
            u16 l_slot = last->getConnectionCount();
            last->setConnectionCount(l_slot + 1);
            last->setConnectionValue(l_slot, 0);  // Index 0
            last->setConnectionDistance(l_slot, dist);

            // Connect first -> last
            u16 f_slot = first->getConnectionCount();
            first->setConnectionCount(f_slot + 1);
            first->setConnectionValue(f_slot, static_cast<s16>(new_nodes.size() - 1));
            first->setConnectionDistance(f_slot, dist);
        }

        m_nodes = std::move(new_nodes);
    }

}  // namespace Toolbox::Rail