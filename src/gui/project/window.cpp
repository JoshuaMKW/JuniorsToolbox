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
                bool any_items_hovered = false;

                ModelIndex view_index = m_view_proxy.toProxyIndex(m_view_index);

                for (size_t i = 0; i < m_view_proxy.getRowCount(m_view_index); ++i) {
                    ModelIndex child_index = m_view_proxy.getIndex(i, 0, view_index);
                    bool is_selected =
                        std::find(m_selected_indices.begin(), m_selected_indices.end(),
                                  child_index) != m_selected_indices.end();

                    if (is_selected) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  ImGui::GetStyleColorVec4(ImGuiCol_TabSelected)));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  ImGui::GetStyleColorVec4(ImGuiCol_ChildBg)));
                    }
                    // Get the label and it's size
                    std::string text   = m_view_proxy.getDisplayText(child_index);
                    ImVec2 text_size   = ImGui::CalcTextSize(text.c_str());
                    ImVec2 rename_size = ImGui::CalcTextSize(m_rename_buffer);

                    float box_width = m_is_renaming && is_selected ? std::max(rename_size.x, 76.0f)
                                                                   : 76.0f;
                    if (ImGui::BeginChild(
                            m_view_proxy.getSourceUUID(child_index), {box_width, 92.0f}, true,
                            ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {

                        m_icon_painter.render(*m_view_proxy.getDecoration(child_index), {72.0f, 72.0f});

                        // Render the label
                        ImVec2 pos    = ImGui::GetCursorScreenPos();
                        ImVec2 newPos = pos;
                        newPos.x += std::max<float>(36.0f - (((m_is_renaming && is_selected)
                                                                  ? std::max(rename_size.x, 40.0f)
                                                                  : text_size.x) /
                                                             2.0f),
                                                    0.0);
                        newPos.y += 72.0f;
                        if (m_is_renaming && is_selected) {
                            ImGui::SetCursorScreenPos(newPos);
                            ImGui::SetKeyboardFocusHere();
                            ImGui::PushItemWidth(std::max(rename_size.x, 40.0f));
                            bool done = ImGui::InputText("##rename", m_rename_buffer,
                                                         IM_ARRAYSIZE(m_rename_buffer),
                                                         ImGuiInputTextFlags_AutoSelectAll |
                                                             ImGuiInputTextFlags_EnterReturnsTrue);
                            if (done) {
                                m_file_system_model->rename(m_view_proxy.toSourceIndex(child_index),
                                                            m_rename_buffer);
                            }
                            ImGui::SetCursorScreenPos(pos);
                            ImGui::PopItemWidth();
                        } else {
                            ImGui::RenderTextEllipsis(
                                ImGui::GetWindowDrawList(), newPos, newPos + ImVec2(64, 20),
                                pos.x + 64.0f, newPos.x + 76.0f, text.c_str(), nullptr, nullptr);
                        }
                        any_items_hovered = any_items_hovered || ImGui::IsItemHovered() ||
                                            ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
                        // Handle click responses
                        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) {
                            if (ImGui::IsMouseDoubleClicked(0)) {
                                if (m_view_proxy.isDirectory(child_index)) {
                                    m_view_index = m_tree_proxy.toSourceIndex(child_index);
                                }
                                m_is_renaming = false;
                            } else if (ImGui::IsMouseClicked(0)) {
                                if (is_selected){
                                    if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                                        m_selected_indices.erase(
                                            std::find(m_selected_indices.begin(),
                                                      m_selected_indices.end(), child_index));
                                        m_is_renaming = false;
                                    } else {
                                        m_is_renaming = true;
                                        std::strncpy(m_rename_buffer, text.c_str(),
                                                     IM_ARRAYSIZE(m_rename_buffer));
                                    }
                                } else {
                                    if (!ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                                        m_selected_indices.clear();
                                    }
                                    m_selected_indices.push_back(child_index);
                                    m_is_renaming = false;
                                }
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor(1);

                    // ImGui::Text("%s", m_view_proxy.getDisplayText(child_index).c_str());

                    if ((i + 1) % x_count != 0) {
                        ImGui::SameLine();
                    }
                }

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                static bool dont_ask_for_deletes = false;
                if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    std::string message = "";

                    if (m_selected_indices.size() == 1) {
                        message = TOOLBOX_FORMAT_FN(
                            "Are you sure you want to delete {}?",
                            m_file_system_model->getDisplayText(m_selected_indices[0]));
                    } else if (m_selected_indices.size() > 1) {
                        message = TOOLBOX_FORMAT_FN(
                            "Are you sure you want to delete the {} selected files?",
                            m_selected_indices.size());
                    } else {
                        TOOLBOX_ERROR("Selected 0 files to delete!");
                    }
                    ImGui::Text(message.c_str());
                    ImGui::Separator();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::Checkbox("Don't ask me next time", &dont_ask_for_deletes);
                    ImGui::PopStyleVar();
                    if (ImGui::Button("OK", ImVec2(120, 0))) {
                        for (auto &item_index : m_selected_indices) {
                            m_file_system_model->remove(item_index);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                    ImGui::EndPopup();
                } else {
                    // Clearing the selection
                    if (!any_items_hovered && ImGui::IsMouseClicked(0)) {
                        m_selected_indices.clear();
                        m_is_renaming = false;
                    }
                    // Delete stuff
                    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !m_selected_indices.empty()) {
                        if (dont_ask_for_deletes) {
                            for (auto &item_index : m_selected_indices) {
                                m_file_system_model->remove(item_index);
                            }
                        } else {
                            ImGui::OpenPopup("Delete?");
                        }
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

            if (m_view_index == m_tree_proxy.toSourceIndex(index)) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            if (isViewedAncestor(index)) {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }
            is_open = ImGui::TreeNodeEx(m_tree_proxy.getDisplayText(index).c_str(), flags);

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
