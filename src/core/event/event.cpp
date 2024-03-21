#pragma once

#include "core/types.hpp"
#include "smart_resource.hpp"
#include "tristate.hpp"

#include "core/event/event.hpp"

namespace Toolbox {

    bool BaseEvent::isInputEvent() const noexcept {
        switch (getType()) {
        case EVENT_CONTEXT_MENU:
        case EVENT_KEY_PRESS:
        case EVENT_KEY_RELEASE:
        case EVENT_MOUSE_ENTER:
        case EVENT_MOUSE_LEAVE:
        case EVENT_MOUSE_PRESS_DBL:
        case EVENT_MOUSE_PRESS_DBL_NON_CLIENT:
        case EVENT_MOUSE_PRESS:
        case EVENT_MOUSE_PRESS_NON_CLIENT:
        case EVENT_MOUSE_RELEASE:
        case EVENT_MOUSE_RELEASE_NON_CLIENT:
        case EVENT_MOUSE_MOVE:
        case EVENT_MOUSE_MOVE_NON_CLIENT:
            return true;
        default:
            return false;
        }
    }

    bool BaseEvent::isKeyEvent() const noexcept {
        switch (getType()) {
        case EVENT_KEY_PRESS:
        case EVENT_KEY_RELEASE:
            return true;
        default:
            return false;
        }
    }

    bool BaseEvent::isMouseEvent() const noexcept {
        switch (getType()) {
        case EVENT_MOUSE_ENTER:
        case EVENT_MOUSE_LEAVE:
        case EVENT_MOUSE_PRESS_DBL:
        case EVENT_MOUSE_PRESS_DBL_NON_CLIENT:
        case EVENT_MOUSE_PRESS:
        case EVENT_MOUSE_PRESS_NON_CLIENT:
        case EVENT_MOUSE_RELEASE:
        case EVENT_MOUSE_RELEASE_NON_CLIENT:
        case EVENT_MOUSE_MOVE:
        case EVENT_MOUSE_MOVE_NON_CLIENT:
        case EVENT_MOUSE_SCROLL:
            return true;
        default:
            return false;
        }
    }

    void BaseEvent::accept() { m_accepted_state = TriState::TS_TRUE; }
    void BaseEvent::ignore() { m_accepted_state = TriState::TS_FALSE; }

    ScopePtr<ISmartResource> BaseEvent::clone(bool deep) const {
        return make_scoped<BaseEvent>(*this);
    }

}  // namespace Toolbox