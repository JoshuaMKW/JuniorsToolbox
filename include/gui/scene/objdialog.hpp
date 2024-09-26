#pragma once

#include <array>
#include <functional>
#include <imgui.h>
#include <vector>

#include "core/error.hpp"
#include "core/memory.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "objlib/template.hpp"

namespace Toolbox::UI {

    class CreateObjDialog {
    public:
        enum class InsertPolicy {
            INSERT_BEFORE,
            INSERT_AFTER,
            INSERT_CHILD,
        };

        using action_t =
            std::function<void(size_t, std::string_view, const Object::Template &, std::string_view,
                               InsertPolicy, SelectionNodeInfo<Object::ISceneObject>)>;
        using cancel_t = std::function<void(SelectionNodeInfo<Object::ISceneObject>)>;

        CreateObjDialog()  = default;
        ~CreateObjDialog() = default;

        void setExtendedMode(bool extended) { m_extended_mode = extended; }
        void setInsertPolicy(InsertPolicy policy) { m_insert_policy = policy; }
        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render(SelectionNodeInfo<Object::ISceneObject> node_info);

    private:
        bool m_open    = false;
        bool m_opening = false;

        bool m_extended_mode = false;

        int m_template_index = -1;
        int m_wizard_index   = -1;

        ImGuiTextFilter m_template_filter;
        ImGuiTextFilter m_wizard_filter;

        std::array<char, 128> m_object_name = {};

        InsertPolicy m_insert_policy = InsertPolicy::INSERT_BEFORE;

        action_t m_on_accept;
        cancel_t m_on_reject;

        std::vector<ScopePtr<Object::Template>> m_templates = {};
    };

    class RenameObjDialog {
    public:
        using action_t =
            std::function<void(std::string_view, SelectionNodeInfo<Object::ISceneObject>)>;
        using cancel_t = std::function<void(SelectionNodeInfo<Object::ISceneObject>)>;

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
            m_object_name.at(end_pos)   = '\0';
        }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render(SelectionNodeInfo<Object::ISceneObject> node_info);

    private:
        bool m_open    = false;
        bool m_opening = false;

        std::array<char, 128> m_object_name   = {};
        std::array<char, 128> m_original_name = {};

        action_t m_on_accept;
        cancel_t m_on_reject;
    };

}  // namespace Toolbox::UI