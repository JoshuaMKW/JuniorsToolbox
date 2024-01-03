#pragma once

#include <array>
#include <functional>
#include <vector>

#include "error.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "rail/rail.hpp"
#include <imgui.h>

namespace Toolbox::UI {

    class CreateRailDialog {
    public:
        enum class InitialShape { NONE, CIRCLE };

        using action_t = std::function<void(std::string_view, u16, s16, bool)>;
        using cancel_t = std::function<void(SelectionNodeInfo<Rail::Rail>)>;

        CreateRailDialog()  = default;
        ~CreateRailDialog() = default;

        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render(SelectionNodeInfo<Rail::Rail> node_info);

    private:
        bool m_open    = false;
        bool m_opening = false;

        u16 m_node_count = 0;
        s16 m_node_distance   = 0;
        bool m_loop      = false;

        std::array<char, 128> m_rail_name = {};

        action_t m_on_accept;
        cancel_t m_on_reject;
    };

    class RenameRailDialog {
    public:
        using action_t = std::function<void(std::string_view, SelectionNodeInfo<Rail::Rail>)>;
        using cancel_t = std::function<void(SelectionNodeInfo<Rail::Rail>)>;

        RenameRailDialog()  = default;
        ~RenameRailDialog() = default;

        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setOriginalName(std::string_view name) {
            size_t end_pos                = std::min(name.size(), m_original_name.size() - 1);
            std::string_view cropped_name = name.substr(0, end_pos);
            std::copy(cropped_name.begin(), cropped_name.end(), m_original_name.begin());
            std::copy(cropped_name.begin(), cropped_name.end(), m_rail_name.begin());
            m_original_name.at(end_pos) = '\0';
            m_rail_name.at(end_pos)     = '\0';
        }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render(SelectionNodeInfo<Rail::Rail> node_info);

    private:
        bool m_open    = false;
        bool m_opening = false;

        std::array<char, 128> m_rail_name     = {};
        std::array<char, 128> m_original_name = {};

        action_t m_on_accept;
        cancel_t m_on_reject;
    };

}  // namespace Toolbox::UI