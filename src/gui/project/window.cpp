#include "gui/project/window.hpp"
#include "model/fsmodel.hpp"

#include <cmath>
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
        if (ImGui::BeginChild("Tree View", {300, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ModelIndex index = m_tree_proxy.getIndex(0, 0);
            renderFolderTree(index);
        }
        ImGui::EndChild();
    }

    void ProjectViewWindow::renderProjectFolderView() {
        if (!m_file_system_model->validateIndex(m_view_index)) {
            return;
        }

        if (ImGui::BeginChild("Folder View", {0, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            if (m_file_system_model->hasChildren(m_view_index)) {
                if (m_file_system_model->canFetchMore(m_view_index)) {
                    m_file_system_model->fetchMore(m_view_index);
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});

                ImVec2 size = ImGui::GetContentRegionAvail();

                size_t x_count = (size_t)(size.x / 80);
                if (x_count == 0) {
                    x_count = 1;
                }

                for (size_t i = 0; i < m_file_system_model->getRowCount(m_view_index); ++i) {
                    ModelIndex child_index = m_file_system_model->getIndex(i, 0, m_view_index);

                    if (ImGui::BeginChild(child_index.getUUID(), {76, 92}, true,
                                          ImGuiWindowFlags_ChildWindow |
                                              ImGuiWindowFlags_NoDecoration)) {

                        m_icon_painter.render(*m_file_system_model->getDecoration(child_index), {72, 72});

                        std::string text = m_file_system_model->getDisplayText(child_index);
                        ImVec2 text_size = ImGui::CalcTextSize(text.c_str());

                        ImVec2 pos = ImGui::GetCursorScreenPos();
                        pos.x += std::max<float>(36.0f - (text_size.x / 2.0f), 0.0);
                        pos.y += 72.0f;
                        if (ImGui::IsMouseDoubleClicked(0) &&
                            ImGui::IsItemHovered(ImGuiHoveredFlags_None) &&
                            m_file_system_model->isDirectory(child_index)) {
                            m_view_index = m_tree_proxy.toSourceIndex(child_index);
                        }

                        ImGui::RenderTextEllipsis(
                            ImGui::GetWindowDrawList(), pos, pos + ImVec2(64, 20), pos.x + 64.0f,
                            pos.x + 76.0f, text.c_str(),
                            nullptr, nullptr);
                    }
                    ImGui::EndChild();

                    // ImGui::Text("%s", m_view_proxy.getDisplayText(child_index).c_str());

                    if ((i + 1) % x_count != 0) {
                        ImGui::SameLine();
                    }
                }

                ImGui::PopStyleVar(4);
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

    bool ProjectViewWindow::isViewedAncestor(const ModelIndex &index) {
        if (m_view_index == m_tree_proxy.toSourceIndex(index)) {
            return true;
        }
        for (size_t i = 0; i < m_tree_proxy.getRowCount(index); ++i) {
            ModelIndex child_index = m_tree_proxy.getIndex(i, 0, index);
            if (isViewedAncestor(child_index)) {
                return true;
            }
        }
        return false;
    }

    void ProjectViewWindow::renderFolderTree(const ModelIndex &index) {
        bool is_open = false;
        if (m_tree_proxy.hasChildren(index)) {
            if (m_tree_proxy.canFetchMore(index)) {
                m_tree_proxy.fetchMore(index);
            }
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

            if (m_view_index == m_tree_proxy.toSourceIndex(index)){
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            if (isViewedAncestor(index)) {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }
            is_open = ImGui::TreeNodeEx(m_tree_proxy.getDisplayText(index).c_str(),
                                        flags);

            if (ImGui::IsItemClicked()) {
                m_view_index = m_tree_proxy.toSourceIndex(index);
            }

            if (is_open) {
                for (size_t i = 0; i < m_tree_proxy.getRowCount(index); ++i) {
                    ModelIndex child_index = m_tree_proxy.getIndex(i, 0, index);
                    if (m_tree_proxy.validateIndex(child_index)) {
                        renderFolderTree(child_index);
                    }
                }
                ImGui::TreePop();
            }
        } else {
            if (ImGui::TreeNodeEx(m_tree_proxy.getDisplayText(index).c_str(),
                                  ImGuiTreeNodeFlags_Leaf)) {
                ImGui::TreePop();
            }
        }
    }

    void ProjectViewWindow::initFolderAssets(const ModelIndex &index) {}

}  // namespace Toolbox::UI