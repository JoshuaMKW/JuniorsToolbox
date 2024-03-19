#pragma once

#include "core/event/event.hpp"
#include <string>

namespace Toolbox {

    class KeyEvent final : public BaseEvent {
    private:
        KeyEvent() = default;

    public:
        KeyEvent(const KeyEvent &)     = default;
        KeyEvent(KeyEvent &&) noexcept = default;

        KeyEvent(TypeID type, int key, int modifiers, const std::string &text = "", int repeat_count = 1);

        [[nodiscard]] size_t getKeyCount() const noexcept;
        [[nodiscard]] size_t getRepeatCount() const noexcept { return m_repeat_count; }

        [[nodiscard]] int getKey() const noexcept { return m_target_key; }
        [[nodiscard]] int getModifiers() const noexcept { return m_target_modifiers; }
        [[nodiscard]] const std::string &getText() const noexcept { return m_resultant_text; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

    private:
        int m_target_key             = -1;
        int m_target_modifiers       = -1;
        int m_repeat_count           = 1;
        std::string m_resultant_text = "";
    };

}  // namespace Toolbox