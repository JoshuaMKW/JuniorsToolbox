#pragma once

#include "core/action/action.hpp"
#include "core/event/event.hpp"
#include "core/keybind/keybind.hpp"

namespace Toolbox {

    class ShortcutEvent final : public BaseEvent {
    private:
        ShortcutEvent() = delete;

    public:
        ShortcutEvent(const ShortcutEvent &)     = default;
        ShortcutEvent(ShortcutEvent &&) noexcept = default;

        ShortcutEvent(const UUID64 &target_id, const Action &action, bool is_system_bind = false);

        [[nodiscard]] bool isSystemBind() const { return m_is_system_bind; }

        [[nodiscard]] const KeyBind &getKeyBind() const { return m_action.getKeyBind(); }
        [[nodiscard]] const Action &getAction() const { return m_action; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        ShortcutEvent &operator=(const ShortcutEvent &)     = default;
        ShortcutEvent &operator=(ShortcutEvent &&) noexcept = default;

    private:
        Action m_action;
        bool m_is_system_bind;
    };

}  // namespace Toolbox