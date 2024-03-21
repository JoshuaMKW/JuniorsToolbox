#pragma once

#include <functional>
#include <string>

#include "core/action/action.hpp"

namespace Toolbox {

    Action::Action(const KeyBind &key_bind, const std::string &name, const std::string &description,
                   std::function<void()> action)
        : m_key_bind(key_bind), m_name(name), m_description(description) {
        m_action = action ? action : NullAction;
    }

}  // namespace Toolbox