#pragma once

#include <algorithm>
#include <functional>

#include "gui/appmain/pad/recorder.hpp"

namespace Toolbox::UI {

    class CreateLinkDialog {
    public:
        using action_t = std::function<void(char from_link, char to_link)>;
        using cancel_t = std::function<void(char from_link, char to_link)>;

        CreateLinkDialog()  = default;
        ~CreateLinkDialog() = default;

        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setLinkData(RefPtr<ReplayLinkData> link_data) { m_link_data = link_data; }

        void setup();

        void open() {
            m_open    = true;
            m_opening = true;
        }
        void render();

    protected:
        bool isValidForCreate(char from_link, char to_link) const;

    private:
        bool m_open    = false;
        bool m_opening = false;

        RefPtr<ReplayLinkData> m_link_data;
        char m_from_link = '*';
        char m_to_link   = '*';

        action_t m_on_accept;
        cancel_t m_on_reject;
    };

}  // namespace Toolbox::UI