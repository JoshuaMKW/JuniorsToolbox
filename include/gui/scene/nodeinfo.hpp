#pragma once

#include "objlib/object.hpp"
#include "scene/raildata.hpp"

namespace Toolbox::UI {

    template <typename T> struct SelectionNodeInfo {
        std::shared_ptr<T> m_selected;
        size_t m_node_id;
        size_t m_selection_index;
        bool m_parent_synced;
        bool m_scene_synced;

        bool operator==(const SelectionNodeInfo &other) { return m_node_id == other.m_node_id; }
    };

}  // namespace Toolbox::UI