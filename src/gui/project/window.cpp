#include "gui/project/window.hpp"
#include "model/fsmodel.hpp"

#include <imgui/imgui.h>
#include <cmath>

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
        if (!m_view_proxy.validateIndex(m_view_index)) {
            return;
        }

        if (ImGui::BeginChild("Folder View", {0, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            if (m_view_proxy.hasChildren(m_view_index)) {
                if (m_view_proxy.canFetchMore(m_view_index)) {
                    m_view_proxy.fetchMore(m_view_index);
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

                for (size_t i = 0; i < m_view_proxy.getRowCount(m_view_index); ++i) {
                    ModelIndex child_index = m_view_proxy.getIndex(i, 0, m_view_index);

                    if (ImGui::BeginChild(child_index.getUUID(), {76, 92}, true,
                                          ImGuiWindowFlags_ChildWindow |
                                              ImGuiWindowFlags_NoDecoration)) {

                        m_icon_painter.render(*m_view_proxy.getDecoration(child_index), {72, 72});

                        std::string text = m_view_proxy.getDisplayText(child_index);
                        ImVec2 text_size = ImGui::CalcTextSize(text.c_str());

                        ImVec2 pos = ImGui::GetCursorScreenPos();
                        pos.x += std::max<float>(36.0f - (text_size.x / 2.0f), 0.0);
                        pos.y += 72.0f;
                        //pos.x += i * size.x / 4.0f;

                        ImGui::RenderTextEllipsis(
                            ImGui::GetWindowDrawList(), pos, pos + ImVec2(72, 20), pos.x + 72.0f,
                            pos.x + 76.0f, m_view_proxy.getDisplayText(child_index).c_str(),
                            nullptr, nullptr);
                    }
                    ImGui::EndChild();

                    // ImGui::Text("%s", m_view_proxy.getDisplayText(child_index).c_str());

                    if ((i + 1) % x_count != 0) {
                        ImGui::SameLine(i * size.x / 4.0f);
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
                    if (m_tree_proxy.validateIndex(child_index)) {
                        renderFolderTree(child_index);
                    }
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