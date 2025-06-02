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

    class AddGroupDialog {
    public:
        enum class InsertPolicy {
            INSERT_BEFORE,
            INSERT_AFTER,
            INSERT_CHILD,
        };

        using action_t =
            std::function<void(size_t, InsertPolicy, std::string_view)>;
        using cancel_t = std::function<void(size_t)>;
        using filter_t = std::function<bool(std::string_view)>;

        AddGroupDialog()  = default;
        ~AddGroupDialog() = default;

        void setInsertPolicy(InsertPolicy policy) { m_insert_policy = policy; }
        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setFilterPredicate(filter_t filter) { m_filter_predicate = std::move(filter); }

        void setup();

        void open() {
            m_opening = true;
        }
        void render(size_t selection_index);

    private:
        bool m_open    = false;
        bool m_opening = false;

        std::array<char, 128> m_group_name = {};

        InsertPolicy m_insert_policy = InsertPolicy::INSERT_BEFORE;

        action_t m_on_accept;
        cancel_t m_on_reject;
        filter_t m_filter_predicate;
    };

}  // namespace Toolbox::UI