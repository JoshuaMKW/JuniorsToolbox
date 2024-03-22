#pragma once

#include "core/action/action.hpp"
#include "core/event/event.hpp"
#include "core/keybind/keybind.hpp"
#include "unique.hpp"

namespace Toolbox {

    class TimerEvent final : public BaseEvent {
    private:
        TimerEvent() = delete;

    public:
        TimerEvent(const TimerEvent &)     = default;
        TimerEvent(TimerEvent &&) noexcept = default;

        TimerEvent(const UUID64 &timer_uuid);

        [[nodiscard]] const UUID64 &getTimerUUID() const { return m_timer_uuid; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        TimerEvent &operator=(const TimerEvent &)     = default;
        TimerEvent &operator=(TimerEvent &&) noexcept = default;

    private:
        UUID64 m_timer_uuid;
    };

}  // namespace Toolbox