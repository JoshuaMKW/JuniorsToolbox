#pragma once

#include "core/types.hpp"
#include "smart_resource.hpp"
#include "tristate.hpp"

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
            EVENT_APPLICATION_FONT_CHANGE,
            EVENT_CLIPBOARD,
            EVENT_CLOSE,
            EVENT_CONTEXT_MENU,
            EVENT_CURSOR_CHANGE,
            EVENT_DRAG_ENTER,
            EVENT_DRAG_LEAVE,
            EVENT_DRAG_MOVE,
            EVENT_DROP,
            EVENT_FILE_OPEN,
            EVENT_FOCUS_IN,
            EVENT_FOCUS_OUT,
            EVENT_KEY_PRESS,
            EVENT_KEY_RELEASE,
            EVENT_LANGUAGE_CHANGE,
            EVENT_MOUSE_ENTER,
            EVENT_MOUSE_LEAVE,
            EVENT_MOUSE_PRESS_DBL,
            EVENT_MOUSE_PRESS_DBL_NON_CLIENT,
            EVENT_MOUSE_PRESS,
            EVENT_MOUSE_PRESS_NON_CLIENT,
            EVENT_MOUSE_RELEASE,
            EVENT_MOUSE_RELEASE_NON_CLIENT,
            EVENT_MOUSE_MOVE,
            EVENT_MOUSE_MOVE_NON_CLIENT,
            EVENT_MOUSE_SCROLL,
            EVENT_WINDOW_HIDE,
            EVENT_WINDOW_MOVE,
            EVENT_WINDOW_RESIZE,
            EVENT_WINDOW_SHOW,
            EVENT_SHORTCUT,
            EVENT_TIMER,

            // This is used to mark the end of the default events
            // For the sake of defining the start of custom ones
            EVENT_USER_BEGIN,

            // This is used to mark the maximum event
            EVENT_USER_END = std::numeric_limits<TypeID>::max(),
        };

    protected:
        BaseEvent() = default;

    public:
        BaseEvent(const BaseEvent &)     = default;
        BaseEvent(BaseEvent &&) noexcept = default;

        BaseEvent(TypeID type) : m_event_type(type) {}

        // One may check against SystemEventType, or their own custom values
        [[nodiscard]] virtual TypeID getType() const noexcept { return m_event_type; }

        [[nodiscard]] bool isInputEvent() const noexcept;
        [[nodiscard]] bool isKeyEvent() const noexcept;
        [[nodiscard]] bool isMouseEvent() const noexcept;
        [[nodiscard]] bool isSystemEvent() const noexcept { return m_is_system_event; }

        void accept();
        void ignore();

        ScopePtr<ISmartResource> clone(bool deep) const override;

    protected:
        void setType(TypeID type) { m_event_type = type; }

    private:
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