#include "gui/event/contextmenuevent.hpp"

namespace Toolbox::UI {

    ContextMenuEvent::ContextMenuEvent(const UUID64 &target_id, float pos_x, float pos_y)
        : BaseEvent(target_id, EVENT_CONTEXT_MENU), m_screen_pos_x(pos_x),
          m_screen_pos_y(pos_y) {}

    ScopePtr<ISmartResource> ContextMenuEvent::clone(bool deep) const {
        return make_scoped<ContextMenuEvent>(*this);
    }

}  // namespace Toolbox::UI