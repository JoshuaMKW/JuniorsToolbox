#include "gui/layer/imlayer.hpp"

using namespace Toolbox;

namespace Toolbox::UI {

    ImLayer::ImLayer(const std::string &name) : ProcessLayer(name) {}

    void ImLayer::onUpdate(TimeStep delta_time) {
        onImGuiUpdate(delta_time);

        const std::string name = std::format("{}##{}", getName(), getUUID());
        if (ImGui::Begin(name.c_str())) {
            onImGuiRender(delta_time);
        }
        ImGui::End();
    }

    void ImLayer::onEvent(RefPtr<BaseEvent> ev) {
        ProcessLayer::onEvent(ev);
        if (ev->isAccepted() || ev->isIgnored()) {
            return;
        }
        switch (ev->getType()) {
        case BaseEvent::SystemEventType::EVENT_ACTION_ADDED:
        case BaseEvent::SystemEventType::EVENT_ACTION_CHANGED:
        case BaseEvent::SystemEventType::EVENT_ACTION_REMOVED:
        case BaseEvent::SystemEventType::EVENT_ACTIVATION_CHANGE:
        case BaseEvent::SystemEventType::EVENT_APPLICATION_EXIT:
        case BaseEvent::SystemEventType::EVENT_APPLICATION_STATE_CHANGE:
        case BaseEvent::SystemEventType::EVENT_CLIPBOARD:
        case BaseEvent::SystemEventType::EVENT_CLOSE:
        case BaseEvent::SystemEventType::EVENT_FILE_OPEN:
        case BaseEvent::SystemEventType::EVENT_FOCUS_IN:
        case BaseEvent::SystemEventType::EVENT_FOCUS_OUT:
        case BaseEvent::SystemEventType::EVENT_KEY_PRESS:
        case BaseEvent::SystemEventType::EVENT_KEY_RELEASE:
        case BaseEvent::SystemEventType::EVENT_LANGUAGE_CHANGE:
        case BaseEvent::SystemEventType::EVENT_SHORTCUT:
        case BaseEvent::SystemEventType::EVENT_TIMER:
        case EVENT_WINDOW_HIDE:
        case EVENT_WINDOW_MOVE:
        case EVENT_WINDOW_RESIZE:
        case EVENT_WINDOW_SHOW:
            onWindowEvent(ref_cast<WindowEvent>(ev));
            break;
        }
    }

}  // namespace Toolbox::UI