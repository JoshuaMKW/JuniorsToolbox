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
        m_context_menu.render("Project View", m_view_index);
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

        const ImGuiStyle &style = ImGui::GetStyle();

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
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);

                ImVec2 size = ImGui::GetContentRegionAvail();

                size_t x_count = (size_t)(size.x / 80);
                if (x_count == 0) {
                    x_count = 1;
                }
                bool any_items_hovered = false;

                ModelIndex view_index = m_view_proxy.toProxyIndex(m_view_index);

                for (size_t i = 0; i < m_view_proxy.getRowCount(m_view_index); ++i) {
                    ModelIndex child_index = m_view_proxy.getIndex(i, 0, view_index);
                    if (!m_view_proxy.validateIndex(child_index)) {
                        TOOLBOX_DEBUG_LOG_V("Invalid index: {}", i);
                        continue;
                    }

                    ModelIndex source_child_index = m_view_proxy.toSourceIndex(child_index);

                    bool is_selected =
                        std::find(m_selected_indices.begin(), m_selected_indices.end(),
                                  source_child_index) != m_selected_indices.end();

                    if (is_selected) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  ImGui::GetStyleColorVec4(ImGuiCol_TabSelected)));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  ImGui::GetStyleColorVec4(ImGuiCol_ChildBg)));
                    }
                    const bool is_selected_rename = m_is_renaming && is_selected;

                    // Get the label and it's size
                    std::string text   = m_view_proxy.getDisplayText(child_index);
                    ImVec2 text_size   = ImGui::CalcTextSize(text.c_str());
                    ImVec2 rename_size = ImGui::CalcTextSize(m_rename_buffer);

                    const float box_width   = 76.0f;
                    const float label_width = box_width - style.WindowPadding.x * 4.0f;
                    const float text_width  = is_selected_rename
                                                  ? std::min(rename_size.x, label_width)
                                                  : std::min(text_size.x, label_width);

                    if (ImGui::BeginChild(child_index.getUUID(), {box_width, 100.0f}, true,
                                          ImGuiWindowFlags_ChildWindow |
                                              ImGuiWindowFlags_NoDecoration)) {
                        ImVec2 pos       = ImGui::GetCursorScreenPos() + style.WindowPadding;
                        ImVec2 icon_size = ImVec2(box_width - style.WindowPadding.x * 2.0f,
                                                  box_width - style.WindowPadding.x * 2.0f);
                        m_icon_painter.render(*m_view_proxy.getDecoration(child_index), pos,
                                              icon_size);

                        // Render the label
                        {
                            if (m_is_renaming && is_selected) {
                                ImVec2 rename_pos =
                                    pos + ImVec2(style.WindowPadding.x,
                                                 icon_size.y + style.ItemInnerSpacing.y -
                                                     style.FramePadding.y);

                                ImGui::SetCursorScreenPos(rename_pos);
                                ImGui::SetKeyboardFocusHere();

                                ImGui::PushItemWidth(label_width);
                                bool done = ImGui::InputText(
                                    "##rename", m_rename_buffer, IM_ARRAYSIZE(m_rename_buffer),
                                    ImGuiInputTextFlags_AutoSelectAll |
                                        ImGuiInputTextFlags_EnterReturnsTrue);
                                if (done) {
                                    m_file_system_model->rename(
                                        m_view_proxy.toSourceIndex(child_index), m_rename_buffer);
                                }
                                ImGui::SetCursorScreenPos(pos);
                                ImGui::PopItemWidth();
                            } else {
                                ImVec2 text_pos =
                                    pos + ImVec2(style.WindowPadding.x,
                                                 icon_size.y + style.ItemInnerSpacing.y);
                                text_pos.x +=
                                    std::max((label_width / 2.0f) - (text_width / 2.0f), 0.0f);

                                float ellipsis_max = text_pos.x + label_width;
                                ImVec2 text_clip_max =
                                    ImVec2(ellipsis_max - 8.0f, text_pos.y + 20.0f);
                                ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), text_pos,
                                                          text_clip_max, ellipsis_max, ellipsis_max,
                                                          text.c_str(), nullptr, nullptr);
                            }
                        }

                        // Handle click responses
                        {
                            any_items_hovered = any_items_hovered || ImGui::IsItemHovered() ||
                                                ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
                            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) {
                                if (ImGui::IsMouseDoubleClicked(0)) {
                                    m_is_renaming = false;
                                    if (m_view_proxy.isDirectory(child_index)) {
                                        m_view_index = m_view_proxy.toSourceIndex(child_index);
                                        ImGui::EndChild();
                                        ImGui::PopStyleColor(1);
                                        break;
                                    }
                                } else if (ImGui::IsMouseClicked(0)) {
                                    m_is_renaming = false;
                                    if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                                        if (is_selected) {
                                            m_selected_indices.erase(std::remove(
                                                m_selected_indices.begin(),
                                                m_selected_indices.end(), source_child_index));
                                        } else {
                                            m_selected_indices.push_back(source_child_index);
                                        }
                                    } else {
                                        m_selected_indices.clear();
                                        m_selected_indices.push_back(source_child_index);
                                    }
                                }
                            }
                        }
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor(1);

                    if ((i + 1) % x_count != 0) {
                        ImGui::SameLine();
                    }
                }

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    std::string message = "";

                    if (m_selected_indices_ctx.size() == 1) {
                        message = TOOLBOX_FORMAT_FN(
                            "Are you sure you want to delete {}?",
                            m_file_system_model->getDisplayText(m_selected_indices[0]));
                    } else if (m_selected_indices_ctx.size() > 1) {
                        message = TOOLBOX_FORMAT_FN(
                            "Are you sure you want to delete the {} selected files?",
                            m_selected_indices_ctx.size());
                    } else {
                        TOOLBOX_ERROR("Selected 0 files to delete!");
                    }
                    ImGui::Text(message.c_str());
                    ImGui::Separator();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::Checkbox("Don't ask me next time", &m_delete_without_request);
                    ImGui::PopStyleVar();
                    if (ImGui::Button("OK", ImVec2(120, 0))) {
                        actionDeleteIndexes(m_selected_indices_ctx);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                } else {
                    // Clearing the selection
                    if (!any_items_hovered && ImGui::IsMouseClicked(0)) {
                        m_selected_indices.clear();
                        m_is_renaming = false;
                    }
                    // Delete stuff
                    if (m_delete_requested) {
                        ImGui::OpenPopup("Delete?");
                        m_delete_requested = false;
                    }
                }

                ImGui::PopStyleVar(5);
            }
        }
        ImGui::EndChild();
    }

    void ProjectViewWindow::renderProjectFolderButton() {}

    void ProjectViewWindow::renderProjectFileButton() {}

    bool ProjectViewWindow::onLoadData(const std::filesystem::path &path) {
        m_project_root      = path;
        m_file_system_model = make_referable<FileSystemModel>();
        m_file_system_model->initialize();
        m_file_system_model->setRoot(m_project_root);
        m_tree_proxy.setSourceModel(m_file_system_model);
        m_tree_proxy.setDirsOnly(true);
        m_view_proxy.setSourceModel(m_file_system_model);
        m_view_proxy.setSortRole(FileSystemModelSortRole::SORT_ROLE_NAME);
        m_view_index = m_view_proxy.getIndex(0, 0);
        return true;
    }

    void ProjectViewWindow::onAttach() { buildContextMenu(); }

    void ProjectViewWindow::onDetach() {}

    void ProjectViewWindow::onImGuiUpdate(TimeStep delta_time) {}

    void ProjectViewWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void ProjectViewWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void ProjectViewWindow::onDropEvent(RefPtr<DropEvent> ev) {}

    void ProjectViewWindow::buildContextMenu() {
        m_context_menu = ContextMenu<ModelIndex>();

        m_context_menu.onOpen(
            [this](const ModelIndex &index) { m_selected_indices_ctx = m_selected_indices; });

        m_context_menu.addOption(
            "Open", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_O}),
            [this]() {
                return m_selected_indices_ctx.size() > 0 &&
                       std::all_of(m_selected_indices_ctx.begin(), m_selected_indices_ctx.end(),
                                   [this](const ModelIndex &index) {
                                       return m_file_system_model->isFile(index);
                                   });
            },
            [this](auto) { actionOpenIndexes(m_selected_indices_ctx); });

        m_context_menu.addOption(
            "Open in Explorer",
            KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_O}),
            [this]() {
                return std::all_of(
                    m_selected_indices_ctx.begin(), m_selected_indices_ctx.end(),
                    [this](const ModelIndex &index) { return m_file_system_model->isFile(index); });
            },
            [this](auto) {
                if (m_selected_indices_ctx.size() == 0) {
                    // TODO: Implement this interface
                    // Toolbox::Platform::OpenFileExplorer(path);
                } else {
                    std::set<fs_path> paths;
                    for (const ModelIndex &item_index : m_selected_indices_ctx) {
                        fs_path path = m_file_system_model->getPath(item_index);
                        if (m_file_system_model->isDirectory(item_index)) {
                            paths.insert(path);
                        } else {
                            paths.insert(path.parent_path());
                        }
                    }
                    for (const fs_path &path : paths) {
                        // TODO: Implement this interface
                        // Toolbox::Platform::OpenFileExplorer(path);
                    }
                }
            });

        m_context_menu.addDivider();

        m_context_menu.addOption(
            "Copy Path",
            KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_C}),
            [this]() { return true; },
            [this](auto) {
                std::string paths;
                if (m_selected_indices_ctx.size() == 0) {
                    paths = m_file_system_model->getPath(m_view_index).string();
                } else {
                    for (const ModelIndex &item_index : m_selected_indices_ctx) {
                        fs_path path = m_file_system_model->getPath(item_index);
                        paths += path.string() + "\n";
                    }
                }
                ImGui::SetClipboardText(paths.c_str());
            });

        m_context_menu.addDivider();

        m_context_menu.addOption(
            "Cut", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_X}),
            [this]() { return m_selected_indices_ctx.size() > 0; }, [this](auto) {});

        m_context_menu.addOption(
            "Copy", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C}),
            [this]() { return m_selected_indices_ctx.size() > 0; }, [this](auto) {});

        m_context_menu.addDivider();

        m_context_menu.addOption(
            "Delete", KeyBind({KeyCode::KEY_DELETE}),
            [this]() { return m_selected_indices_ctx.size() > 0; },
            [this](auto) {
                if (m_delete_without_request) {
                    actionDeleteIndexes(m_selected_indices_ctx);
                } else {
                    m_delete_requested = true;
                }
            });

        m_context_menu.addOption(
            "Rename", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R}),
            [this]() { return m_selected_indices_ctx.size() == 1; },
            [this](auto) { actionRenameIndex(m_selected_indices_ctx[0]); });

        m_context_menu.addDivider();

        m_context_menu.addOption(
            "New...", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N}),
            [this]() { return m_selected_indices_ctx.size() == 0; },
            [this](const ModelIndex &view_index) {
                // TODO: Open a dialog that gives the user different file types to create.
            });
    }

    void ProjectViewWindow::actionDeleteIndexes(std::vector<ModelIndex> &indices) {
        for (auto &item_index : indices) {
            m_file_system_model->remove(item_index);
        }
        indices.clear();
    }

    void ProjectViewWindow::actionOpenIndexes(const std::vector<ModelIndex> &indices) {
        for (auto &item_index : indices) {
            if (m_file_system_model->isDirectory(item_index)) {
                m_view_index = item_index;
                continue;
            }

            // TODO: Open files based on extension. Can be either internal or external
            // depending on the file type.
        }
    }

    void ProjectViewWindow::actionRenameIndex(const ModelIndex &index) {
        m_is_renaming         = true;
        std::string file_name = m_file_system_model->getDisplayText(index);
        std::strncpy(m_rename_buffer, file_name.c_str(), IM_ARRAYSIZE(m_rename_buffer));
    }

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
                // m_fs_watchdog.addPath(m_file_system_model->getPath(m_view_index));
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
