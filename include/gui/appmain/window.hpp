#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fsystem.hpp"
#include "core/memory.hpp"
#include "gui/window.hpp"

namespace Toolbox::UI {

    struct WindowArguments {
        std::string m_window_type;
        std::optional<Toolbox::fs_path> m_load_path;
        std::vector<std::string> m_vargs;
    };

    class WindowFactory {
    public:
        static Toolbox::RefPtr<ImWindow> create(const WindowArguments &args);
    };

}