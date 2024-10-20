#include "gui/new_item/window.hpp"
#include "gui/application.hpp"
#include "gui/scene/window.hpp"

#include "resource/resource.hpp"

namespace Toolbox::UI {

    struct _BuiltinItemInfo {
        std::string m_name;
        std::string m_extension;
        std::string m_description;
        std::string m_icon_name;
        NewItemWindow::window_constructor m_win_factory;
    };

    static std::array<_BuiltinItemInfo, 2> s_default_items = {
        _BuiltinItemInfo("Basic Scene", "", "A minimal scene that runs in game.", "toolbox.png",
                         []() -> RefPtr<SceneWindow> {
                             RefPtr<SceneWindow> window =
                                 GUIApplication::instance().createWindow<SceneWindow>(
                                     "Scene Editor");
                             window->initToBasic();
                             return window;
                         }),
        _BuiltinItemInfo("J3D Texture Image", ".bti", "A texture resource for models and UI.",
                         "FileSystem/fs_bti.png", []() -> RefPtr<SceneWindow> { return nullptr; }),
    };

    NewItemWindow::NewItemWindow(const std::string &name)
        : ImWindow(name), m_selected_item_index(-1) {
        ResourceManager &res_manager = GUIApplication::instance().getResourceManager();
        UUID64 icon_path_uuid        = res_manager.getResourcePathUUID("Images/Icons");
        m_item_infos.resize(s_default_items.size());
        std::transform(
            s_default_items.begin(), s_default_items.end(), m_item_infos.begin(),
            [&res_manager, &icon_path_uuid](const _BuiltinItemInfo &info) {
                auto result = res_manager.getImageHandle(info.m_icon_name, icon_path_uuid);
                if (!result) {
                    TOOLBOX_ERROR_V("[NEW_ITEM_WINDOW] Failed to find icon \"{}\" for item \"{}\"",
                                    info.m_icon_name, info.m_name);
                    return ItemInfo(info.m_name, info.m_extension, info.m_description, nullptr,
                                    info.m_win_factory);
                }
                return ItemInfo(info.m_name, info.m_extension, info.m_description, result.value(),
                                info.m_win_factory);
            });
    }

