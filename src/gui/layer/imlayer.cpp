#include "gui/layer/imlayer.hpp"

using namespace Toolbox;

namespace Toolbox::UI {

    ImProcessLayer::ImProcessLayer(const std::string &name) : ProcessLayer(name) {}

    void ImProcessLayer::onUpdate(TimeStep delta_time) {
        if (isOpen()) {
            onImGuiUpdate(delta_time);
        }
        onImGuiRender(delta_time);
        if (isOpen()) {
            onImGuiPostUpdate(delta_time);
        }
    }

    void ImProcessLayer::onEvent(RefPtr<BaseEvent> ev) {
        ProcessLayer::onEvent(ev);
        if (ev->isAccepted() || ev->isIgnored()) {
            return;
        }
        switch (ev->getType()) {
        case EVENT_FOCUS_IN:
        case EVENT_FOCUS_OUT:
            // TODO: Call focus event
            break;
        case EVENT_WINDOW_HIDE:
        case EVENT_WINDOW_MOVE:
        case EVENT_WINDOW_RESIZE:
        case EVENT_WINDOW_SHOW:
            onWindowEvent(ref_cast<WindowEvent>(ev));
            break;
        }
    }

}  // namespace Toolbox::UI