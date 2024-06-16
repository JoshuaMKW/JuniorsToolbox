#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gui/widget/widget.hpp"
#include "unique.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    using TreeNodeID = UUID64;

    enum class TreeNodeFlags : uint32_t {
        NODE_NONE       = 0,
        NODE_LEAF       = 1 << 0,
        NODE_SELECTABLE = 1 << 1,
        NODE_EXPANDED   = 1 << 2,
        NODE_SELECTED   = 1 << 3,
    };
    TOOLBOX_BITWISE_ENUM(TreeNodeFlags);

    template <typename _T> struct TreeNodeRenderProxy {
        bool renderBegin(const _T &node_data, ImRect &out_rect) {}
        void renderEnd() {}

        bool isNodeLeaf(const _T &node_data) const { return false; }
        bool isNodeSelectable(const _T &node_data) const { return false; }
        bool isNodeExpanded(const _T &node_data) const { return false; }
        bool isNodeSelected(const _T &node_data) const { return false; }

        void expandNode(_T &node_data) {}
        void collapseNode(_T &node_data) {}

        void selectNode(_T &node_data) {}
        void deselectNode(_T &node_data) {}

        ImVec2 getNodeSize(const _T &node_data) const { return ImVec2(0.0f, 0.0f); }

    private:
        ImRect m_rect;
        TreeNodeFlags m_flags;
    };

    template <> struct TreeNodeRenderProxy<std::string> {
        bool renderBegin(const std::string &node_data, ImRect &out_rect,
                         ImGuiTreeNodeFlags default_flags) {
            ImGui::TreeNodeEx(node_data.c_str(), default_flags);
            out_rect.Min = ImGui::GetItemRectMin();
            out_rect.Max = ImGui::GetItemRectMax();
        }
        void renderEnd() {}
    };

    struct DropTargetInfo {
        std::optional<TreeNodeID> m_parent;
        size_t m_child_index;
    };

    template <typename _T> class ImTreeWidget : public ImWidget {
    private:
        struct TreeNodeInfo : public IUnique {
            TreeNodeID m_parent;
            TreeNodeRenderProxy<_T> m_render_proxy;
            ImRect m_rect;

            bool isNodeLeaf() const {
                return (m_flags & TreeNodeFlags::NODE_LEAF) != TreeNodeFlags::NODE_NONE;
            }
            bool isNodeExpanded() const {
                return (m_flags & TreeNodeFlags::NODE_EXPANDED) != TreeNodeFlags::NODE_NONE;
            }
            void expandNode() { m_flags |= TreeNodeFlags::NODE_EXPANDED; }
            void collapseNode() { m_flags &= ~TreeNodeFlags::NODE_EXPANDED; }

            bool isNodeSelectable() const {
                return (m_flags & TreeNodeFlags::NODE_SELECTABLE) != TreeNodeFlags::NODE_NONE;
            }
            bool isNodeSelected() const {
                return (m_flags & TreeNodeFlags::NODE_SELECTED) != TreeNodeFlags::NODE_NONE;
            }
            void selectNode() { m_flags |= TreeNodeFlags::NODE_SELECTED; }
            void deselectNode() { m_flags &= ~TreeNodeFlags::NODE_SELECTED; }

        private:
            TreeNodeFlags m_flags;
        };

    public:
        explicit ImTreeWidget(const std::string &name) : ImWidget(name) {}
        ImTreeWidget(const std::string &name, std::optional<ImVec2> default_size)
            : ImWidget(name, default_size) {}
        ImTreeWidget(const std::string &name, std::optional<ImVec2> min_size,
                     std::optional<ImVec2> max_size)
            : ImWidget(name, min_size, max_size) {}
        ImTreeWidget(const std::string &name, std::optional<ImVec2> default_size,
                     std::optional<ImVec2> min_size, std::optional<ImVec2> max_size)
            : ImWidget(name, default_size, min_size, max_size) {}

        virtual ~ImTreeWidget() = default;

        TreeNodeID rootNode() const { return m_root_node; }
        void setRootNode(const TreeNodeID &root_node) { m_root_node = root_node; }
        void setRootNode(TreeNodeID &&root_node) { m_root_node = std::move(root_node); }

        TreeNodeID findNode(const _T &value) const;

        TreeNodeID getParentNode(const TreeNodeID &id) const;
        std::vector<TreeNodeID> getChildrenNodes(const TreeNodeID &id) const;

        void addNode(const TreeNodeID &id, const TreeNodeID &parent,
                     TreeNodeFlags flags = TreeNodeFlags::NODE_NONE);
        void insertNode(const TreeNodeID &id, const TreeNodeID &parent, size_t index,
                        TreeNodeFlags flags = TreeNodeFlags::NODE_NONE);
        void removeNode(const TreeNodeID &id);

        TreeNodeFlags getNodeFlags(const TreeNodeID &id) const { return m_node_info[id].m_flags; }
        void setNodeFlags(const TreeNodeID &id, TreeNodeFlags flags) {
            m_node_info[id].m_flags = flags;
        }

        bool isNodeLeaf(const TreeNodeID &id) const { return m_node_info[id].isNodeLeaf(); }
        bool isNodeSelectable(const TreeNodeID &id) const {
            return m_node_info[id].isNodeSelectable();
        }

        bool isNodeExpanded(const TreeNodeID &id) const { return m_node_info[id].isNodeExpanded(); }
        void expandNode(const TreeNodeID &id) { m_node_info[id].expandNode(); }
        void collapseNode(const TreeNodeID &id) { m_node_info[id].collapseNode(); }

        bool isNodeSelected(const TreeNodeID &id) const { return m_node_info[id].isNodeExpanded(); }
        void selectNode(const TreeNodeID &id) { m_node_info[id].selectNode(); }
        void deselectNode(const TreeNodeID &id) { m_node_info[id].deselectNode(); }

        TreeNodeID getNodeAt(const ImVec2 &pos) const;
        TreeNodeID getNodeAt(size_t index, std::optional<TreeNodeID> parent) const;

        DropTargetInfo getDropTarget(const ImVec2 &pos) const;

        void onAttach() override;

    protected:
        void onRenderBody(TimeStep delta_time) override;

        std::optional<TreeNodeID> recursiveFindNode(const ImVec2 &pos,
                                                    const TreeNodeID &node, size_t index) const;
        DropTargetInfo recursiveFindTarget(const ImVec2 &pos, const TreeNodeID &node,
                                           size_t index) const;

    private:
        TreeNodeID m_root_node;
        mutable std::unordered_map<TreeNodeID, TreeNodeInfo> m_node_info;
    };

    // Implementation

    template <typename _T>
    std::optional<TreeNodeID> ImTreeWidget<_T>::recursiveFindNode(const ImVec2 &pos,
                                                                  const TreeNodeID &node_id,
                                                                  size_t index) const {
        const TreeNodeInfo &info = m_node_info[node_id];

        if (info.m_rect.Contains(pos)) {
            return node_id;
        }

        if (!info.isNodeExpanded()) {
            return {};
        }

        std::vector<TreeNodeID> children = getChildrenNodes(node_id);
        for (size_t i = 0; i < children.size(); ++i) {
            std::optional<TreeNodeID> result = recursiveFindNode(pos, children[i], i);
            if (result) {
                return result;
            }
        }

        return {};
    }

    template <typename _T>
    DropTargetInfo ImTreeWidget<_T>::recursiveFindTarget(const ImVec2 &pos,
                                                         const TreeNodeID &node_id,
                                                         size_t index) const {
        const TreeNodeInfo &info = m_node_info[node_id];

        if (info.m_rect.Contains(pos)) {
            if (info.isNodeLeaf()) {
                // Determine if it should drop above or below the node
                if (pos.y < info.m_rect.GetCenter().y) {
                    return {info.m_parent, index};
                } else {
                    return {info.m_parent, index + 1};
                }
            }

            // If the node is not a leaf, then the middle of the rect is for
            // expanding/collapsing the node and dropping as a child.
            size_t as_child_range = info.m_rect.GetHeight() / 3;
            if (pos.y < info.m_rect.GetTL().y + as_child_range) {
                // Drop above the node
                return {info.m_parent, index};
            } else if (pos.y > info.m_rect.GetBR().y - as_child_range) {
                // Drop below the node
                return {info.m_parent, index + 1};
            } else {
                // Drop as a child
                return {node_id, getChildrenNodes(node_id).size()};
            }
        }

        if (!info.isNodeExpanded()) {
            return {};
        }

        std::vector<TreeNodeID> children = getChildrenNodes(node_id);
        for (size_t i = 0; i < children.size(); ++i) {
            DropTargetInfo result = recursiveFindTarget(pos, children[i], i);
            if (result.m_parent) {
                return result;
            }
        }

        return {};
    }

    template <typename _T> TreeNodeID ImTreeWidget<_T>::findNode(const _T& value) const {
        return {};
    }

    template <typename _T> TreeNodeID ImTreeWidget<_T>::getParentNode(const TreeNodeID &id) const {
        return m_node_info[id].m_parent;
    }

    template <typename _T>
    std::vector<TreeNodeID> ImTreeWidget<_T>::getChildrenNodes(const TreeNodeID &id) const {
        std::vector<TreeNodeID> nodes;
        for (const auto &[node_id, node_info] : m_node_info) {
            if (node_info.m_parent == id) {
                nodes.push_back(node_id);
            }
        }
        return nodes;
    }

    template <typename _T>
    void ImTreeWidget<_T>::addNode(const TreeNodeID &id, const TreeNodeID &parent,
                                   TreeNodeFlags flags) {
        m_node_info[id] = {
            parent, flags, {0, 0, 0, 0}
        };
    }

    template <typename _T>
    void ImTreeWidget<_T>::insertNode(const TreeNodeID &id, const TreeNodeID &parent, size_t index,
                                      TreeNodeFlags flags) {
        m_node_info[id] = {
            parent, flags, {0, 0, 0, 0}
        };
    }

    template <typename _T> void ImTreeWidget<_T>::removeNode(const TreeNodeID &id) {}

    template <typename _T> TreeNodeID ImTreeWidget<_T>::getNodeAt(const ImVec2 &pos) const {
        return recursiveFindNode(pos, m_root_node, 0);
    }

    template <typename _T>
    TreeNodeID ImTreeWidget<_T>::getNodeAt(size_t index, std::optional<TreeNodeID> parent) const {
        if (!parent) {
            return index == 0 ? m_root_node : TreeNodeID();
        }
        std::vector<TreeNodeID> children = getChildrenNodes(*parent);
        if (index >= children.size()) {
            return TreeNodeID();
        }
        return children[index];
    }

    template <typename _T> DropTargetInfo ImTreeWidget<_T>::getDropTarget(const ImVec2 &pos) const {
        return recursiveFindTarget(pos, m_root_node->getUUID());
    }

    template <typename _T> void ImTreeWidget<_T>::onAttach() { ImWidget::onAttach(); }

    template <typename _T> void ImTreeWidget<_T>::onRenderBody(TimeStep delta_time) {}

}  // namespace Toolbox::UI