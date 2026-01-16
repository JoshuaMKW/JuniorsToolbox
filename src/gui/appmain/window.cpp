#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "gui/window.hpp"

#include "gui/appmain/window.hpp"

#include "gui/appmain/application.hpp"
#include "gui/appmain/debugger/window.hpp"
#include "gui/appmain/pad/window.hpp"
#include "gui/appmain/project/window.hpp"
#include "gui/appmain/scene/window.hpp"
#include "gui/appmain/settings/window.hpp"

namespace Toolbox::UI {

    using window_ctor_t = std::function<RefPtr<ImWindow>(const std::string &name)>;

#define TOOLBOX_WINDOW_ENTRY(WindowClass)                                                          \
    {TOOLBOX_STRINGIFY_MACRO(WindowClass),                                                         \
     [](const std::string &name) -> ::Toolbox::RefPtr<::Toolbox::UI::ImWindow> {                   \
         return ::Toolbox::MainApplication::instance().createWindow<WindowClass>(name);            \
     }}

#define TOOLBOX_WINDOW_ENTRY_WITH_ALIAS(WindowClass, Alias)                                        \
    {TOOLBOX_STRINGIFY_MACRO(WindowClass),                                                         \
     [](const std::string &name) -> ::Toolbox::RefPtr<::Toolbox::UI::ImWindow> {                   \
         return ::Toolbox::MainApplication::instance().createWindow<WindowClass>(Alias);           \
     }}

    Toolbox::RefPtr<ImWindow> WindowFactory::create(const WindowArguments &args) {
        static const std::unordered_map<std::string, window_ctor_t> window_ctor_map = {
            TOOLBOX_WINDOW_ENTRY_WITH_ALIAS(DebuggerWindow, "Memory Debugger"),
            TOOLBOX_WINDOW_ENTRY_WITH_ALIAS(PadInputWindow, "Pad Recorder"),
            TOOLBOX_WINDOW_ENTRY_WITH_ALIAS(ProjectViewWindow, "Project View"),
            TOOLBOX_WINDOW_ENTRY_WITH_ALIAS(SceneWindow, "Scene Editor"),
            TOOLBOX_WINDOW_ENTRY_WITH_ALIAS(SettingsWindow, "Application Settings")};

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