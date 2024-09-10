#if WIN32
#include <Windows.h>
#elif __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#else
#error "Unsupported OS"
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include "core/input/input.hpp"

// Internals

namespace {

    using namespace Toolbox::Input;

    constexpr size_t c_keys_max    = 512;
    constexpr size_t c_buttons_max = 32;

    using namespace Toolbox::Input;

    static double s_mouse_position_x = 0;
    static double s_mouse_position_y = 0;

    static double s_prev_mouse_position_x = 0;
    static double s_prev_mouse_position_y = 0;

    static double s_mouse_delta_x = 0;
    static double s_mouse_delta_y = 0;

    static double s_mouse_scroll_delta_x = 0;
    static double s_mouse_scroll_delta_y = 0;

    static bool s_mouse_wrapped = false;

    static bool s_keys_down[c_keys_max]      = {false};
    static bool s_prev_keys_down[c_keys_max] = {false};

    static bool s_mouse_buttons_down[c_buttons_max]      = {false};
    static bool s_prev_mouse_buttons_down[c_buttons_max] = {false};

    static bool GetKeyState(Toolbox::Input::KeyCode key) { return s_keys_down[raw_enum(key)]; }
    static void SetKeyState(Toolbox::Input::KeyCode key, bool state) { s_keys_down[raw_enum(key)] = state; }

    static bool GetMouseButtonState(MouseButton button) {
        return s_mouse_buttons_down[raw_enum(button)];
    }
    static void SetMouseButtonState(MouseButton button, bool state) {
        s_mouse_buttons_down[raw_enum(button)] = state;
    }

}  // namespace

// Externals

namespace Toolbox::Input {
    bool IsKeyModifier(KeyCode key) {
        return key == KeyCode::KEY_LEFTSHIFT || key == KeyCode::KEY_RIGHTSHIFT ||
               key == KeyCode::KEY_LEFTCONTROL || key == KeyCode::KEY_RIGHTCONTROL ||
               key == KeyCode::KEY_LEFTALT || key == KeyCode::KEY_RIGHTALT ||
               key == KeyCode::KEY_LEFTSUPER || key == KeyCode::KEY_RIGHTSUPER;
    }

    KeyCodes GetPressedKeys(bool include_mods) {
        KeyCodes keys;
        for (auto key : GetKeyCodes()) {
            if (!include_mods && IsKeyModifier(key)) {
                continue;
            }
            if (GetKey(key)) {
                keys.push_back(key);
            }
        }
        return keys;
    }

    KeyModifiers GetPressedKeyModifiers() {
        KeyModifiers modifiers = KeyModifier::KEY_NONE;
        if (GetKey(KeyCode::KEY_LEFTSHIFT) || GetKey(KeyCode::KEY_RIGHTSHIFT)) {
            modifiers |= KeyModifier::KEY_SHIFT;
        }
        if (GetKey(KeyCode::KEY_LEFTCONTROL) || GetKey(KeyCode::KEY_RIGHTCONTROL)) {
            modifiers |= KeyModifier::KEY_CTRL;
        }
        if (GetKey(KeyCode::KEY_LEFTALT) || GetKey(KeyCode::KEY_RIGHTALT)) {
            modifiers |= KeyModifier::KEY_ALT;
        }
        if (GetKey(KeyCode::KEY_LEFTSUPER) || GetKey(KeyCode::KEY_RIGHTSUPER)) {
            modifiers |= KeyModifier::KEY_SUPER;
        }
        return modifiers;
    }

    bool GetKey(KeyCode key) { return s_keys_down[raw_enum(key)]; }

    bool GetKeyDown(KeyCode key) {
        bool is_down =
            s_keys_down[raw_enum(key)] && !s_prev_keys_down[raw_enum(key)];
        return is_down;
    }

    bool GetKeyUp(KeyCode key) {
        return s_prev_keys_down[raw_enum(key)] && !s_keys_down[raw_enum(key)];
    }