    void NewItemWindow::onRenderBody(TimeStep delta_time) {
        ImGuiStyle &style = ImGui::GetStyle();

        ImGui::Text("Search: ");
        char search_buffer[256];
        std::strncpy(search_buffer, m_search_buffer.c_str(), sizeof(search_buffer));
        if (ImGui::InputText("##search", search_buffer, sizeof(search_buffer))) {
            m_search_buffer = search_buffer;
        }

        std::regex search_regex(m_search_buffer, std::regex::icase);

        if (ImGui::BeginTable("##item_list", 1)) {
            const ImVec2 row_size = {500, 60};

            for (size_t i = 0; i < m_item_infos.size(); ++i) {
                const ItemInfo &info = m_item_infos[i];
                if (m_search_buffer.size() > 0 && !std::regex_search(info.m_name, search_regex)) {
                    continue;
                }

                bool selected = m_selected_item_index == i;

                std::string id_name = std::format("item_{}", i);
                ImGuiID bb_id = ImGui::GetID(id_name.c_str(), id_name.c_str() + id_name.size());

                ImVec2 bb_pos = ImGui::GetCursorPos();
                ImVec2 pos    = ImGui::GetCursorScreenPos();

                const ImRect bb(pos, pos + row_size);
                if (!ImGui::ItemAdd(bb, bb_id)) {
                    TOOLBOX_DEBUG_LOG_V("[NEW_ITEM_WINDOW] Item {} is failed to add interactor", i);
                }

                bool hovered, held;
                bool pressed = ImGui::ButtonBehavior(bb, bb_id, &hovered, &held);

                if (pressed) {
                    m_selected_item_index = i;
                    selected              = true;
                }

                ImVec4 bg_color;
                if (hovered || pressed) {
                    bg_color = style.Colors[ImGuiCol_ButtonHovered];
                } else if (selected) {
                    bg_color = style.Colors[ImGuiCol_ButtonActive];
                } else {
                    bg_color = style.Colors[ImGuiCol_TableRowBg];
                }

                ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_color);

                if (ImGui::BeginChild(id_name.c_str(), row_size, ImGuiChildFlags_Borders,
                                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) {
                    ImVec2 win_size = ImGui::GetWindowSize();
                    ImGui::PushID(i);
                    renderItemRow(info, selected, win_size);
                    ImGui::PopID();
                }
                ImGui::EndChild();

                if (i < m_item_infos.size() - 1) {
                    ImGui::TableNextRow();
                }

                ImGui::SetCursorScreenPos({pos.x, pos.y + row_size.y + style.ItemSpacing.y});

                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        renderControlPanel();
    }

    void NewItemWindow::renderItemRow(const ItemInfo &info, bool selected, const ImVec2 &row_size) {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec2 window_pos = ImGui::GetWindowPos();

        const ImVec2 icon_size = {48, 48};

        // Render icon
        {
            ImVec2 icon_pos = ImVec2(style.WindowPadding.x, row_size.y / 2 - icon_size.x / 2);

            ImagePainter painter;
            painter.render(*info.m_icon, window_pos + icon_pos, icon_size);
        }

        // Render name text
        {
            ImVec2 name_text_size = ImGui::CalcTextSize(
                info.m_name.c_str(), info.m_name.c_str() + info.m_name.size(), false, 0);
            ImVec2 name_text_pos = ImVec2(style.WindowPadding.x + icon_size.x + style.ItemSpacing.x,
                                          row_size.y / 2 - name_text_size.y / 2);

            ImGui::SetCursorPos(name_text_pos);
            ImGui::Text(info.m_name.c_str());
        }

        // Render extension text
        {
            ImVec2 ext_text_size =
                ImGui::CalcTextSize(info.m_extension.c_str(),
                                    info.m_extension.c_str() + info.m_extension.size(), false, 0);
            ImVec2 ext_text_pos = ImVec2(row_size.x - ext_text_size.x - style.WindowPadding.x,
                                         row_size.y / 2 - ext_text_size.y / 2);

            ImGui::SetCursorPos(ext_text_pos);
            ImGui::Text(info.m_extension.c_str());
        }
    }

    void NewItemWindow::renderItemDescription(const std::string &description, const ImVec2 &size) {
        ImGui::TextWrapped(description.c_str());
    }

    void NewItemWindow::renderControlPanel() {
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 win_size = ImGui::GetWindowSize();

        ImVec2 cancel_text_size = ImGui::CalcTextSize("Cancel");
        ImVec2 open_text_size   = ImGui::CalcTextSize("Open");

        ImVec2 cancel_button_size = cancel_text_size + (style.FramePadding * 2);
        ImVec2 open_button_size   = open_text_size + (style.FramePadding * 2);

        float y_pos              = ImGui::GetCursorPosY();
        ImVec2 cancel_button_pos = {win_size.x - cancel_button_size.x - style.WindowPadding.x,
                                    y_pos};
        ImVec2 open_button_pos   = {cancel_button_pos.x - open_button_size.x - style.ItemSpacing.x,
                                    y_pos};

        ImGui::SetCursorPos(open_button_pos);
        if (ImGui::Button("Open", open_button_size)) {
            if (m_selected_item_index >= 0) {
                const ItemInfo &info    = m_item_infos[m_selected_item_index];
                RefPtr<ImWindow> window = info.m_win_factory();
                if (!window) {
                    TOOLBOX_DEBUG_LOG("[NEW_ITEM_WINDOW] Failed to create window");
                }
            }
        }

        ImGui::SetCursorPos(cancel_button_pos);
        if (ImGui::Button("Cancel", cancel_button_size)) {
            close();
        }
    }

    void NewItemWindow::onAttach() {}

    void NewItemWindow::onDetach() {}

    void NewItemWindow::onImGuiUpdate(TimeStep delta_time) {}

}  // namespace Toolbox::UI