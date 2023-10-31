#pragma once

#include <array>
#include <functional>
#include <vector>

#include "error.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "objlib/template.hpp"
#include <imgui.h>

namespace Toolbox::UI {

    class CreateObjDialog {
    public:
        using action_t = std::function<std::expected<void, BaseError>(
            std::string_view, const Object::Template &, std::string_view, NodeInfo)>;
        using cancel_t = std::function<void(NodeInfo)>;

        CreateObjDialog()  = default;
        ~CreateObjDialog() = default;

        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render(NodeInfo node_info);

    private:
        bool m_open = false;
        bool m_opening = false;

        int m_template_index = -1;
        int m_wizard_index   = -1;

        ImGuiTextFilter m_template_filter;
        ImGuiTextFilter m_wizard_filter;

        std::array<char, 128> m_object_name = {};

        action_t m_on_accept;
        cancel_t m_on_reject;

        std::vector<std::unique_ptr<Object::Template>> m_templates = {};
    };

    class RenameObjDialog {
    public:
        using action_t = std::function<std::expected<void, BaseError>(std::string_view, NodeInfo)>;
        using cancel_t = std::function<void(NodeInfo)>;

        RenameObjDialog()  = default;
        ~RenameObjDialog() = default;

        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setOriginalName(std::string_view name) {
            size_t end_pos                = std::min(name.size(), m_original_name.size() - 1);
            std::string_view cropped_name = name.substr(0, end_pos);
            std::copy(cropped_name.begin(), cropped_name.end(), m_original_name.begin());
            std::copy(cropped_name.begin(), cropped_name.end(), m_object_name.begin());
            m_original_name.at(end_pos) = '\0';
            m_object_name.at(end_pos) = '\0';
        }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render(NodeInfo node_info);

    private:
        bool m_open    = false;
        bool m_opening = false;

        std::array<char, 128> m_object_name   = {};
        std::array<char, 128> m_original_name = {};

        action_t m_on_accept;
        cancel_t m_on_reject;
    };

}  // namespace Toolbox::UI