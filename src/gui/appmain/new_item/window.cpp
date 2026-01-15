#include "gui/appmain/new_item/window.hpp"
#include "gui/appmain/application.hpp"
#include "gui/appmain/scene/window.hpp"

#include "resource/resource.hpp"

namespace Toolbox::UI {

    struct _BuiltinItemInfo {
        std::string m_name;
        std::string m_extension;
        std::string m_description;
        std::string m_icon_name;
        NewItemWindow::window_constructor m_win_factory;
    };

    static std::array<_BuiltinItemInfo, 10> s_default_items = {
        _BuiltinItemInfo("Basic Scene", "", "A minimal scene that runs in game.", "toolbox.png",
                         [](const fs_path &context_path) -> RefPtr<SceneWindow> {
                             RefPtr<SceneWindow> window =
                                 MainApplication::instance().createWindow<SceneWindow>(
                                     "Scene Editor");
                             window->initToBasic();
                             window->setIOContextPath(context_path);
                             return window;
                         }),
        _BuiltinItemInfo("Message Data", ".bmg", "A message list with metadata descriptors.",
            "FileSystem/fs_bmg.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "Parameter Data", ".prm", "A parameter sheet for tuning behaviors.",
            "FileSystem/fs_prm.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "Sunscript", ".sb", "A script format for defining high-level scene behaviors.",
            "FileSystem/fs_sb.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "DolphinOS Movie Data", ".thp",
            "A video format that displays JPEGs and plays adpcm audio.", "FileSystem/fs_thp.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "J2D Texture Image", ".bti", "A texture resource for models and UI.",
            "FileSystem/fs_bti.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "J2D Texture UV Anim", ".btk", "A texture coordinate animation for models and UI.",
            "FileSystem/fs_btk.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "J2D Texture Pattern Anim", ".btp", "A texture pattern animation for models and UI.",
            "FileSystem/fs_btp.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "J3D Model Data", ".bmd", "A 3D model format for Nintendo games.",
            "FileSystem/fs_bmd.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
        _BuiltinItemInfo(
            "JParticle Data", ".jpa",
            "A particle format defining texture resources, physics parameters, and more.",
            "FileSystem/fs_jpa.png",
            [](const fs_path &context_path) -> RefPtr<SceneWindow> { return nullptr; }),
    };

    NewItemWindow::NewItemWindow(const std::string &name)
        : ImWindow(name), m_selected_item_index(-1) {
        ResourceManager &res_manager = MainApplication::instance().getResourceManager();
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

        bool regex_valid = true;

        std::regex search_regex;
        try {
            search_regex = std::regex(m_search_buffer, std::regex::icase | std::regex::grep);
        } catch (std::regex_error &e) {
            TOOLBOX_DEBUG_LOG_V("[NEW_ITEM_WINDOW] Failed to compile regex: {}", e.what());
            regex_valid = false;
        }

        float window_height = ImGui::GetWindowSize().y;
        float table_height  = window_height - style.WindowPadding.y * 4 - style.FramePadding.y * 2;

        const ImVec2 row_size = {400, 60};

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImGui::GetCursorScreenPos() + ImVec2(row_size.x, table_height),
            ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_TableRowBgAlt]));

        if (ImGui::BeginTable("##item_list", 1,
                              ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY,
                              ImVec2(row_size.x, table_height))) {
            for (size_t i = 0; i < m_item_infos.size(); ++i) {
                const ItemInfo &info = m_item_infos[i];

#ifndef TOOLBOX_DEBUG
                if (info.m_win_factory == nullptr) {
                    continue;
                }
#endif

                bool matches_search = false;
                if (regex_valid) {
                    matches_search = std::regex_search(info.m_name, search_regex);
                } else {
                    matches_search = info.m_name.find(m_search_buffer) != std::string::npos;
                }

                if (!matches_search) {
                    continue;
                }

                ImGui::TableNextRow(ImGuiTableRowFlags_None, row_size.y);

                ImGui::GetCurrentWindow()->SkipItems =
                    false;  // Fix issue caused by table for future rows.

                bool selected = m_selected_item_index == i;

                std::string id_name = std::format("item_{}", i);
                ImGuiID bb_id = ImGui::GetID(id_name.c_str(), id_name.c_str() + id_name.size());

                ImVec2 bb_pos = ImGui::GetCursorScreenPos();

                const ImRect bb(bb_pos, bb_pos + row_size);
                if (!ImGui::ItemAdd(bb, bb_id)) {
                    TOOLBOX_DEBUG_LOG_V("[NEW_ITEM_WINDOW] Item {} is failed to add interactor", i);
                }

                bool hovered, held;
                bool pressed = ImGui::ButtonBehavior(bb, bb_id, &hovered, &held);

                if (pressed) {
                    m_selected_item_index = i;
                    selected              = true;
                }

                if (hovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::BeginGroup();
                renderItemRow(info, selected, pressed, hovered, bb_pos, row_size);
                ImGui::EndGroup();

                // ImGui::SetCursorScreenPos({pos.x, pos.y + row_size.y + style.ItemSpacing.y});
            }
            ImGui::EndTable();
        }

        ImGui::SameLine();

        ImGui::BeginGroup();

        char search_buffer[256];
        std::strncpy(search_buffer, m_search_buffer.c_str(), sizeof(search_buffer));

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputTextWithHint("##search", "Search...", search_buffer,
                                     sizeof(search_buffer))) {
            m_search_buffer = search_buffer;
        }

        ImGui::Separator();

        if (m_selected_item_index >= 0) {
            const ItemInfo &info = m_item_infos[m_selected_item_index];
            ImGui::BeginGroup();
            renderItemDescription(info, ImGui::GetCursorScreenPos(), {500, 500});
            ImGui::EndGroup();
        }

        renderControlPanel();

        ImGui::EndGroup();
    }

    void NewItemWindow::renderItemRow(const ItemInfo &info, bool selected, bool pressed,
                                      bool hovered, const ImVec2 &row_pos, const ImVec2 &row_size) {
        ImGuiStyle &style     = ImGui::GetStyle();
        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        ImVec4 bg_color;
        if (hovered || pressed) {
            bg_color = style.Colors[ImGuiCol_ButtonHovered];
        } else if (selected) {
            bg_color = style.Colors[ImGuiCol_ButtonActive];
        } else {
            bg_color = style.Colors[ImGuiCol_TableRowBg];
        }

        draw_list->AddRectFilled(row_pos, row_pos + row_size,
                                 ImGui::ColorConvertFloat4ToU32(bg_color));

        const ImVec2 icon_size = {48, 48};

        // Render icon
        {
            ImVec2 icon_pos = ImVec2(style.WindowPadding.x, row_size.y / 2 - icon_size.x / 2);

            ImagePainter painter;
            painter.render(*info.m_icon, row_pos + icon_pos, icon_size);
        }

        // Render name text
        {
            ImVec2 name_text_size = ImGui::CalcTextSize(
                info.m_name.c_str(), info.m_name.c_str() + info.m_name.size(), false, 0);
            ImVec2 name_text_pos = ImVec2(style.WindowPadding.x + icon_size.x + style.ItemSpacing.x,
                                          row_size.y / 2 - name_text_size.y / 2);

            draw_list->AddText(row_pos + name_text_pos,
                               ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
                               info.m_name.c_str(), info.m_name.c_str() + info.m_name.size());
        }

        // Render extension text
        {
            const std::string &type = info.m_extension.empty() ? "General" : info.m_extension;

            ImVec2 ext_text_size =
                ImGui::CalcTextSize(type.c_str(), type.c_str() + type.size(), false, 0);
            ImVec2 ext_text_pos = ImVec2(row_size.x - ext_text_size.x - style.WindowPadding.x - 16,
                                         row_size.y / 2 - ext_text_size.y / 2);

            draw_list->AddText(row_pos + ext_text_pos,
                               ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
                               type.c_str(), type.c_str() + type.size());
        }
    }

    void NewItemWindow::renderItemDescription(const ItemInfo &info, const ImVec2 &pos,
                                              const ImVec2 &size) {
        ImGui::Text("Type:");
        ImGui::SameLine();

        const std::string &type = info.m_extension.empty() ? "General" : info.m_extension;
        ImGui::TextWrapped(type.c_str());

        ImGui::TextWrapped(info.m_description.c_str());
    }

    void NewItemWindow::renderControlPanel() {
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 win_size = ImGui::GetWindowSize();

        ImVec2 cancel_text_size = ImGui::CalcTextSize("Cancel");
        ImVec2 open_text_size   = ImGui::CalcTextSize("Open");

        ImVec2 cancel_button_size = cancel_text_size + (style.FramePadding * 2);
        ImVec2 open_button_size   = open_text_size + (style.FramePadding * 2);

        float y_pos =
            win_size.y - style.WindowPadding.y - (style.FramePadding.y * 2) - ImGui::GetFontSize();
        ImVec2 cancel_button_pos = {win_size.x - cancel_button_size.x - style.WindowPadding.x,
                                    y_pos};
        ImVec2 open_button_pos   = {cancel_button_pos.x - open_button_size.x - style.ItemSpacing.x,
                                    y_pos};

        ImGui::SetCursorPos(open_button_pos);
        if (ImGui::Button("Open", open_button_size)) {
            if (m_selected_item_index >= 0) {
                const ItemInfo &info    = m_item_infos[m_selected_item_index];
                RefPtr<ImWindow> window = info.m_win_factory(m_context_path);
                if (!window) {
                    TOOLBOX_DEBUG_LOG("[NEW_ITEM_WINDOW] Failed to create window");
                } else {
                    close();
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