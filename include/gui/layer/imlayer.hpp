#pragma once

#include "core/application/layer.hpp"
#include "core/event/event.hpp"
#include "core/event/keyevent.hpp"
#include "core/event/shortcutevent.hpp"
#include "core/event/timerevent.hpp"
#include "gui/event/dropevent.hpp"
#include "gui/event/mouseevent.hpp"
#include "gui/event/windowevent.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class ImLayer : public ProcessLayer {
    public:
        ImLayer()  = delete;
        ~ImLayer() = default;

        explicit ImLayer(const std::string &name);

        void onUpdate(TimeStep delta_time) override final;
        void onEvent(RefPtr<BaseEvent> ev) override final;

    protected:
        virtual void onImGuiUpdate(TimeStep delta_time) {}
        virtual void onImGuiRender(TimeStep delta_time) {}

        // Event callbacks
        virtual void onDropEvent(RefPtr<DropEvent> ev) {}
        virtual void onFocusEvent(RefPtr<BaseEvent> ev) {}
        virtual void onMouseEvent(RefPtr<MouseEvent> ev) {}
        virtual void onWindowEvent(RefPtr<WindowEvent> ev) {}

    private:
        bool m_is_open    = true;
        ImVec2 m_size     = {0, 0};
        ImVec2 m_position = {0, 0};
    };

}  // namespace Toolbox::UI