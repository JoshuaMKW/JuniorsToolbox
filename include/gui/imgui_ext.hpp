#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

static ImVector<ImRect> s_GroupPanelLabelStack;

namespace ImGui {

    bool BeginGroupPanel(const char *name, bool *open, const ImVec2 &size);
    void EndGroupPanel();
    bool BeginChildPanel(ImGuiID id, const ImVec2 &size, ImGuiWindowFlags extra_flags = 0);
    void EndChildPanel();

}  // namespace ImGui