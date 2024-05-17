#pragma once

#include <string>
#include <vector>

#include "core/input/keycode.hpp"

namespace Toolbox {

    // Takes in GLFW_KEY_XXX
    std::string KeyNameFromEnum(Input::KeyCode key);
    Input::KeyCode KeyNameToEnum(const std::string &key);

    class KeyBind {
    public:
        KeyBind()                    = default;
        KeyBind(const KeyBind &)     = default;
        KeyBind(KeyBind &&) noexcept = default;
        ~KeyBind()                   = default;

        KeyBind(std::initializer_list<Input::KeyCode> keys);
        explicit KeyBind(const Input::KeyCodes &keys);

        [[nodiscard]] static KeyBind FromString(const std::string &);

        [[nodiscard]] bool empty() const { return m_key_combo.empty(); }
        [[nodiscard]] size_t size() const { return m_key_combo.size(); }

        [[nodiscard]] Input::KeyCode getKey(size_t index) const { return m_key_combo.at(index); }
        [[nodiscard]] const Input::KeyCodes &getKeys() const { return m_key_combo; }

        [[nodiscard]] std::string toString() const;

        // Returns true if the input matches the keybind
        [[nodiscard]] bool isInputMatching() const;

        // Returns true if the keybind is post-complete and consumes the input
        [[nodiscard]] bool scanNextInputKey();

        KeyBind &operator=(const KeyBind &)     = default;
        KeyBind &operator=(KeyBind &&) noexcept = default;

        [[nodiscard]] bool operator==(const KeyBind &other) const;
        [[nodiscard]] bool operator!=(const KeyBind &other) const;

    private:
        Input::KeyCodes m_key_combo;
    };

}  // namespace Toolbox