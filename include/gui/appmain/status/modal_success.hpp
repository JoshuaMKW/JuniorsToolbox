#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"

#include "core/clipboard.hpp"
#include "core/log.hpp"
#include "core/types.hpp"
#include "gui/context_menu.hpp"
#include "gui/window.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class SuccessModal {
    public:
        SuccessModal(ImWindow *parent, const std::string &name, const std::string &message) {
            m_parent  = parent;
            m_name    = name;
            m_message = message;
        }
        ~SuccessModal() = default;

        bool is_open() const { return m_is_open; }
        bool is_closed() const { return m_is_closed; }

        bool open();
        bool render();
        void close();

    private:
        ImWindow *m_parent;
        std::string m_name;
        std::string m_message;
        bool m_is_open = false;
        bool m_is_closed = false;
    };

}  // namespace Toolbox::UI