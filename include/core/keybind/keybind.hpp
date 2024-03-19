#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Toolbox {

    // Takes in GLFW_KEY_XXX
    std::string KeyNameFromEnum(int key);
    int KeyNameToEnum(const std::string &key);

    class KeyBind {
    public:
        KeyBind()                    = default;
        KeyBind(const KeyBind &)     = default;
        KeyBind(KeyBind &&) noexcept = default;
        ~KeyBind()                   = default;

        explicit KeyBind(std::initializer_list<int> keys);

        [[nodiscard]] static KeyBind FromString(const std::string &);

        [[nodiscard]] bool isInputMatching() const;
        [[nodiscard]] std::string toString() const;

        [[nodiscard]] bool scanNextInputKey();

        KeyBind &operator=(const KeyBind &)     = default;
        KeyBind &operator=(KeyBind &&) noexcept = default;

    private:
        std::vector<int> m_key_combo;
    };

}  // namespace Toolbox