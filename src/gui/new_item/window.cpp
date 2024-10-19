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
        _BuiltinItemInfo("J3D Texture Image", "", "A texture resource for models and UI.",
                         "FileSystem/fs_bti.png", []() -> RefPtr<SceneWindow> { return nullptr; }),
    };

    NewItemWindow::NewItemWindow(const std::string &name) : ImWindow(name) {
        ResourceManager &res_manager = GUIApplication::instance().getResourceManager();
        UUID64 icon_path_uuid        = res_manager.getResourcePathUUID("Images/Icons");
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
        ImGui::Text("Search: ");
        char search_buffer[256];
        std::strncpy(search_buffer, m_search_buffer.c_str(), sizeof(search_buffer));
        if (ImGui::InputText("##search", search_buffer, sizeof(search_buffer))) {
            m_search_buffer = search_buffer;
        }

        std::regex search_regex(m_search_buffer, std::regex::icase);

        if (ImGui::BeginTable("##item_list", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
            for (size_t i = 0; i < m_item_infos.size(); ++i) {
                const ItemInfo &info = m_item_infos[i];
                if (m_search_buffer.size() > 0 && !std::regex_search(info.m_name, search_regex)) {
                    continue;
                }

                bool selected = m_selected_item_index == i;

                // TODO: Make selectable
                std::string id_name = std::format("##item_{}", i);
                if (ImGui::BeginChild(id_name.c_str(), {0, 100}, true)) {
                    ImGui::PushID(i);
                    renderItemRow(info, selected);
                    ImGui::PopID();
                    ImGui::EndChild();
                }

                ImGui::TableNextRow();
            }
            ImGui::EndChild();
        }

        renderControlPanel();
    }

    void NewItemWindow::renderItemRow(const ItemInfo &info, bool selected) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});

        ImGui::PopStyleVar(2);

        ImGui::SameLine();

        ImagePainter painter;
        painter.render(*info.m_icon, ImVec2(32, 32));

        ImGui::SameLine();
        ImGui::Text(info.m_name.c_str());

        renderItemDescription(info.m_description);
    }

    void NewItemWindow::renderItemDescription(const std::string &description) {}

    void NewItemWindow::renderControlPanel() {}

    void NewItemWindow::onAttach() {}

    void NewItemWindow::onDetach() {}

    void NewItemWindow::onImGuiUpdate(TimeStep delta_time) {}

}  // namespace Toolbox::UI