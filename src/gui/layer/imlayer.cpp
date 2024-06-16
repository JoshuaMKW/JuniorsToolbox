#include "gui/layer/imlayer.hpp"

using namespace Toolbox;

namespace Toolbox::UI {

    ImProcessLayer::ImProcessLayer(const std::string &name) : ProcessLayer(name) {}

    static bool isMouseOver(ImVec2 pos, ImVec2 size, ImVec2 mouse_pos) {
        return mouse_pos.x >= pos.x && mouse_pos.x <= pos.x + size.x && mouse_pos.y >= pos.y &&
               mouse_pos.y <= pos.y + size.y;
    }

    static bool isMouseEvent(RefPtr<BaseEvent> ev) {
        BaseEvent::TypeID event_id = ev->getType();
        return event_id >= EVENT_MOUSE_ENTER && event_id <= EVENT_MOUSE_SCROLL;
    }

    bool ImProcessLayer::isTargetOfEvent(RefPtr<BaseEvent> ev) const noexcept {
        /*if (isMouseEvent(ev)) {
            return isMouseOver(m_position, m_size, ref_cast<MouseEvent>(ev)->getGlobalPoint());
        }*/
        return ProcessLayer::isTargetOfEvent(ev);
    }

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
        // In this case, the event is a mouse event and the mouse is not over the layer
        if (isMouseEvent(ev) &&
            !isMouseOver(m_position, m_size, ref_cast<MouseEvent>(ev)->getGlobalPoint())) {
            return;
        }

        ProcessLayer::onEvent(ev);
        if (ev->isAccepted()) {
            return;
        }

        bool is_target = isTargetOfEvent(ev);

        switch (ev->getType()) {
        case EVENT_CONTEXT_MENU:
            if (is_target) {
                onContextMenuEvent(ref_cast<ContextMenuEvent>(ev));
            }
            break;
        case EVENT_CURSOR_CHANGE:
            break;
        case EVENT_DRAG_ENTER:
        case EVENT_DRAG_LEAVE:
        case EVENT_DRAG_MOVE:
            if (is_target) {
                onDragEvent(ref_cast<DragEvent>(ev));
            }
            break;
        case EVENT_DROP:
            if (is_target) {
                onDropEvent(ref_cast<DropEvent>(ev));
            }
            break;
        case EVENT_FONT_CHANGE:
            break;
        case EVENT_FOCUS_IN:
        case EVENT_FOCUS_OUT:
            if (is_target) {
                onFocusEvent(ev);
            }
            break;
        case EVENT_MOUSE_ENTER:
        case EVENT_MOUSE_LEAVE:
        case EVENT_MOUSE_PRESS_DBL:
        case EVENT_MOUSE_PRESS:
        case EVENT_MOUSE_RELEASE:
        case EVENT_MOUSE_MOVE:
        case EVENT_MOUSE_SCROLL:
            // We allow the event to propogate to sublayers
            // first, as the mouse event may be handled by
            // a sublayer whose rect is within the parent
            propogateEvent(ev);
            if (is_target && !ev->isHandled()) {
                onMouseEvent(ref_cast<MouseEvent>(ev));
            }
            break;
        case EVENT_WINDOW_CLOSE:
        case EVENT_WINDOW_HIDE:
        case EVENT_WINDOW_MOVE:
        case EVENT_WINDOW_RESIZE:
        case EVENT_WINDOW_SHOW:
            if (is_target) {
                onWindowEvent(ref_cast<WindowEvent>(ev));
            }
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