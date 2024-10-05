#include "gui/event/dragevent.hpp"

namespace Toolbox::UI {

    DragEvent::DragEvent(TypeID type, float pos_x, float pos_y, RefPtr<DragAction> action)
        : BaseEvent(action->getTargetUUID().value(), type), m_screen_pos_x(pos_x),
          m_screen_pos_y(pos_y), m_drag_action(action) {}

    ScopePtr<ISmartResource> DragEvent::clone(bool deep) const {
        return make_scoped<DragEvent>(*this);
    }

}  // namespace Toolbox::UI