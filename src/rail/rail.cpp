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
        glm::vec3 accum = std::accumulate(
            m_nodes.begin(), m_nodes.end(), glm::vec3(),
            [](glm::vec3 a, node_ptr_t node) { return a + node->getPosition(); });
        return accum / static_cast<float>(node_count);
    }

    void Rail::addNode(node_ptr_t node) {
        node->m_rail = this;
        m_nodes.push_back(node);
    }

    std::expected<void, MetaError> Rail::insertNode(size_t index, node_ptr_t node) {
        if (index > m_nodes.size()) {
            return make_meta_error<void>("Insert index out of range", index, m_nodes.size());
        }
        node->m_rail = this;
        m_nodes.insert(m_nodes.begin() + index, node);
        return {};
    }

    std::expected<void, MetaError> Rail::removeNode(size_t index) {
        if (index >= m_nodes.size()) {
            return make_meta_error<void>("Remove index out of range", index, m_nodes.size());
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
            return make_meta_error<void>("Swap index (1) out of range", index1, m_nodes.size());
        }

        if (index2 >= m_nodes.size()) {
            return make_meta_error<void>("Swap index (2) out of range", index2, m_nodes.size());
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

}  // namespace Toolbox::Rail