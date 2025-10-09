#pragma once

#include <cstdint>
#include <imgui.h>
#include <map>

#include "core/input/mousebutton.hpp"
#include "core/input/keycode.hpp"

struct GLFWwindow;

namespace Toolbox::Input {

    bool IsCapsLockOn();
    bool IsKeyModifier(KeyCode key);
    KeyCodes GetPressedKeys(bool include_mods = true);
    KeyModifiers GetPressedKeyModifiers();
    bool GetKey(KeyCode key);
    bool GetKeyDown(KeyCode key);
    bool GetKeyUp(KeyCode key);

    MouseButtons GetPressedMouseButtons();
    bool GetMouseButton(MouseButton button, bool include_external = false);
    bool GetMouseButtonDown(MouseButton button, bool include_external = false);
    bool GetMouseButtonUp(MouseButton button, bool include_external = false);

    // This gets the mouse position only when within a GLFW window
    void GetMouseViewportPosition(double &x, double &y);
    // This gets the mouse position even when outside the window
    void GetMousePosition(double &x, double &y);
    void SetMousePosition(double x, double y, bool overwrite_delta = true);

    void GetMouseDelta(double &x, double &y);
    void GetMouseScrollDelta(double &x, double &y);

    bool GetMouseWrapped();
    void SetMouseWrapped(bool wrapped);

    void UpdateInputState();
    void PostUpdateInputState();

    void GLFWKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    void GLFWMousePositionCallback(GLFWwindow *window, double xpos, double ypos);
    void GLFWMouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    void GLFWMouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

}