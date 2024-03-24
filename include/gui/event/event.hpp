#pragma once

#include "core/event/event.hpp"

namespace Toolbox {

    enum GUIEventType : BaseEvent::TypeID {
        EVENT_CONTEXT_MENU               = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 0,
        EVENT_CURSOR_CHANGE              = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 1,
        EVENT_DRAG_ENTER                 = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 2,
        EVENT_DRAG_LEAVE                 = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 3,
        EVENT_DRAG_MOVE                  = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 4,
        EVENT_DROP                       = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 5,
        EVENT_FOCUS_IN                   = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 6,
        EVENT_FOCUS_OUT                  = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 7,
        EVENT_FONT_CHANGE                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 8,
        EVENT_MOUSE_ENTER                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 9,
        EVENT_MOUSE_LEAVE                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 10,
        EVENT_MOUSE_PRESS_DBL            = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 11,
        EVENT_MOUSE_PRESS_DBL_NON_CLIENT = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 12,
        EVENT_MOUSE_PRESS                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 13,
        EVENT_MOUSE_PRESS_NON_CLIENT     = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 14,
        EVENT_MOUSE_RELEASE              = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 15,
        EVENT_MOUSE_RELEASE_NON_CLIENT   = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 16,
        EVENT_MOUSE_MOVE                 = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 17,
        EVENT_MOUSE_MOVE_NON_CLIENT      = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 18,
        EVENT_MOUSE_SCROLL               = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 19,
        EVENT_WINDOW_HIDE                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 20,
        EVENT_WINDOW_MOVE                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 21,
        EVENT_WINDOW_RESIZE              = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 22,
        EVENT_WINDOW_SHOW                = BaseEvent::SystemEventType::_EVENT_SYSTEM_END + 23,
    };

}  // namespace Toolbox