    MouseButtons GetPressedMouseButtons() {
        MouseButtons buttons;
        for (auto button : GetMouseButtons()) {
            if (GetMouseButton(button)) {
                buttons.push_back(button);
            }
        }
        return buttons;
    }

    bool GetMouseButton(MouseButton button) { return s_mouse_buttons_down[raw_enum(button)]; }

    bool GetMouseButtonDown(MouseButton button) {
        return s_mouse_buttons_down[raw_enum(button)] &&
               !s_prev_mouse_buttons_down[raw_enum(button)];
    }

    bool GetMouseButtonUp(MouseButton button) {
        return s_prev_mouse_buttons_down[raw_enum(button)] &&
               !s_mouse_buttons_down[raw_enum(button)];
    }

    void GetMouseViewportPosition(double &x, double &y) {
        x = s_mouse_position_x;
        y = s_mouse_position_y;
    }

    void GetMouseDelta(double &x, double &y) {
        x = s_mouse_delta_x;
        y = s_mouse_delta_y;
    }

    void GetMouseScrollDelta(double &x, double &y) {
        x = s_mouse_scroll_delta_x;
        y = s_mouse_scroll_delta_y;
    }

    bool GetMouseWrapped() { return s_mouse_wrapped; }

    void SetMousePosition(double pos_x, double pos_y, bool overwrite_delta) {
        if (!overwrite_delta) {
            SetMouseWrapped(true);
        }
#if WIN32
        SetCursorPos((int)pos_x, (int)pos_y);
#elif __linux__
        Display *display = XOpenDisplay(0);
        if (display == nullptr)
            return;

        float rel_pos_x = s_mouse_position_x - pos_x;
        float rel_pos_y = s_mouse_position_y - pos_y;
        XWarpPointer(display, 0, 0, 0, 0, 0, 0, rel_pos_x, rel_pos_y);

        XFlush(display);
        XCloseDisplay(display);
#else
#error "Unsupported OS"
#endif
    }

    void SetMouseWrapped(bool wrapped) { s_mouse_wrapped = wrapped; }

    void UpdateInputState() {
        if (!s_mouse_wrapped) {
            s_mouse_delta_x = s_mouse_position_x - s_prev_mouse_position_x;
            s_mouse_delta_y = s_mouse_position_y - s_prev_mouse_position_y;
        } else {
            s_mouse_wrapped = false;
        }
    }

    void PostUpdateInputState() {
        for (int i = 0; i < c_keys_max; i++)
            s_prev_keys_down[i] = s_keys_down[i];
        for (int i = 0; i < c_buttons_max; i++)
            s_prev_mouse_buttons_down[i] = s_mouse_buttons_down[i];

        s_prev_mouse_position_x = s_mouse_position_x;
        s_prev_mouse_position_y = s_mouse_position_y;

        s_mouse_scroll_delta_x = 0;
        s_mouse_scroll_delta_y = 0;
    }

    void GLFWKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (key >= c_keys_max)
            return;

        if (action == GLFW_PRESS)
            s_keys_down[key] = true;
        else if (action == GLFW_RELEASE)
            s_keys_down[key] = false;

        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }

    void GLFWMousePositionCallback(GLFWwindow *window, double xpos, double ypos) {
        int win_x, win_y;
        glfwGetWindowPos(window, &win_x, &win_y);

        s_mouse_position_x = win_x + xpos;
        s_mouse_position_y = win_y + ypos;

        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    }

    void GLFWMouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        if (button >= c_buttons_max)
            return;

        if (action == GLFW_PRESS)
            s_mouse_buttons_down[button] = true;
        else if (action == GLFW_RELEASE)
            s_mouse_buttons_down[button] = false;

        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    }

    void GLFWMouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
        s_mouse_scroll_delta_x = xoffset;
        s_mouse_scroll_delta_y = yoffset;

        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    }

}  // namespace Toolbox::Input
