#pragma once

#include <algorithm>
#include <string>

#include "core/event/keyevent.hpp"
#include <magic_enum.hpp>

namespace Toolbox {

    KeyEvent::KeyEvent(TypeID type, int key, int modifiers, const std::string &text)
        : BaseEvent(type), m_target_key(key), m_target_modifiers(modifiers),
          m_resultant_text(text) {
        TOOLBOX_ASSERT_V(
            type == EVENT_KEY_PRESS || type == EVENT_KEY_RELEASE,
            "KeyEvent must be constructed with type EVENT_KEY_PRESS or EVENT_KEY_RELEASE (Got {})",
            magic_enum::enum_name(type));
    }

    [[nodiscard]] size_t KeyEvent::getKeyCount() const noexcept {
        return std::max(m_resultant_text.length(), (size_t)1);
    }

    ScopePtr<ISmartResource> KeyEvent::clone(bool deep) const {
        KeyEvent result;
        result.m_resultant_text   = m_resultant_text;
        result.m_target_key       = m_target_key;
        result.m_target_modifiers = m_target_modifiers;
        return make_scoped<KeyEvent>(std::move(result));
    }

}  // namespace Toolbox