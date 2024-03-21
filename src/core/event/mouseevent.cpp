#include "core/event/mouseevent.hpp"
#include "core/input/input.hpp"

namespace Toolbox {

    MouseEvent::MouseEvent(TypeID type, float pos_x, float pos_y, Input::MouseButton button,
                           Input::MouseButtonState press_state)
        : BaseEvent(type), m_screen_pos_x(pos_x), m_screen_pos_y(pos_y), m_mouse_button(button),
          m_mouse_press_state(press_state) {
        m_mouse_state = Input::GetPressedMouseButtons();
    }

    ScopePtr<ISmartResource> MouseEvent::clone(bool deep) const {
        return make_scoped<MouseEvent>(*this);
    }

}  // namespace Toolbox