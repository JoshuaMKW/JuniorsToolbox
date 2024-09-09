#include "gui/project/window.hpp"
#include "model/fsmodel.hpp"

#include <imgui/imgui.h>

namespace Toolbox::UI {

    ProjectViewWindow::ProjectViewWindow(const std::string &name) : ImWindow(name) {}

    void ProjectViewWindow::onRenderMenuBar() {}

    void ProjectViewWindow::onRenderBody(TimeStep delta_time) {
        renderProjectTreeView();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        renderProjectFolderView();
        renderProjectFolderButton();
        renderProjectFileButton();
    }

    void ProjectViewWindow::renderProjectTreeView() {
        if (ImGui::BeginChild("Tree View", {160, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ModelIndex index = m_tree_proxy.getIndex(0, 0);
            renderFolderTree(index);
        }
        ImGui::EndChild();
    }

    void ProjectViewWindow::renderProjectFolderView() {
        if (!m_view_index.isValid()) {
            return;
        }

        if (ImGui::BeginChild("Folder View", { 400, 0 }, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            if (m_view_proxy.hasChildren(m_view_index)) {
                if (m_view_proxy.canFetchMore(m_view_index)) {
                    m_view_proxy.fetchMore(m_view_index);
                }
                for (size_t i = 0; i < m_view_proxy.getRowCount(m_view_index); ++i) {
                    ModelIndex child_index = m_view_proxy.getIndex(i, 0, m_view_index);

                    if (ImGui::BeginChild(child_index.getUUID(), {160, 180}, true,
                                          ImGuiWindowFlags_ChildWindow |
                                              ImGuiWindowFlags_NoDecoration)) {
                        const ImageHandle &icon = m_view_proxy.getIcon(child_index);
                        m_icon_painter.render(icon, {140, 140});

                        ImGui::RenderTextEllipsis(
                            ImGui::GetWindowDrawList(), {10, 150}, {130, 170}, 120.0f, 150.0f,
                            m_view_proxy.getPath(child_index).filename().string().c_str(), nullptr,
                            nullptr);
                    }
                    ImGui::EndChild();
                }
            }
        }
        ImGui::EndChild();
    }

    void ProjectViewWindow::renderProjectFolderButton() {}

    void ProjectViewWindow::renderProjectFileButton() {}

    void ProjectViewWindow::onAttach() {}

    void ProjectViewWindow::onDetach() {}

    void ProjectViewWindow::onImGuiUpdate(TimeStep delta_time) {}

    void ProjectViewWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void ProjectViewWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void ProjectViewWindow::onDropEvent(RefPtr<DropEvent> ev) {}

    void ProjectViewWindow::renderFolderTree(const ModelIndex &index) {
        bool is_open = false;
        if (m_tree_proxy.hasChildren(index)) {
            if (m_tree_proxy.canFetchMore(index)) {
                m_tree_proxy.fetchMore(index);
            }
            is_open = ImGui::TreeNodeEx(m_tree_proxy.getPath(index).filename().string().c_str(),
                                        ImGuiTreeNodeFlags_OpenOnArrow);
            if (is_open) {
                for (size_t i = 0; i < m_tree_proxy.getRowCount(index); ++i) {
                    ModelIndex child_index = m_tree_proxy.getIndex(i, 0, index);
                    renderFolderTree(child_index);
                }
                ImGui::TreePop();
            }
        } else {
            if (ImGui::TreeNodeEx(m_tree_proxy.getPath(index).filename().string().c_str(),
                                  ImGuiTreeNodeFlags_Leaf)) {
                ImGui::TreePop();
            }
        }
    }

    void ProjectViewWindow::initFolderAssets(const ModelIndex &index) {}

}  // namespace Toolbox::UI