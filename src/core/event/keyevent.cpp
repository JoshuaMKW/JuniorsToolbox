#pragma once

#include <algorithm>
#include <string>

#include "core/event/keyevent.hpp"
#include <magic_enum.hpp>

namespace Toolbox {

    KeyEvent::KeyEvent(TypeID type, Input::KeyCode key, Input::KeyModifiers modifiers,
                       const std::string &text, int repeat_count)
        : BaseEvent(type), m_target_key(key), m_modifiers(modifiers), m_resultant_text(text),
          m_repeat_count(repeat_count) {
        TOOLBOX_ASSERT_V(
            type == EVENT_KEY_PRESS || type == EVENT_KEY_RELEASE,
            "KeyEvent must be constructed with type EVENT_KEY_PRESS or EVENT_KEY_RELEASE (Got {})",
            type < SystemEventType::EVENT_USER_BEGIN ? magic_enum::enum_name((SystemEventType)type)
                                                     : "UserEnum");
        m_held_keys = Input::GetPressedKeys();
    }

    [[nodiscard]] size_t KeyEvent::getKeyCount() const noexcept {
        return std::max(m_held_keys.size(), (size_t)1);
    }

    ScopePtr<ISmartResource> KeyEvent::clone(bool deep) const {
        return make_scoped<KeyEvent>(*this);
    }

}  // namespace Toolbox