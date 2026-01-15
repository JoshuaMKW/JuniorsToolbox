#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "fsystem.hpp"
#include "gui/window.hpp"
#include "core/memory.hpp"

#include "gui/appmain/window.hpp"

#include "gui/appmain/application.hpp"
#include "gui/appmain/debugger/window.hpp"
#include "gui/appmain/pad/window.hpp"
#include "gui/appmain/project/window.hpp"
#include "gui/appmain/scene/window.hpp"
#include "gui/appmain/settings/window.hpp"

namespace Toolbox::UI {

#define TOOLBOX_WINDOW_ENTRY(WindowClass)                                                          \
    {TOOLBOX_STRINGIFY_MACRO(WindowClass),                                                         \
     [](const std::string &name) -> ::Toolbox::RefPtr<::Toolbox::UI::ImWindow> {               \
         return ::Toolbox::MainApplication::instance().createWindow<WindowClass>(name);        \
     }}

    Toolbox::RefPtr<ImWindow> WindowFactory::create(const WindowArguments &args) {
        static const std::unordered_map<std::string, window_ctor_t> window_ctor_map = {
            TOOLBOX_WINDOW_ENTRY(DebuggerWindow), TOOLBOX_WINDOW_ENTRY(PadInputWindow),
            TOOLBOX_WINDOW_ENTRY(ProjectViewWindow), TOOLBOX_WINDOW_ENTRY(SceneWindow),
            TOOLBOX_WINDOW_ENTRY(SettingsWindow)};

        if (window_ctor_map.contains(args.m_window_type)) {
            Toolbox::RefPtr<ImWindow> window =
                window_ctor_map.at(args.m_window_type)(args.m_window_type);
            if (args.m_load_path.has_value()) {
                (void)window->onLoadData(args.m_load_path.value());
            }
            return window;
        }

        return nullptr;
    }

}  // namespace Toolbox::UI