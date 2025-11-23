#include "gui/project/window.hpp"
#include "gui/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/new_item/window.hpp"
#include "model/fsmodel.hpp"

#include <cctype>
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

        m_did_drag_drop = DragDropManager::instance().getCurrentDragAction() != nullptr;
    }

    void ProjectViewWindow::renderProjectTreeView() {
        if (ImGui::BeginChild("Tree View", {300, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ModelIndex index = m_tree_proxy->getIndex(0, 0);
            renderFolderTree(index);
        }
        ImGui::EndChild();
    }

    void ProjectViewWindow::renderProjectFolderView() {
        if (!m_file_system_model->validateIndex(m_view_index)) {
            return;
        }

        const ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 mouse_pos;
        {
            double mouse_x, mouse_y;
            Input::GetMousePosition(mouse_x, mouse_y);
            mouse_pos.x = mouse_x;
            mouse_pos.y = mouse_y;
        }

        bool is_left_click         = Input::GetMouseButtonDown(MouseButton::BUTTON_LEFT);
        bool is_left_click_release = Input::GetMouseButtonUp(MouseButton::BUTTON_LEFT);
        bool is_left_drag          = Input::GetMouseButton(MouseButton::BUTTON_LEFT);

        if (!is_left_drag) {
            m_last_reg_mouse_pos = mouse_pos;
        }

        bool is_double_left_click   = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        bool is_right_click         = Input::GetMouseButtonDown(MouseButton::BUTTON_RIGHT);
        bool is_right_click_release = Input::GetMouseButtonUp(MouseButton::BUTTON_RIGHT);

        if (ImGui::BeginChild("Folder View", {0, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            if (m_file_system_model->hasChildren(m_view_index)) {
                if (m_file_system_model->canFetchMore(m_view_index)) {
                    m_file_system_model->fetchMore(m_view_index);
                }

                const bool is_window_hovered = ImGui::IsWindowHovered(
                    ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy);

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);

                const float box_width   = 76.0f;
                const float box_height  = 100.0f;
                const float label_width = box_width - style.WindowPadding.x * 4.0f;

                ImVec2 size = ImGui::GetContentRegionAvail();

                size_t x_count = (size_t)(size.x / (box_width + style.ItemSpacing.x) + 0.1f);
                if (x_count == 0) {
                    x_count = 1;
                }
                bool any_items_hovered = false;

                ModelIndex view_index = m_view_proxy->toProxyIndex(m_view_index);
                const IDataModel::index_container &selection =
                    m_selection_mgr.getState().getSelection();

                for (size_t i = 0; i < m_view_proxy->getRowCount(view_index); ++i) {
                    ModelIndex child_index = m_view_proxy->getIndex(i, 0, view_index);
                    if (!m_view_proxy->validateIndex(child_index)) {
                        TOOLBOX_DEBUG_LOG_V("Invalid index: {}", i);
                        continue;
                    }

                    ImVec2 win_min = ImGui::GetCursorScreenPos();
                    ImVec2 win_max = win_min + ImVec2(box_width, box_height);

                    const bool is_selected = m_selection_mgr.getState().isSelected(child_index);
                    const bool is_hovered =
                        is_window_hovered && ImGui::IsMouseHoveringRect(win_min, win_max);
                    const bool is_cut = std::find(m_cut_indices.begin(), m_cut_indices.end(),
                                                  child_index) != m_cut_indices.end();

                    const float render_alpha = is_cut ? 0.5f : 1.0f;

                    if (is_hovered) {
                        ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_TabHovered);
                        col.w *= render_alpha;
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(col));
                    } else if (is_selected) {
                        ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_TabSelected);
                        col.w *= render_alpha;
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(col));
                    } else {
                        ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
                        col.w *= render_alpha;
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(col));
                    }

                    const bool is_selected_rename = m_is_renaming && is_selected;

                    // Get the label and it's size
                    std::string text   = m_view_proxy->getDisplayText(child_index);
                    ImVec2 text_size   = ImGui::CalcTextSize(text.c_str());
                    ImVec2 rename_size = ImGui::CalcTextSize(m_rename_buffer);

                    const float text_width = is_selected_rename
                                                 ? std::min(rename_size.x, label_width)
                                                 : std::min(text_size.x, label_width);

                    ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildWindow |
                                                    ImGuiWindowFlags_NoDecoration |
                                                    ImGuiWindowFlags_NoScrollWithMouse;

                    if (child_index == m_selection_mgr.getState().getLastSelected()) {
                        ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                        col.w *= render_alpha;
                        ImGui::GetWindowDrawList()->AddRect(win_min - (style.FramePadding / 2.0f),
                                                            win_max + (style.FramePadding / 2.0f),
                                                            ImGui::ColorConvertFloat4ToU32(col),
                                                            3.0f, ImDrawFlags_RoundCornersAll,
                                                            2.0f);
                    }

                    if (ImGui::BeginChild(child_index.getUUID(), {box_width, 100.0f}, true,
                                          window_flags)) {
                        ImVec2 pos       = ImGui::GetCursorScreenPos() + style.WindowPadding;
                        ImVec2 icon_size = ImVec2(box_width - style.WindowPadding.x * 2.0f,
                                                  box_width - style.WindowPadding.x * 2.0f);

                        m_icon_painter.setTintColor({1.0f, 1.0f, 1.0f, render_alpha});
                        m_icon_painter.render(*m_view_proxy->getDecoration(child_index), pos,
                                              icon_size);

                        // Render the label
                        {
                            if (is_selected_rename) {
                                ImVec2 rename_pos =
                                    pos + ImVec2(style.WindowPadding.x,
                                                 icon_size.y + style.ItemInnerSpacing.y -
                                                     style.FramePadding.y);

                                ImGui::SetCursorScreenPos(rename_pos);
                                ImGui::SetKeyboardFocusHere();

                                if (m_is_valid_name) {
                                    ImGui::PushStyleColor(
                                        ImGuiCol_Text,
                                        ImGui::ColorConvertFloat4ToU32(
                                            ImGui::GetStyleColorVec4(ImGuiCol_Text)));
                                } else {
                                    ImGui::PushStyleColor(
                                        ImGuiCol_Text,
                                        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0, 0.3, 0.3, 1.0)));
                                }
                                ImGui::PushItemWidth(label_width);
                                bool edited = ImGui::InputText("##rename", m_rename_buffer,
                                                               IM_ARRAYSIZE(m_rename_buffer),
                                                               ImGuiInputTextFlags_AutoSelectAll);
                                // Pop text color
                                ImGui::PopStyleColor(1);
                                if (edited) {
                                    m_is_valid_name = isValidName(m_rename_buffer, selection);
                                }
                                if (ImGui::IsItemDeactivatedAfterEdit() && m_is_valid_name) {
                                    if (std::strlen(m_rename_buffer) == 0) {
                                        TOOLBOX_DEBUG_LOG(
                                            "Attempted to rename to the empty string, ignoring.");
                                    } else {
                                        m_file_system_model->rename(
                                            m_view_proxy->toSourceIndex(child_index),
                                            m_rename_buffer);
                                    }
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
                                                          text_clip_max, ellipsis_max, text.c_str(),
                                                          nullptr, nullptr);
                            }
                        }

                        // Handle click responses
                        {
                            any_items_hovered |= ImGui::IsItemHovered() || is_hovered;
                            if (is_hovered) {
                                if (is_double_left_click) {
                                    m_is_renaming = false;
                                    if (m_view_proxy->isDirectory(child_index)) {
                                        ModelIndex new_view_index =
                                            m_view_proxy->toSourceIndex(child_index);
                                        if (m_view_index != new_view_index) {
                                            m_selection_mgr.getState().clearSelection();
                                            m_view_index = new_view_index;
                                            m_file_system_model->watchPathForUpdates(m_view_index,
                                                                                     true);
                                        }
                                        ImGui::EndChild();
                                        ImGui::PopStyleColor(1);
                                        break;
                                    }
                                } else {
                                    if ((is_left_click && !is_left_click_release) ||
                                        (is_right_click && !is_right_click_release)) {
                                        m_is_renaming = false;
                                    }
                                }
                                m_selection_mgr.handleActionsByMouseInput(child_index, true);
                            }
                        }
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor(1);

                    if ((i + 1) % x_count != 0) {
                        ImGui::SameLine();
                    }
                }

                if (is_window_hovered && !any_items_hovered &&
                    (is_left_click_release || is_right_click_release)) {
                    const bool is_ctrl_state = Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) ||
                                               Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL);
                    const bool is_shft_state = Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT) ||
                                               Input::GetKey(Input::KeyCode::KEY_RIGHTSHIFT);
                    if (!is_ctrl_state && !is_shft_state && !m_selection_mgr.isDragState()) {
                        m_selection_mgr.getState().clearSelection();
                    }

                    m_is_renaming = false;
                }

                if (m_selection_mgr.processDragState()) {
                    if (DragDropManager::instance().getCurrentDragAction() == nullptr) {
                        RefPtr<DragAction> action = DragDropManager::instance().createDragAction(
                            getUUID(), getLowHandle(),
                            std::move(*m_selection_mgr.actionCopySelection().release()));
                        if (action) {
                            action->setHotSpot(mouse_pos);
                            action->setRender([action](const ImVec2 &pos, const ImVec2 &size) {
                                auto urls_maybe = action->getPayload().get_urls();
                                if (!urls_maybe) {
                                    return;
                                }
                                std::vector<std::string> urls = urls_maybe.value();

                                ImGuiStyle &style = ImGui::GetStyle();

                                size_t num_files       = urls.size();
                                std::string file_count = std::format("{}", num_files);
                                ImVec2 text_size       = ImGui::CalcTextSize(file_count.c_str());

                                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                                ImVec2 center         = pos + (size / 2.0f);

                                ImGui::DrawSquare((size / 2.0f), size.x / 5.0f, IM_COL32_WHITE,
                                                  ImGui::ColorConvertFloat4ToU32(
                                                      style.Colors[ImGuiCol_HeaderActive]),
                                                  1.0f);

                                draw_list->AddText(center - (text_size / 2.0f), IM_COL32_WHITE,
                                                   file_count.c_str(),
                                                   file_count.c_str() + file_count.size());
                            });
                        }
                    }
                }

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    std::string message = "";

                    if (selection.size() == 1) {
                        message = TOOLBOX_FORMAT_FN("Are you sure you want to delete {}?",
                                                    m_view_proxy->getDisplayText(selection[0]));
                    } else if (selection.size() > 1) {
                        message = TOOLBOX_FORMAT_FN(
                            "Are you sure you want to delete the {} selected files?",
                            selection.size());
                    } else {
                        TOOLBOX_ERROR("Selected 0 files to delete!");
                    }
                    ImGui::Text(message.c_str());
                    ImGui::Separator();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::Checkbox("Don't ask me next time", &m_delete_without_request);
                    ImGui::PopStyleVar();
                    if (ImGui::Button("OK", ImVec2(120, 0))) {
                        std::thread t = std::thread(TOOLBOX_BIND_EVENT_FN(optionDeleteProc_));
                        t.detach();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                } else {
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

        m_folder_view_context_menu.renderForItem(
            "Project View", m_view_proxy->toProxyIndex(m_view_index),
            ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenOverlapped);
        m_folder_view_context_menu.applyDeferredCmds();
    }

    void ProjectViewWindow::renderProjectFolderButton() {}

    void ProjectViewWindow::renderProjectFileButton() {}

    bool ProjectViewWindow::onLoadData(const std::filesystem::path &path) {
        m_project_root = path;

        m_file_system_model = make_referable<FileSystemModel>();
        m_file_system_model->initialize();
        m_file_system_model->setRoot(m_project_root);

        m_tree_proxy = make_referable<FileSystemModelSortFilterProxy>();
        m_tree_proxy->setSourceModel(m_file_system_model);
        m_tree_proxy->setSortRole(FileSystemModelSortRole::SORT_ROLE_NAME);
        m_tree_proxy->setDirsOnly(true);

        m_view_proxy = make_referable<FileSystemModelSortFilterProxy>();
        m_view_proxy->setSourceModel(m_file_system_model);
        m_view_proxy->setSortRole(FileSystemModelSortRole::SORT_ROLE_NAME);

        m_view_index = m_file_system_model->getIndex(0, 0);
        m_file_system_model->watchPathForUpdates(m_view_index, true);

        m_selection_mgr = ModelSelectionManager(m_view_proxy);
        m_selection_mgr.setDeepSpans(false);
        return true;
    }

    void ProjectViewWindow::onAttach() {
        ImWindow::onAttach();
        buildContextMenu();

        m_rarc_processor.tStart(true, nullptr);
    }

    void ProjectViewWindow::onDetach() { ImWindow::onDetach(); }

    void ProjectViewWindow::onImGuiUpdate(TimeStep delta_time) {}

    void ProjectViewWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void ProjectViewWindow::onDragEvent(RefPtr<DragEvent> ev) {
        float x, y;
        ev->getGlobalPoint(x, y);

        if (ev->getType() == EVENT_DRAG_ENTER) {
            TOOLBOX_DEBUG_LOG("Drag enter");
            ev->accept();
        } else if (ev->getType() == EVENT_DRAG_LEAVE) {
            TOOLBOX_DEBUG_LOG("Drag leave");
            ev->accept();
        } else if (ev->getType() == EVENT_DRAG_MOVE) {
            TOOLBOX_DEBUG_LOG_V("Drag move ({}, {})", x, y);
            ev->accept();
        }
    }

    void ProjectViewWindow::onDropEvent(RefPtr<DropEvent> ev) {
        if (m_view_proxy->isReadOnly()) {
            return;
        }

        std::thread t = std::thread(TOOLBOX_BIND_EVENT_FN(evInsertProc_), ev->getMimeData());
        t.detach();

        ev->accept();
    }

    void ProjectViewWindow::buildContextMenu() {
        m_folder_view_context_menu = ContextMenu<ModelIndex>();

        ContextMenuBuilder(&m_folder_view_context_menu)
            .addOption(
                "Open", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_O}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() > 0 &&
                           std::all_of(
                               selection.begin(), selection.end(), [this](const ModelIndex &index) {
                                   return m_view_proxy->isFile(index) || isPathForScene(index);
                               });
                },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    actionOpenIndexes(selection);
                })
            .addOption(
                "Open in Explorer",
                KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_O}),
                [this](const ModelIndex &index) { return true; },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    if (selection.size() == 0) {
                        Toolbox::Platform::OpenFileExplorer(
                            m_file_system_model->getPath(m_view_index));
                    } else {
                        std::set<fs_path> paths;
                        for (const ModelIndex &item_index : selection) {
                            fs_path path = m_view_proxy->getPath(item_index);
                            if (m_view_proxy->isDirectory(item_index)) {
                                paths.insert(path);
                            } else {
                                paths.insert(path.parent_path());
                            }
                        }
                        for (const fs_path &path : paths) {
                            Toolbox::Platform::OpenFileExplorer(path);
                        }
                    }
                })
            .addDivider()
            .addOption(
                "Copy Path",
                KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_C}),
                [this](const ModelIndex &index) { return true; },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();

                    std::string paths;
                    if (selection.size() == 0) {
                        paths = m_file_system_model->getPath(m_view_index).string();
                    } else {
                        for (const ModelIndex &item_index : selection) {
                            fs_path path = m_view_proxy->getPath(item_index);
                            paths += path.string() + "\n";
                        }
                    }
                    ImGui::SetClipboardText(paths.c_str());
                })
            .addDivider()
            .addOption(
                "Cut", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_X}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() > 0;
                },
                [this](const ModelIndex &index) {
                    actionCutIndexes(m_selection_mgr.getState().getSelection());
                })
            .addOption(
                "Copy", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() > 0;
                },
                [this](const ModelIndex &index) { m_selection_mgr.actionCopySelection(); })
            .addDivider()
            .addOption(
                "Delete", KeyBind({KeyCode::KEY_DELETE}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() > 0;
                },
                [this](const ModelIndex &index) {
                    if (m_delete_without_request) {
                        std::thread t = std::thread(TOOLBOX_BIND_EVENT_FN(optionDeleteProc_));
                        t.detach();
                    } else {
                        m_delete_requested = true;
                    }
                })
            .addOption(
                "Rename", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() == 1;
                },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    actionRenameIndex(selection[0]);
                })
            .addOption(
                "Paste", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V}),
                [this](const ModelIndex &index) { return true; },
                [this](const ModelIndex &index) {
                    std::thread t = std::thread(TOOLBOX_BIND_EVENT_FN(optionPasteProc_));
                    t.detach();
                })
            .addDivider()
            .addOption(
                "Extract Here",
                KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_E}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() == 1 && m_view_proxy->isArchive(selection[0]);
                },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    fs_path arc_path = m_view_proxy->getPath(selection[0]);
                    m_rarc_processor.requestExtractArchive(arc_path, arc_path.parent_path());
                })
            .addOption(
                "Compile to RARC",
                KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_R}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() == 1 && m_view_proxy->isDirectory(selection[0]);
                },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    fs_path src_path = m_view_proxy->getPath(selection[0]);
                    fs_path dst_path = src_path;
                    dst_path.replace_extension(".arc");
                    m_rarc_processor.requestCompileArchive(src_path, dst_path, false);
                })
            .addOption(
                "Compile to SZS",
                KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_S}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() == 1 && m_view_proxy->isDirectory(selection[0]);
                },
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    fs_path src_path = m_view_proxy->getPath(selection[0]);
                    fs_path dst_path = src_path;
                    dst_path.replace_extension(".szs");
                    m_rarc_processor.requestCompileArchive(src_path, dst_path, true);
                })
            .addOption(
                "New Folder",
                KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_N}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() == 0;
                },
                [this](const ModelIndex &view_index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    std::string folder_name = m_file_system_model->findUniqueName(
                        m_view_proxy->toSourceIndex(view_index), "New Folder");
                    ModelIndex new_index = m_view_proxy->mkdir(view_index, folder_name);
                    if (m_view_proxy->validateIndex(new_index)) {
                        m_selection_mgr.actionSelectIndex(new_index, true);
                        actionRenameIndex(new_index);
                    }
                })
            .addOption(
                "New Item...", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N}),
                [this](const ModelIndex &index) {
                    const IDataModel::index_container &selection =
                        m_selection_mgr.getState().getSelection();
                    return selection.size() == 0;
                },
                [this](const ModelIndex &view_index) {
                    RefPtr<NewItemWindow> window =
                        GUIApplication::instance().createWindow<NewItemWindow>("New Item");
                    if (window) {
                        window->setContextPath(m_view_proxy->getPath(view_index));
                    }
                });
    }

    void ProjectViewWindow::actionOpenIndexes(const std::vector<ModelIndex> &indices) {
        for (auto &item_index : indices) {
            if (actionOpenPad(item_index)) {
                continue;
            }

            if (actionOpenScene(item_index)) {
                continue;
            }

            if (m_view_proxy->isDirectory(item_index)) {
                ModelIndex new_view_index = m_view_proxy->toSourceIndex(item_index);
                if (m_view_index != new_view_index) {
                    m_selection_mgr.getState().clearSelection();
                    m_view_index = new_view_index;
                    m_file_system_model->watchPathForUpdates(m_view_index, true);
                }
                continue;
            }

            // TODO: Open files based on extension. Can be either internal or external
            // depending on the file type.
        }
    }

    void ProjectViewWindow::actionCutIndexes(const std::vector<ModelIndex> &indices) {
        m_cut_indices           = indices;
        ScopePtr<MimeData> data = m_selection_mgr.actionCopySelection();
        SystemClipboard::instance().setContent(*data);
    }

    void ProjectViewWindow::actionRenameIndex(const ModelIndex &index) {
        m_is_renaming         = true;
        std::string file_name = m_view_proxy->getDisplayText(index);
        std::strncpy(m_rename_buffer, file_name.c_str(), IM_ARRAYSIZE(m_rename_buffer));
    }

    bool ProjectViewWindow::actionOpenScene(const ModelIndex &index) {
        if (!m_view_proxy->validateIndex(index)) {
            return false;
        }

        GUIApplication &app = GUIApplication::instance();

        if (m_view_proxy->isDirectory(index)) {
            // ./scene/
            fs_path scene_path = m_view_proxy->getPath(index);

            RefPtr<ImWindow> existing_editor = app.findWindow("Scene Editor", scene_path.string());
            if (existing_editor) {
                existing_editor->focus();
                return true;
            }

            size_t scene, scenario;
            app.getProjectManager().getSceneLayoutManager().getScenarioForFileName(
                scene_path.parent_path().filename().string(), scene, scenario);

            RefPtr<SceneWindow> window = app.createWindow<SceneWindow>("Scene Editor");
            window->setStageScenario((u8)scene, (u8)scenario);

            if (!window->onLoadData(scene_path)) {
                app.showErrorModal(this, name(),
                                   "Failed to open the folder as a scene!\n\n - (Check application "
                                   "log for details)");
                app.removeWindow(window);
                return false;
            }

            return true;
        } else if (m_view_proxy->isArchive(index)) {
            TOOLBOX_ERROR("[PROJECT] Archives are not supported yet");
        } else if (m_view_proxy->isFile(index)) {
            // ./scene/map/scene.bin
            fs_path scene_path = m_view_proxy->getPath(index);
            if (scene_path.filename().string() != "scene.bin") {
                return false;
            }

            fs_path scene_folder = scene_path.parent_path().parent_path();
            if (scene_folder.filename().string() != "scene") {
                return false;
            }

            RefPtr<ImWindow> existing_editor =
                app.findWindow("Scene Editor", scene_folder.string());
            if (existing_editor) {
                existing_editor->focus();
                return true;
            }

            RefPtr<SceneWindow> window = app.createWindow<SceneWindow>("Scene Editor");
            if (!window->onLoadData(scene_folder)) {
                app.showErrorModal(
                    this, name(),
                    "Failed to open the scene.bin as a scene!\n\n - (Check application "
                    "log for details)");
                app.removeWindow(window);
                return false;
            }

            return true;
        }

        return false;
    }

    bool ProjectViewWindow::actionOpenPad(const ModelIndex &index) {
        if (!m_view_proxy->isDirectory(index)) {
            return false;
        }

        GUIApplication &app = GUIApplication::instance();

        // ./scene/map/map/pad/
        fs_path pad_path = m_view_proxy->getPath(index);
        if (pad_path.filename().string() != "pad") {
            return false;
        }

        RefPtr<ImWindow> existing_editor = app.findWindow("Pad Recorder", pad_path.string());
        if (existing_editor) {
            existing_editor->focus();
            return true;
        }

        RefPtr<SceneWindow> window = app.createWindow<SceneWindow>("Pad Recorder");
        if (!window->onLoadData(pad_path)) {
            app.showErrorModal(this, name(),
                               "Failed to open the folder as a pad recording!\n\n - (Check application "
                               "log for details)");
            app.removeWindow(window);
            return false;
        }

        return true;
    }

    bool ProjectViewWindow::isPathForScene(const ModelIndex &index) const {
        if (!m_view_proxy->validateIndex(index)) {
            return false;
        }

        if (m_view_proxy->isDirectory(index)) {
            fs_path scene_path = m_view_proxy->getPath(index);
            if (scene_path.filename().string() == "scene") {
                return true;
            }
        } else if (m_view_proxy->isFile(index)) {
            fs_path scene_path = m_view_proxy->getPath(index);
            if (scene_path.filename().string() == "scene.bin") {
                fs_path scene_folder = scene_path.parent_path().parent_path();
                if (scene_folder.filename().string() == "scene") {
                    return true;
                }
            }
        } else if (m_view_proxy->isArchive(index)) {
            // fs_path
        }

        return false;
    }

    bool ProjectViewWindow::isViewedAncestor(const ModelIndex &index) {
        if (m_view_index == m_tree_proxy->toSourceIndex(index)) {
            return true;
        }
        for (size_t i = 0; i < m_tree_proxy->getRowCount(index); ++i) {
            ModelIndex child_index = m_tree_proxy->getIndex(i, 0, index);
            if (isViewedAncestor(child_index)) {
                return true;
            }
        }
        return false;
    }

    void ProjectViewWindow::renderFolderTree(const ModelIndex &index) {
        bool is_open = false;
        if (m_tree_proxy->isDirectory(index)) {
            if (m_tree_proxy->canFetchMore(index)) {
                m_tree_proxy->fetchMore(index);
            }
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
            if (m_tree_proxy->hasChildren(index)) {
                flags |= ImGuiTreeNodeFlags_OpenOnArrow;
            } else {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }

            if (m_view_index == m_tree_proxy->toSourceIndex(index)) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            if (isViewedAncestor(index)) {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }
            is_open = ImGui::TreeNodeEx(m_tree_proxy->getDisplayText(index).c_str(), flags);

            if (ImGui::IsItemClicked()) {
                ModelIndex new_view_index = m_tree_proxy->toSourceIndex(index);
                if (m_view_index != new_view_index) {
                    m_selection_mgr.getState().clearSelection();
                    m_view_index = new_view_index;
                    m_file_system_model->watchPathForUpdates(m_view_index, true);
                }
                // m_fs_watchdog.addPath(m_view_proxy->getPath(m_view_index));
            }

            if (is_open) {
                for (size_t i = 0; i < m_tree_proxy->getRowCount(index); ++i) {
                    ModelIndex child_index = m_tree_proxy->getIndex(i, 0, index);
                    if (m_tree_proxy->validateIndex(child_index)) {
                        renderFolderTree(child_index);
                    }
                }
                ImGui::TreePop();
            }
        } else {
            if (ImGui::TreeNodeEx(m_tree_proxy->getDisplayText(index).c_str(),
                                  ImGuiTreeNodeFlags_Leaf)) {
                ImGui::TreePop();
            }
        }
    }

    void ProjectViewWindow::initFolderAssets(const ModelIndex &index) {}

    void ProjectViewWindow::evInsertProc_(MimeData data) {
        ModelInsertPolicy policy = ModelInsertPolicy::INSERT_CHILD;
        (void)m_view_proxy->insertMimeData(m_view_proxy->toProxyIndex(m_view_index), data, policy);
    }

    void ProjectViewWindow::optionDeleteProc_() { m_selection_mgr.actionDeleteSelection(); }

    void ProjectViewWindow::optionPasteProc_() {
        const IDataModel::index_container &selection = m_selection_mgr.getState().getSelection();

        auto content_types = SystemClipboard::instance().getAvailableContentFormats();
        if (!content_types) {
            TOOLBOX_ERROR("Couldn't get content types");
            return;
        }

        if (std::find(content_types.value().begin(), content_types.value().end(),
                      std::string("text/uri-list")) == content_types.value().end()) {
            return;
        }
        auto maybe_data = SystemClipboard::instance().getContent();
        if (!maybe_data) {
            return;
        }

        bool success = false;

        if (selection.size() > 0) {
            success |= m_selection_mgr.actionPasteIntoSelection(maybe_data.value());
        } else {
            if (m_view_proxy->isReadOnly()) {
                return;
            }

            ModelInsertPolicy policy = ModelInsertPolicy::INSERT_CHILD;
            success |= m_view_proxy->insertMimeData(m_view_proxy->toProxyIndex(m_view_index),
                                                    maybe_data.value(), policy);
        }

        for (const ModelIndex &index : m_cut_indices) {
            (void)m_view_proxy->removeIndex(index);
        }
        m_cut_indices.clear();
    }

    bool char_equals(char a, char b) {
#ifdef TOOLBOX_PLATFORM_WINDOWS
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
#else
        return a == b;
#endif
    }
    bool ProjectViewWindow::isValidName(const std::string &name,
                                        const std::vector<ModelIndex> &selected_indices) const {
        TOOLBOX_ASSERT(selected_indices.size() == 1, "Can't rename more than one file!");

        ModelIndex parent = m_view_proxy->getParent(selected_indices[0]);
        for (size_t i = 0; i < m_view_proxy->getRowCount(parent); ++i) {
            ModelIndex child_index = m_view_proxy->getIndex(i, 0, parent);
            if (child_index == selected_indices[0]) {
                continue;
            }
            std::string child_name = m_view_proxy->getDisplayText(child_index);
            if (std::equal(child_name.begin(), child_name.end(), name.begin(), name.end(),
                           char_equals)) {
                return false;
            }
        }
        if (name.contains('/')) {
            return false;
        }
#ifdef TOOLBOX_PLATFORM_WINDOWS
        // Windows naming constraints sourced from here:
        // https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
        if (name.contains('\\') || name.contains('<') || name.contains('>') || name.contains(':') ||
            name.contains('"') || name.contains('|') || name.contains('?') || name.contains('*')) {
            return false;
        }
        if (name.back() == ' ' || name.back() == '.') {
            return false;
        }
        if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL") {
            return false;
        }
        if (name.starts_with("CON.") || name.starts_with("PRN.") || name.starts_with("AUX.") ||
            name.starts_with("NUL.")) {
            return false;
        }
        if ((name.starts_with("COM") || name.starts_with("LPT")) && name.length() == 4 &&
            (isdigit(name.back()) ||
             // The escapes for superscript 1, 2, and 3, which are
             // also disallowed after these patterns.
             name.back() == '\XB6' || name.back() == '\XB2' || name.back() == '\XB3')) {
            return false;
        }
#endif

        return true;
    }

}  // namespace Toolbox::UI
