#pragma once

#include "core/types.hpp"
#include "smart_resource.hpp"
#include "tristate.hpp"
#include "unique.hpp"

namespace Toolbox {

    class BaseEvent : public ISmartResource {
    public:
        using TypeID = u64;

        enum SystemEventType : TypeID {
            // Nothing event, the start of events
            EVENT_NONE,

            EVENT_ACTION_ADDED,
            EVENT_ACTION_CHANGED,
            EVENT_ACTION_REMOVED,
            EVENT_ACTIVATION_CHANGE,
            EVENT_APPLICATION_EXIT,
            EVENT_APPLICATION_STATE_CHANGE,
            EVENT_CLIPBOARD,
            EVENT_CLOSE,
            EVENT_FILE_OPEN,
            EVENT_KEY_PRESS,
            EVENT_KEY_RELEASE,
            EVENT_LANGUAGE_CHANGE,
            EVENT_SHORTCUT,
            EVENT_TIMER,

            // Internal use only
            _EVENT_SYSTEM_END,

            // This is used to mark the end of the default events
            // For the sake of defining the start of custom ones
            EVENT_USER_BEGIN = 0x1000,

            // This is used to mark the maximum event
            EVENT_USER_END = std::numeric_limits<TypeID>::max(),
        };

    protected:
        BaseEvent() = default;

    public:
        BaseEvent(const BaseEvent &)     = default;
        BaseEvent(BaseEvent &&) noexcept = default;

        BaseEvent(const UUID64 &target_id, TypeID type) : m_target_id(target_id), m_event_type(type) {}

        // One may check against SystemEventType, or their own custom values
        [[nodiscard]] virtual TypeID getType() const noexcept { return m_event_type; }
        [[nodiscard]] UUID64 getTargetId() const noexcept { return m_target_id; }

        bool isAccepted() const noexcept { return m_accepted_state == TriState::TS_TRUE; }
        bool isIgnored() const noexcept { return m_accepted_state == TriState::TS_FALSE; }

        [[nodiscard]] bool isSystemEvent() const noexcept { return m_is_system_event; }

        void accept();
        void ignore();

        ScopePtr<ISmartResource> clone(bool deep) const override;

    protected:
        void setType(TypeID type) { m_event_type = type; }

    private:
        UUID64 m_target_id;
        TriState m_accepted_state = TriState::TS_INDETERMINATE;
        TypeID m_event_type       = SystemEventType::EVENT_NONE;
        bool m_is_system_event    = false;
    };

    class IEventProcessor {
    public:
        virtual ~IEventProcessor() = default;

        virtual void onEvent(const BaseEvent &event) = 0;
    };

}  // namespace Toolbox