#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Toolbox::UI {

    bool KeyBindHeld(const std::vector<int> &keybind);
    
    // Takes in GLFW_KEY_XXX
    std::string KeyNameFromEnum(int key);
    int KeyNameToEnum(const std::string &key);

    inline std::string KeyBindToString(const std::vector<int> &keybind) {
        std::string keybind_name = "";
        for (size_t j = 0; j < keybind.size(); ++j) {
            int key = keybind.at(j);
            keybind_name += KeyNameFromEnum(key);
            if (j < keybind.size() - 1) {
                keybind_name += "+";
            }
        }
        return keybind_name;
    }

    inline std::vector<int> KeyBindFromString(std::string_view keybind_str) {
        std::vector<int> keybind;

        size_t key_begin_index = 0;
        while (true) {
            size_t next_delimiter_index = keybind_str.find('+', key_begin_index);
            std::string key_name        = std::string(keybind_str.substr(key_begin_index, next_delimiter_index));
            keybind.push_back(KeyNameToEnum(key_name));

            // Last character is + or we reached end of string
            if (next_delimiter_index >= std::string::npos - 1) {
                break;
            }

            key_begin_index = next_delimiter_index + 1;
        }

        return keybind;
    }

}  // namespace Toolbox::UI