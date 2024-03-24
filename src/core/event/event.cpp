#pragma once

#include "core/types.hpp"
#include "smart_resource.hpp"
#include "tristate.hpp"

#include "core/event/event.hpp"

namespace Toolbox {

    void BaseEvent::accept() { m_accepted_state = TriState::TS_TRUE; }
    void BaseEvent::ignore() { m_accepted_state = TriState::TS_FALSE; }

    ScopePtr<ISmartResource> BaseEvent::clone(bool deep) const {
        return make_scoped<BaseEvent>(*this);
    }

}  // namespace Toolbox