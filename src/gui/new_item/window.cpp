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

    void NewItemWindow::onRenderBody(TimeStep delta_time) {}

    void NewItemWindow::renderItemRow() {}

    void NewItemWindow::renderItemDescription() {}

    void NewItemWindow::renderControlPanel() {}

    void NewItemWindow::onAttach() {}

    void NewItemWindow::onDetach() {}

    void NewItemWindow::onImGuiUpdate(TimeStep delta_time) {}

}  // namespace Toolbox::UI