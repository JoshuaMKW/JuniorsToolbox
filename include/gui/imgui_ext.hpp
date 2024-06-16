#pragma once

#include "core/assert.hpp"
#include "core/core.hpp"

#ifdef IM_ASSERT
#undef IM_ASSERT
#endif

#define IM_ASSERT(_EXPR) TOOLBOX_CORE_ASSERT(_EXPR)

#ifndef TOOLBOX_DEBUG
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#endif

#include "imgui_internal.h"

static ImVector<ImRect> s_GroupPanelLabelStack;

enum ImGuiDropFlags_ {
    ImGuiDropFlags_None         = 0,
    ImGuiDropFlags_InsertBefore = 1,
    ImGuiDropFlags_InsertAfter  = 2,
    ImGuiDropFlags_InsertChild  = 3,
};

typedef int ImGuiDropFlags;

namespace ImGui {

    bool BeginGroupPanel(const char *name, bool *open, const ImVec2 &size);
    void EndGroupPanel();
    bool BeginChildPanel(ImGuiID id, const ImVec2 &size, ImGuiWindowFlags extra_flags = 0);
    void EndChildPanel();
    void RenderFrame(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool border, float rounding,
                     ImDrawFlags draw_flags);
    bool ButtonEx(const char *label, const ImVec2 &size_arg, ImGuiButtonFlags flags,
                  ImDrawFlags draw_flags);
    bool Button(const char *label, float rounding, ImDrawFlags draw_flags);
    bool Button(const char *label, const ImVec2 &size, float rounding, ImDrawFlags draw_flags);
    bool AlignedButton(const char *label, ImVec2 size = ImVec2{0, 0},
                       ImGuiButtonFlags flags = ImGuiButtonFlags_None);
    bool AlignedButton(const char *label, ImVec2 size, ImGuiButtonFlags flags,
                       ImDrawFlags draw_flags);
    bool AlignedButton(const char *label, ImVec2 size, ImGuiButtonFlags flags, float rounding,
                       ImDrawFlags draw_flags);
    bool SwitchButton(const char *label, bool active, ImVec2 size = ImVec2{0, 0},
                      ImGuiButtonFlags flags = ImGuiButtonFlags_None);
    bool SwitchButton(const char *label, bool active, ImVec2 size, ImGuiButtonFlags flags,
                      ImDrawFlags draw_flags);
    bool SwitchButton(const char *label, bool active, ImVec2 size, ImGuiButtonFlags flags,
                      float rounding, ImDrawFlags draw_flags);
    bool ArrowButtonEx(const char *str_id, ImGuiDir dir, ImVec2 size, ImGuiButtonFlags flags,
                       float arrow_scale);
    bool InputScalarCompact(const char *label, ImGuiDataType data_type, void *p_data,
                            const void *p_step, const void *p_step_fast, const char *format,
                            ImGuiInputTextFlags flags = 0);
    bool InputScalarCompactN(const char *label, ImGuiDataType data_type, void *p_data,
                             int components, const void *p_step, const void *p_step_fast,
                             const char *format, ImGuiInputTextFlags flags = 0);
    bool TreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, bool focused);
    bool TreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, bool focused, bool *visible);
    bool TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                          const char *label_end, bool focused);
    bool TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                          const char *label_end, bool focused, bool *visible);

    bool DrawCircle(const ImVec2 &center, float radius, ImU32 color,
                    ImU32 fill_color = IM_COL32_BLACK_TRANS, float thickness = 1.0f);
    bool DrawSquare(const ImVec2 &center, float size, ImU32 color,
                    ImU32 fill_color = IM_COL32_BLACK_TRANS, float thickness = 1.0f);
    bool DrawNgon(int num_sides, const ImVec2 &center, float radius, ImU32 color,
                  ImU32 fill_color = IM_COL32_BLACK_TRANS, float thickness = 1.0f,
                  float angle = 0.0f);
    bool DrawConvexPolygon(const ImVec2 *points, int num_points, ImU32 color,
                           ImU32 fill_color = IM_COL32_BLACK_TRANS, float thickness = 1.0f);
    bool DrawConcavePolygon(const ImVec2 *points, int num_points, ImU32 color,
                            ImU32 fill_color = IM_COL32_BLACK_TRANS, float thickness = 1.0f);

    bool IsDragDropSource(ImGuiDragDropFlags flags = ImGuiDragDropFlags_None);
    void RenderDragDropTargetRect(const ImRect &bb, const ImRect &item_clip_rect,
                                  ImGuiDropFlags flags);

}  // namespace ImGui

#define ImGuiViewportFlags_TransparentFrameBuffer (1 << 20)

void ImGui_ImplGlfw_CreateWindow_Ex(ImGuiViewport *viewport);
void ImGui_ImplOpenGL3_RenderWindow_Ex(ImGuiViewport *viewport, void *);