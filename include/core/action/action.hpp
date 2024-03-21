#pragma once

#include <functional>
#include <string>

#include "core/keybind/keybind.hpp"

namespace Toolbox {

    class Action {
    public:
        Action()                   = delete;
        Action(const Action &)     = default;
        Action(Action &&) noexcept = default;

        Action(const KeyBind &key_bind, const std::string &name,
               const std::string &description, std::function<void()> action);

        [[nodiscard]] const KeyBind &getKeyBind() const { return m_key_bind; }
        [[nodiscard]] const std::string &getName() const { return m_name; }
        [[nodiscard]] const std::string &getDescription() const { return m_description; }

        void execute() const { m_action(); }

        Action &operator=(const Action &)     = default;
        Action &operator=(Action &&) noexcept = default;

    protected:
        static void NullAction() {}

    private:
        KeyBind m_key_bind;
        std::string m_name;
        std::string m_description;
        std::function<void()> m_action;
    };

}  // namespace Toolbox