#pragma once

#include "objlib/object.hpp"

namespace Toolbox::UI {

    struct NodeInfo {
        std::shared_ptr<Object::ISceneObject> m_selected;
        size_t m_node_id;
        bool m_hierarchy_synced;
        bool m_scene_synced;

        bool operator==(const NodeInfo &other) { return m_node_id == other.m_node_id; }
    };

}