#pragma once

#include "imgui_internal.h"

static ImVector<ImRect> s_GroupPanelLabelStack;

namespace ImGui {

    bool BeginGroupPanel(const char *name, bool *open, const ImVec2 &size);
    void EndGroupPanel();
    bool BeginChildPanel(ImGuiID id, const ImVec2 &size, ImGuiWindowFlags extra_flags = 0);
    void EndChildPanel();
    bool ArrowButtonEx(const char *str_id, ImGuiDir dir, ImVec2 size, ImGuiButtonFlags flags,
                       float arrow_scale);
    bool InputScalarCompact(const char *label, ImGuiDataType data_type, void *p_data,
                            const void *p_step, const void *p_step_fast, const char *format,
                            ImGuiInputTextFlags flags = 0);
    bool InputScalarCompactN(const char *label, ImGuiDataType data_type, void *p_data,
                             int components, const void *p_step, const void *p_step_fast,
                             const char *format, ImGuiInputTextFlags flags = 0);
    bool TreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, bool focused);
    bool TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                          const char *label_end, bool focused);
}  // namespace ImGui