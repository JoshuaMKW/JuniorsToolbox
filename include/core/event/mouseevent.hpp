#pragma once

#include "core/event/event.hpp"
#include <string>

namespace Toolbox {

    class MouseEvent final : public BaseEvent {
    private:
        MouseEvent() = default;

    public:
        MouseEvent(const MouseEvent &)     = default;
        MouseEvent(MouseEvent &&) noexcept = default;

        ScopePtr<ISmartResource> clone(bool deep) const override;

    private:
        
    };

}  // namespace Toolbox