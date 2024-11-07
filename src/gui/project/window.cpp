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

                ModelIndex view_index = m_view_proxy.toProxyIndex(m_view_index);

                for (size_t i = 0; i < m_view_proxy.getRowCount(m_view_index); ++i) {
                    ModelIndex child_index = m_view_proxy.getIndex(i, 0, view_index);
                    if (!m_view_proxy.validateIndex(child_index)) {
                        TOOLBOX_DEBUG_LOG_V("Invalid index: {}", i);
                        continue;
                    }

                    ModelIndex source_child_index = m_view_proxy.toSourceIndex(child_index);

                    ImVec2 win_min = ImGui::GetCursorScreenPos();
                    ImVec2 win_max = win_min + ImVec2(box_width, box_height);

                    bool is_selected =
                        std::find(m_selected_indices.begin(), m_selected_indices.end(),
                                  source_child_index) != m_selected_indices.end();

                    if (is_selected) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  ImGui::GetStyleColorVec4(ImGuiCol_TabSelected)));
                    } else if (ImGui::IsMouseHoveringRect(win_min, win_max)) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  ImGui::GetStyleColorVec4(ImGuiCol_TabHovered)));
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

                    const float text_width = is_selected_rename
                                                 ? std::min(rename_size.x, label_width)
                                                 : std::min(text_size.x, label_width);

                    ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildWindow |
                                                    ImGuiWindowFlags_NoDecoration |
                                                    ImGuiWindowFlags_NoScrollWithMouse;

                    if (source_child_index == m_last_selected_index) {
                        ImGui::GetWindowDrawList()->AddRect(
                            win_min - (style.FramePadding / 2.0f),
                            win_max + (style.FramePadding / 2.0f),
                            ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]), 3.0f,
                            ImDrawFlags_RoundCornersAll, 2.0f);
                    }

                    if (ImGui::BeginChild(child_index.getUUID(), {box_width, 100.0f}, true,
                                          window_flags)) {
                        ImVec2 pos       = ImGui::GetCursorScreenPos() + style.WindowPadding;
                        ImVec2 icon_size = ImVec2(box_width - style.WindowPadding.x * 2.0f,
                                                  box_width - style.WindowPadding.x * 2.0f);
                        m_icon_painter.render(*m_view_proxy.getDecoration(child_index), pos,
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
                                    m_is_valid_name =
                                        isValidName(m_rename_buffer, m_selected_indices);
                                }
                                if (ImGui::IsItemDeactivatedAfterEdit() && m_is_valid_name) {
                                    if (std::strlen(m_rename_buffer) == 0) {
                                        TOOLBOX_DEBUG_LOG(
                                            "Attempted to rename to the empty string, ignoring.");
                                    } else {
                                        m_file_system_model->rename(
                                            m_view_proxy.toSourceIndex(child_index),
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
                                                          text_clip_max, ellipsis_max, ellipsis_max,
                                                          text.c_str(), nullptr, nullptr);
                            }
                        }

                        // Handle click responses
                        {
                            any_items_hovered |= ImGui::IsItemHovered() ||
                                                 ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
                            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) {
                                if (is_double_left_click) {
                                    m_is_renaming = false;
                                    if (m_view_proxy.isDirectory(child_index)) {
                                        m_view_index = m_view_proxy.toSourceIndex(child_index);
                                        ImGui::EndChild();
                                        ImGui::PopStyleColor(1);
                                        break;
                                    }
                                } else {
                                    if ((is_left_click && !is_left_click_release) ||
                                        (is_right_click && !is_right_click_release)) {
                                        m_is_renaming = false;
                                        actionSelectIndex(view_index, child_index, is_selected);
                                    } else if (is_left_click_release || is_right_click_release) {
                                        actionClearRequestExcIndex(view_index, child_index,
                                                                   is_left_click_release);
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

                if (!any_items_hovered && (is_left_click_release || is_right_click_release)) {
                    actionClearRequestExcIndex(view_index, ModelIndex(), is_left_click_release);
                    m_is_renaming = false;
                }

                if (any_items_hovered && is_left_drag && m_selected_indices.size() > 0 &&
                    ImLengthSqr(mouse_pos - m_last_reg_mouse_pos) > 10.0f) {
                    if (DragDropManager::instance().getCurrentDragAction() == nullptr) {
                        RefPtr<DragAction> action = DragDropManager::instance().createDragAction(
                            getUUID(), getLowHandle(), std::move(buildFolderViewMimeData()));
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

                                draw_list->AddText(pos + (size / 2.0f) - (text_size / 2.0f),
                                                   IM_COL32_WHITE, file_count.c_str(),
                                                   file_count.c_str() + file_count.size());
                            });
                        }
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

        m_folder_view_context_menu.render("Project View", m_view_index,
                                          ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                                              ImGuiHoveredFlags_AllowWhenOverlapped);
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
        m_view_index = m_file_system_model->getIndex(0, 0);
        return true;
    }

    void ProjectViewWindow::onAttach() {
        ImWindow::onAttach();
        buildContextMenu();
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
        MimeData data = ev->getMimeData();
        auto urls     = data.get_urls();
        if (!urls) {
            TOOLBOX_ERROR("Passed mime data did not represent files!");
            return;
        }
        std::vector<fs_path> paths;
        paths.reserve(urls.value().size());
        for (auto &url : urls.value()) {
            paths.push_back(url);
        }
        actionPasteIntoIndex(m_view_index, paths);
        ev->accept();
    }

    void ProjectViewWindow::buildContextMenu() {
        m_folder_view_context_menu = ContextMenu<ModelIndex>();

        m_folder_view_context_menu.onOpen(
            [this](const ModelIndex &index) { m_selected_indices_ctx = m_selected_indices; });

        m_folder_view_context_menu.addOption(
            "Open", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_O}),
            [this]() {
                return m_selected_indices_ctx.size() > 0 &&
                       std::all_of(m_selected_indices_ctx.begin(), m_selected_indices_ctx.end(),
                                   [this](const ModelIndex &index) {
                                       return m_file_system_model->isFile(index) ||
                                              isPathForScene(index);
                                   });
            },
            [this](auto) { actionOpenIndexes(m_selected_indices_ctx); });

        m_folder_view_context_menu.addOption(
            "Open in Explorer",
            KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_O}),
            [this]() {
                return std::all_of(
                    m_selected_indices_ctx.begin(), m_selected_indices_ctx.end(),
                    [this](const ModelIndex &index) { return m_file_system_model->isFile(index); });
            },
            [this](auto) {
                if (m_selected_indices_ctx.size() == 0) {
                    Toolbox::Platform::OpenFileExplorer(m_file_system_model->getPath(m_view_index));
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
                        Toolbox::Platform::OpenFileExplorer(path);
                    }
                }
            });

        m_folder_view_context_menu.addDivider();

        m_folder_view_context_menu.addOption(
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

        m_folder_view_context_menu.addDivider();

        m_folder_view_context_menu.addOption(
            "Cut", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_X}),
            [this]() { return m_selected_indices_ctx.size() > 0; }, [this](auto) {});

        m_folder_view_context_menu.addOption(
            "Copy", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C}),
            [this]() { return m_selected_indices_ctx.size() > 0; },
            [this](auto) { actionCopyIndexes(m_selected_indices_ctx); });

        m_folder_view_context_menu.addDivider();

        m_folder_view_context_menu.addOption(
            "Delete", KeyBind({KeyCode::KEY_DELETE}),
            [this]() { return m_selected_indices_ctx.size() > 0; },
            [this](auto) {
                if (m_delete_without_request) {
                    actionDeleteIndexes(m_selected_indices_ctx);
                } else {
                    m_delete_requested = true;
                }
            });

        m_folder_view_context_menu.addOption(
            "Rename", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R}),
            [this]() { return m_selected_indices_ctx.size() == 1; },
            [this](auto) { actionRenameIndex(m_selected_indices_ctx[0]); });

        m_folder_view_context_menu.addOption(
            "Paste", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V}),
            [this]() { return m_selected_indices_ctx.size() == 0; },
            [this](auto) {
                auto content_types = SystemClipboard::instance().getAvailableContentFormats();
                if (!content_types) {
                    TOOLBOX_ERROR("Couldn't get content types");
                    return;
                }

                if (std::find(content_types.value().begin(), content_types.value().end(),
                              std::string("text/uri-list")) == content_types.value().end()) {
                    return;
                }
                auto maybe_data = SystemClipboard::instance().getFiles();

                std::vector<fs_path> data = maybe_data.value();

                actionPasteIntoIndex(m_view_index, data);
            });

        m_folder_view_context_menu.addDivider();

        m_folder_view_context_menu.addOption(
            "New Folder",
            KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_N}),
            [this]() { return m_selected_indices_ctx.size() == 0; },
            [this](const ModelIndex &view_index) {
                std::string folder_name =
                    m_file_system_model->findUniqueName(view_index, "New Folder");
                ModelIndex new_index = m_file_system_model->mkdir(view_index, folder_name);
                if (m_file_system_model->validateIndex(new_index)) {
                    m_selected_indices.clear();
                    m_selected_indices.push_back(new_index);
                    m_selected_indices_ctx = m_selected_indices;
                    actionRenameIndex(new_index);
                }
            });

        m_folder_view_context_menu.addOption(
            "New Item...", KeyBind({KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N}),
            [this]() { return m_selected_indices_ctx.size() == 0; },
            [this](const ModelIndex &view_index) {
                RefPtr<NewItemWindow> window =
                    GUIApplication::instance().createWindow<NewItemWindow>("New Item");
                if (window) {
                    window->setContextPath(m_file_system_model->getPath(view_index));
                }
            });
    }

    MimeData ProjectViewWindow::buildFolderViewMimeData() const {
        MimeData result;

        std::vector<std::string> paths;
        for (const ModelIndex &index : m_selected_indices) {
            paths.push_back(m_file_system_model->getPath(index).string());
        }

        result.set_urls(paths);
        return result;
    }

    void ProjectViewWindow::actionDeleteIndexes(std::vector<ModelIndex> &indices) {
        for (auto &item_index : indices) {
            if (m_file_system_model->isDirectory(item_index)) {
                m_file_system_model->rmdir(item_index);
            } else {
                m_file_system_model->remove(item_index);
            }
        }
        indices.clear();
    }

    void ProjectViewWindow::actionOpenIndexes(const std::vector<ModelIndex> &indices) {
        for (auto &item_index : indices) {
            if (actionOpenPad(item_index)) {
                continue;
            }

            if (actionOpenScene(item_index)) {
                continue;
            }

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

    void ProjectViewWindow::actionPasteIntoIndex(const ModelIndex &index,
                                                 const std::vector<fs_path> &paths) {
        for (const fs_path &src_path : paths) {
            m_file_system_model->copy(src_path, index, src_path.filename().string());
        }
    }

    void ProjectViewWindow::actionCopyIndexes(const std::vector<ModelIndex> &indices) {

        std::vector<std::string> copied_paths;
        for (const ModelIndex &index : indices) {
#ifdef TOOLBOX_PLATFORM_LINUX
            const char *prefix = "file://";
#elif defined TOOLBOX_PLATFORM_WINDOWS
            const char *prefix = "file:///";
#endif
            copied_paths.push_back(prefix + m_file_system_model->getPath(index).string());
        }

        MimeData data;
        data.set_urls(copied_paths);

        auto result = SystemClipboard::instance().setContent(data);
        if (!result) {
            TOOLBOX_ERROR("[PROJECT] Failed to set contents of clipboard");
        }
    }

    void ProjectViewWindow::actionSelectIndex(const ModelIndex &view_index,
                                              const ModelIndex &child_index, bool is_selected) {
        if (m_did_drag_drop) {
            return;
        }

        ModelIndex source_child_index = m_view_proxy.toSourceIndex(child_index);
        if (Input::GetKey(KeyCode::KEY_LEFTCONTROL) || Input::GetKey(KeyCode::KEY_RIGHTCONTROL)) {
            if (is_selected) {
                m_selected_indices.erase(std::remove(m_selected_indices.begin(),
                                                     m_selected_indices.end(), source_child_index));
            } else {
                m_selected_indices.push_back(source_child_index);
            }
            m_last_selected_index = source_child_index;
        } else {
            if (Input::GetKey(KeyCode::KEY_LEFTSHIFT) || Input::GetKey(KeyCode::KEY_RIGHTSHIFT)) {
                m_selected_indices.clear();
                if (m_file_system_model->validateIndex(m_last_selected_index)) {
                    int64_t this_row = m_view_proxy.getRow(child_index);
                    int64_t last_row =
                        m_view_proxy.getRow(m_view_proxy.toProxyIndex(m_last_selected_index));
                    if (this_row > last_row) {
                        for (int64_t i = last_row; i <= this_row; ++i) {
                            ModelIndex span_index = m_view_proxy.getIndex(i, 0, view_index);
                            m_selected_indices.push_back(m_view_proxy.toSourceIndex(span_index));
                        }
                    } else {
                        for (int64_t i = this_row; i <= last_row; ++i) {
                            ModelIndex span_index = m_view_proxy.getIndex(i, 0, view_index);
                            m_selected_indices.push_back(m_view_proxy.toSourceIndex(span_index));
                        }
                    }
                } else {
                    m_selected_indices.push_back(source_child_index);
                }
            } else {
                if (is_selected) {
                    if (m_selected_indices.size() < 2) {
                        m_selected_indices.clear();
                        m_selected_indices.push_back(source_child_index);
                    }
                    m_last_selected_index = source_child_index;
                } else {
                    m_selected_indices.clear();
                    m_selected_indices.push_back(source_child_index);
                    m_last_selected_index = source_child_index;
                }
            }
        }
    }

    void ProjectViewWindow::actionClearRequestExcIndex(const ModelIndex &view_index,
                                                       const ModelIndex &child_index,
                                                       bool is_left_button) {
        if (m_did_drag_drop) {
            return;
        }

        if (Input::GetKey(KeyCode::KEY_LEFTCONTROL) || Input::GetKey(KeyCode::KEY_RIGHTCONTROL)) {
            return;
        }

        if (Input::GetKey(KeyCode::KEY_LEFTSHIFT) || Input::GetKey(KeyCode::KEY_RIGHTSHIFT)) {
            return;
        }

        if (is_left_button) {
            if (m_selected_indices.size() > 0) {
                m_selected_indices.clear();
                m_last_selected_index = ModelIndex();
                if (m_view_proxy.validateIndex(child_index)) {
                    ModelIndex source_index = m_view_proxy.toSourceIndex(child_index);
                    m_selected_indices.push_back(source_index);
                    m_last_selected_index = source_index;
                }
            }
        } else {
            if (!m_view_proxy.validateIndex(child_index)) {
                m_selected_indices.clear();
                m_last_selected_index = ModelIndex();
            }
            m_selected_indices_ctx = m_selected_indices;
            m_last_selected_index  = ModelIndex();
        }
    }

    bool ProjectViewWindow::actionOpenScene(const ModelIndex &index) {
        if (!m_file_system_model->validateIndex(index)) {
            return false;
        }

        GUIApplication &app = GUIApplication::instance();

        if (m_file_system_model->isDirectory(index)) {
            // ./scene/
            fs_path scene_path = m_file_system_model->getPath(index);

            RefPtr<ImWindow> existing_editor = app.findWindow("Scene Editor", scene_path.string());
            if (existing_editor) {
                existing_editor->focus();
                return true;
            }

            RefPtr<SceneWindow> window = app.createWindow<SceneWindow>("Scene Editor");
            if (!window->onLoadData(scene_path)) {
                app.removeWindow(window);
                return false;
            }

            return true;
        } else if (m_file_system_model->isArchive(index)) {
            TOOLBOX_ERROR("[PROJECT] Archives are not supported yet");
        } else if (m_file_system_model->isFile(index)) {
            // ./scene/map/scene.bin
            fs_path scene_path = m_file_system_model->getPath(index);
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
                app.removeWindow(window);
                return false;
            }

            return true;
        }

        return false;
    }

    bool ProjectViewWindow::actionOpenPad(const ModelIndex &index) {
        if (!m_file_system_model->isDirectory(index)) {
            return false;
        }

        GUIApplication &app = GUIApplication::instance();

        // ./scene/map/map/pad/
        fs_path pad_path = m_file_system_model->getPath(index);
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
            app.removeWindow(window);
            return false;
        }

        return true;
    }

    bool ProjectViewWindow::isPathForScene(const ModelIndex &index) const {
        if (!m_file_system_model->validateIndex(index)) {
            return false;
        }

        if (m_file_system_model->isDirectory(index)) {
            fs_path scene_path = m_file_system_model->getPath(index);
            if (scene_path.filename().string() == "scene") {
                return true;
            }
        } else if (m_file_system_model->isFile(index)) {
            fs_path scene_path = m_file_system_model->getPath(index);
            if (scene_path.filename().string() == "scene.bin") {
                fs_path scene_folder = scene_path.parent_path().parent_path();
                if (scene_folder.filename().string() == "scene") {
                    return true;
                }
            }
        }

        return false;
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
        if (m_tree_proxy.isDirectory(index)) {
            if (m_tree_proxy.canFetchMore(index)) {
                m_tree_proxy.fetchMore(index);
            }
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
            if (m_tree_proxy.hasChildren(index)) {
                flags |= ImGuiTreeNodeFlags_OpenOnArrow;
            } else {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }

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

        ModelIndex parent = m_file_system_model->getParent(selected_indices[0]);
        for (size_t i = 0; i < m_file_system_model->getRowCount(parent); ++i) {
            ModelIndex child_index = m_file_system_model->getIndex(i, 0, parent);
            if (child_index == selected_indices[0]) {
                continue;
            }
            std::string child_name = m_file_system_model->getDisplayText(child_index);
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
        if ((name.starts_with("COM") ||
             name.starts_with("LPT")) &&
            name.length() == 4 &&
            (isdigit(name.back()) ||
             // The escapes for superscript 1, 2, and 3, which are
             // also disallowed after these patterns.
             name.back() == '\XB6' ||
             name.back() == '\XB2' ||
             name.back() == '\XB3')){
            return false;
        }
#endif

        return true;
    }

}  // namespace Toolbox::UI
