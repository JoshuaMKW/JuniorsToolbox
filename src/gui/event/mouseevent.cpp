#include "core/input/input.hpp"
#include "gui/event/mouseevent.hpp"

namespace Toolbox::UI {

    MouseEvent::MouseEvent(const UUID64 &target_id, TypeID type, const ImVec2 &pos,
                           Input::MouseButton button,
                           Input::MouseButtonState press_state, bool is_client)
        : BaseEvent(target_id, type), m_screen_pos(pos), m_mouse_button(button),
          m_mouse_press_state(press_state), m_is_client(is_client) {
        m_mouse_state = Input::GetPressedMouseButtons();
    }

    ScopePtr<ISmartResource> MouseEvent::clone(bool deep) const {
        return make_scoped<MouseEvent>(*this);
    }

}  // namespace Toolbox