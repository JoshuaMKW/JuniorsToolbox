#include "core/event/shortcutevent.hpp"

namespace Toolbox {
    ShortcutEvent::ShortcutEvent(const Action &action, bool is_system_bind) : BaseEvent(EVENT_SHORTCUT), m_action(action), m_is_system_bind(is_system_bind) {}

    ScopePtr<ISmartResource> ShortcutEvent::clone(bool deep) const {
        return std::make_unique<ShortcutEvent>(*this);
    }

}  // namespace Toolbox