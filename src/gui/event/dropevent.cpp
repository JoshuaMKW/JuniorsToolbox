#include "gui/event/dropevent.hpp"

namespace Toolbox::UI {

    DropEvent::DropEvent(const ImVec2 &pos, RefPtr<DragAction> action)
        : BaseEvent(action->getTargetUUID().value(), EVENT_DROP), m_screen_pos(pos),
          m_drag_action(action) {}

    ScopePtr<ISmartResource> DropEvent::clone(bool deep) const {
        return make_scoped<DropEvent>(*this);
    }

}  // namespace Toolbox::UI