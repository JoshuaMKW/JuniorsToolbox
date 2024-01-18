#include "gui/input.hpp"
#include <imgui.h>

#if WIN32
#include <Windows.h>
#elif __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#else
#error "Unsupported OS"
#endif
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

namespace Toolbox::UI::Input {
    // Anonymous namespace within Input allows us to have private-scope variables that are only
    // accessible from this TU. This allows us to hide direct access to the input state variables
    // and only allow access via the Get functions.
    namespace {
        constexpr uint32_t KEY_MAX          = 512;
        constexpr uint32_t MOUSE_BUTTON_MAX = 3;

        ImVec2 mMousePosition = {0, 0};
        ImVec2 mMouseDelta = {0, 0};
        int32_t mMouseScrollDelta = 0;

        bool mMouseWrapped = false;

        bool mKeysDown[KEY_MAX]{};
        bool mPrevKeysDown[KEY_MAX]{};

        bool mMouseButtonsDown[MOUSE_BUTTON_MAX]{};
        bool mPrevMouseButtonsDown[MOUSE_BUTTON_MAX]{};
        ImVec2 mPrevMousePosition = {0, 0};

        void SetKeyboardState(uint32_t key, bool pressed) { mKeysDown[key] = pressed; }

        void SetMouseState(uint32_t button, bool pressed) { mMouseButtonsDown[button] = pressed; }

        void SetMousePosition(uint32_t x, uint32_t y) { mMousePosition = ImVec2((float)x, (float)y); }

        void SetMouseScrollDelta(uint32_t delta) { mMouseScrollDelta = delta; }
    }  // namespace
}  // namespace Toolbox::UI::Input

bool Toolbox::UI::Input::GetKey(uint32_t key) { return mKeysDown[key]; }

bool Toolbox::UI::Input::GetKeyDown(uint32_t key) {
    bool is_down = mKeysDown[key] && !mPrevKeysDown[key];
    return is_down;
}

bool Toolbox::UI::Input::GetKeyUp(uint32_t key) { return mPrevKeysDown[key] && !mKeysDown[key]; }

bool Toolbox::UI::Input::GetMouseButton(uint32_t button) { return mMouseButtonsDown[button]; }

bool Toolbox::UI::Input::GetMouseButtonDown(uint32_t button) {
    return mMouseButtonsDown[button] && !mPrevMouseButtonsDown[button];
}

bool Toolbox::UI::Input::GetMouseButtonUp(uint32_t button) {
    return mPrevMouseButtonsDown[button] && !mMouseButtonsDown[button];
}

ImVec2 Toolbox::UI::Input::GetMouseViewportPosition() { return mMousePosition; }

ImVec2 Toolbox::UI::Input::GetMouseDelta() { return mMouseDelta; }

int32_t Toolbox::UI::Input::GetMouseScrollDelta() { return mMouseScrollDelta; }

bool Toolbox::UI::Input::GetMouseWrapped() { return mMouseWrapped; }

void Toolbox::UI::Input::SetMousePosition(ImVec2 pos, bool overwrite_delta) {
    if (!overwrite_delta) {
        SetMouseWrapped(true);
    }
#if WIN32
    SetCursorPos((int)pos.x, (int)pos.y);
#elif __linux__
    Display *display = XOpenDisplay(0);
    if (display == nullptr)
        return;

    ImVec2 rel_pos = {mMousePosition.x - pos.x, mMousePosition.y - pos.y};
    XWarpPointer(display, 0, 0, 0, 0, 0, 0, rel_pos.x, rel_pos.y);

    XFlush(display);
    XCloseDisplay(display);
#else
#error "Unsupported OS"
#endif
}

void Toolbox::UI::Input::SetMouseWrapped(bool wrapped) { mMouseWrapped = wrapped; }

void Toolbox::UI::Input::UpdateInputState() {
    if (!mMouseWrapped) {
        mMouseDelta = {mMousePosition.x - mPrevMousePosition.x,
                       mMousePosition.y - mPrevMousePosition.y};
    } else {
        mMouseWrapped = false;
    }
}

void Toolbox::UI::Input::PostUpdateInputState() {
    for (int i = 0; i < KEY_MAX; i++)
        mPrevKeysDown[i] = mKeysDown[i];
    for (int i = 0; i < MOUSE_BUTTON_MAX; i++)
        mPrevMouseButtonsDown[i] = mMouseButtonsDown[i];

    mPrevMousePosition = mMousePosition;

    mMouseScrollDelta = 0;
}

void Toolbox::UI::Input::GLFWKeyCallback(GLFWwindow *window, int key, int scancode, int action,
                                         int mods) {
    if (key >= KEY_MAX)
        return;

    if (action == GLFW_PRESS)
        SetKeyboardState(key, true);
    else if (action == GLFW_RELEASE)
        SetKeyboardState(key, false);

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

void Toolbox::UI::Input::GLFWMousePositionCallback(GLFWwindow *window, double xpos, double ypos) {
    SetMousePosition((uint32_t)xpos, (uint32_t)ypos);

    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
}

void Toolbox::UI::Input::GLFWMouseButtonCallback(GLFWwindow *window, int button, int action,
                                                 int mods) {
    if (button >= MOUSE_BUTTON_MAX)
        return;

    if (action == GLFW_PRESS)
        SetMouseState(button, true);
    else if (action == GLFW_RELEASE)
        SetMouseState(button, false);

    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}

void Toolbox::UI::Input::GLFWMouseScrollCallback(GLFWwindow *window, double xoffset,
                                                 double yoffset) {
    SetMouseScrollDelta((uint32_t)yoffset);

    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}
