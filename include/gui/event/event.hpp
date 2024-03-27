#pragma once

#include "core/event/event.hpp"

namespace Toolbox {

    enum GUIEventType : BaseEvent::TypeID {
        EVENT_CONTEXT_MENU = BaseEvent::SystemEventType::_EVENT_SYSTEM_END,
        EVENT_CURSOR_CHANGE,
        EVENT_DRAG_ENTER,
        EVENT_DRAG_LEAVE,
        EVENT_DRAG_MOVE,
        EVENT_DROP,
        EVENT_FOCUS_IN,
        EVENT_FOCUS_OUT,
        EVENT_FONT_CHANGE,
        EVENT_MOUSE_ENTER,
        EVENT_MOUSE_LEAVE,
        EVENT_MOUSE_PRESS_DBL,
        EVENT_MOUSE_PRESS_DBL_NON_CLIENT,
        EVENT_MOUSE_PRESS,
        EVENT_MOUSE_PRESS_NON_CLIENT,
        EVENT_MOUSE_RELEASE,
        EVENT_MOUSE_RELEASE_NON_CLIENT,
        EVENT_MOUSE_MOVE,
        EVENT_MOUSE_MOVE_NON_CLIENT,
        EVENT_MOUSE_SCROLL,
        EVENT_WINDOW_CLOSE,
        EVENT_WINDOW_HIDE,
        EVENT_WINDOW_MOVE,
        EVENT_WINDOW_RESIZE,
        EVENT_WINDOW_SHOW,
    };

}  // namespace Toolbox