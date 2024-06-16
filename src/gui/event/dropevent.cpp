#include "gui/event/dropevent.hpp"

namespace Toolbox::UI {

    DropEvent::DropEvent(const ImVec2 &pos, DropType drop_type,
                         const DragAction &action)
        : BaseEvent(action.getTargetUUID().value(), EVENT_DROP), m_screen_pos(pos),
          m_drop_type(drop_type), m_source_id(action.getSourceUUID()) {}

    ScopePtr<ISmartResource> DropEvent::clone(bool deep) const {
        return make_scoped<DropEvent>(*this);
    }

}  // namespace Toolbox::UI