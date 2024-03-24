#include "gui/event/dropevent.hpp"

namespace Toolbox::UI {

    DropEvent::DropEvent(float pos_x, float pos_y, DropType drop_type,
                         const DragAction &action)
        : BaseEvent(action.getTargetUUID().value(), EVENT_DROP), m_screen_pos_x(pos_x),
          m_screen_pos_y(pos_y),
          m_drop_type(drop_type), m_source_id(action.getSourceUUID()) {}

    ScopePtr<ISmartResource> DropEvent::clone(bool deep) const {
        return ScopePtr<ISmartResource>();
    }

}  // namespace Toolbox::UI