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
        if (ev->isHandled()) {
            return;
        }

        switch (ev->getType()) {
        case EVENT_FOCUS_IN:
        case EVENT_FOCUS_OUT:
            onFocusEvent(ev);
            break;
        case EVENT_WINDOW_CLOSE:
        case EVENT_WINDOW_HIDE:
        case EVENT_WINDOW_MOVE:
        case EVENT_WINDOW_RESIZE:
        case EVENT_WINDOW_SHOW:
            onWindowEvent(ref_cast<WindowEvent>(ev));
            break;
        }
    }

    void ImProcessLayer::onFocusEvent(RefPtr<BaseEvent> ev) {
        ev->accept();
        switch (ev->getType()) {
        case EVENT_FOCUS_IN:
            m_is_focused = true;
            break;
        case EVENT_FOCUS_OUT:
            m_is_focused = false;
            break;
        default:
            break;
        }
    }

    void ImProcessLayer::onWindowEvent(RefPtr<WindowEvent> ev) {
        ev->accept();
        switch (ev->getType()) {
        case EVENT_WINDOW_MOVE:
            m_position = ev->getGlobalPoint();
            break;
        case EVENT_WINDOW_RESIZE:
            m_size = ev->getSize();
            break;
        case EVENT_WINDOW_CLOSE:
            m_is_open   = false;
            m_is_hidden = false;
            break;
        case EVENT_WINDOW_HIDE:
            m_is_hidden = true;
            break;
        case EVENT_WINDOW_SHOW:
            m_is_open   = true;
            m_is_hidden = false;
            break;
        default:
            break;
        }
    }

}  // namespace Toolbox::UI