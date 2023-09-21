#include "rail/rail.hpp"
#include "rail/node.hpp"

#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"

using namespace Toolbox::Object;

namespace Toolbox::Rail {

    std::unique_ptr<IClonable> Rail::clone(bool deep) const {
        auto clone = std::make_unique<Rail>(m_name);

        if (deep) {
            for (const auto &node : m_nodes) {
                clone->m_nodes.push_back(*make_deep_clone<RailNode>(node));
            }
        } else {
            clone->m_nodes.clear();
            for (auto &node : m_nodes) {
                clone->m_nodes.push_back(node);
            }
        }

        return clone;
    }

}  // namespace Toolbox::Rail