#include "core/application/layer.hpp"

namespace Toolbox {

    bool ProcessLayer::isTargetOfEvent(RefPtr<BaseEvent> ev) const noexcept {
        bool is_same_id = ev->getTargetId() == getUUID();
        return is_same_id;
    }

    void ProcessLayer::onEvent(RefPtr<BaseEvent> ev) {
        if (!isTargetOfEvent(ev)) {
            propogateEvent(ev);
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
        case BaseEvent::SystemEventType::EVENT_FILE_OPEN:
        case BaseEvent::SystemEventType::EVENT_LANGUAGE_CHANGE:
            break;
        case BaseEvent::SystemEventType::EVENT_KEY_PRESS:
        case BaseEvent::SystemEventType::EVENT_KEY_RELEASE:
            onKeyEvent(ref_cast<KeyEvent>(ev));
            break;
        case BaseEvent::SystemEventType::EVENT_SHORTCUT:
            onShortcutEvent(ref_cast<ShortcutEvent>(ev));
            break;
        case BaseEvent::SystemEventType::EVENT_TIMER:
            onTimerEvent(ref_cast<TimerEvent>(ev));
            break;
        default:
            break;
        }
    }

    void ProcessLayer::propogateEvent(RefPtr<BaseEvent> ev) {
        for (auto &layer : m_sublayers) {
            layer->onEvent(ev);
            if (ev->isHandled()) {
                break;
            }
        }
        return;
    }

}  // namespace Toolbox