#include "core/event/timerevent.hpp"

namespace Toolbox {

    TimerEvent::TimerEvent(const UUID64 &timer_uuid) : BaseEvent(EVENT_TIMER), m_timer_uuid(timer_uuid) {}

    ScopePtr<ISmartResource> TimerEvent::clone(bool deep) const {
        return std::make_unique<TimerEvent>(*this);
    }

}  // namespace Toolbox