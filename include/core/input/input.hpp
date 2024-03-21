#pragma once

#include <cstdint>
#include <imgui.h>
#include <map>

#include "core/input/mousebutton.hpp"
#include "core/input/keycode.hpp"

struct GLFWwindow;

namespace Toolbox::Input {

    bool IsKeyModifier(KeyCode key);
    KeyCodes GetPressedKeys(bool include_mods = true);
    KeyModifiers GetPressedKeyModifiers();
    bool GetKey(KeyCode key);
    bool GetKeyDown(KeyCode key);
    bool GetKeyUp(KeyCode key);

    MouseButtons GetPressedMouseButtons();
    bool GetMouseButton(MouseButton button);
    bool GetMouseButtonDown(MouseButton button);
    bool GetMouseButtonUp(MouseButton button);

    void GetMouseViewportPosition(float &x, float &y);
    void SetMousePosition(float x, float y, bool overwrite_delta = true);

    void GetMouseDelta(float &x, float &y);
    void GetMouseScrollDelta(float &x, float &y);

    bool GetMouseWrapped();
    void SetMouseWrapped(bool wrapped);

    void UpdateInputState();
    void PostUpdateInputState();

    void GLFWKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    void GLFWMousePositionCallback(GLFWwindow *window, double xpos, double ypos);
    void GLFWMouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    void GLFWMouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

}