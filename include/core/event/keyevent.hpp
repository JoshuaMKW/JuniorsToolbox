#pragma once

#include <string>

#include "core/event/event.hpp"
#include "core/input/input.hpp"

namespace Toolbox {

    class KeyEvent final : public BaseEvent {
    private:
        KeyEvent() = default;

    public:
        KeyEvent(const KeyEvent &)     = default;
        KeyEvent(KeyEvent &&) noexcept = default;

        KeyEvent(const UUID64 &target_id, TypeID type, Input::KeyCode key,
                 Input::KeyModifiers modifiers,
                 const std::string &text = "", int repeat_count = 1);

        [[nodiscard]] size_t getKeyCount() const noexcept;
        [[nodiscard]] size_t getRepeatCount() const noexcept { return m_repeat_count; }

        [[nodiscard]] Input::KeyCode getKey() const noexcept { return m_target_key; }
        [[nodiscard]] const Input::KeyCodes &getHeldKeys() const noexcept { return m_held_keys; }
        [[nodiscard]] Input::KeyModifiers getModifiers() const noexcept { return m_modifiers; }
        [[nodiscard]] const std::string &getText() const noexcept { return m_resultant_text; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        KeyEvent &operator=(const KeyEvent &)     = default;
        KeyEvent &operator=(KeyEvent &&) noexcept = default;

    private:
        Input::KeyCode m_target_key     = Input::KeyCode::KEY_NONE;
        Input::KeyCodes m_held_keys     = {};
        Input::KeyModifiers m_modifiers = Input::KeyModifier::KEY_NONE;
        int m_repeat_count              = 1;
        std::string m_resultant_text    = "";
    };

}  // namespace Toolbox