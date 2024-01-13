#pragma once

#include <cstdint>
#include <imgui.h>
#include <map>

struct GLFWwindow;

namespace Toolbox::UI::Input {
    bool GetKey(uint32_t key);
    bool GetKeyDown(uint32_t key);
    bool GetKeyUp(uint32_t key);

    bool GetMouseButton(uint32_t button);
    bool GetMouseButtonDown(uint32_t button);
    bool GetMouseButtonUp(uint32_t button);

    ImVec2 GetMousePosition();
    void SetMousePosition(ImVec2 pos, bool overwrite_delta = true);

    ImVec2 GetMouseDelta();
    int32_t GetMouseScrollDelta();

    bool GetMouseWrapped();
    void SetMouseWrapped(bool wrapped);

    void UpdateInputState();
    void PostUpdateInputState();

    void GLFWKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    void GLFWMousePositionCallback(GLFWwindow *window, double xpos, double ypos);
    void GLFWMouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    void GLFWMouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset);
}  // namespace Toolbox::UI::Input
