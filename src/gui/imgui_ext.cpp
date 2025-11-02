#include "gui/imgui_ext.hpp"
#include <IconsForkAwesome.h>
#include <string>
#include <vector>

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <glfw/glfw3.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <GLFW/glfw3.h>
#endif
#include <imgui_impl_glfw.cpp>
#include <imgui_impl_opengl3.h>

#pragma comment(lib, "opengl32.lib")

static const ImGuiDataTypeInfo GDataTypeInfo[] = {
    {sizeof(char),           "S8",     "%d",    "%d"   }, // ImGuiDataType_S8
    {sizeof(unsigned char),  "U8",     "%u",    "%u"   },
    {sizeof(short),          "S16",    "%d",    "%d"   }, // ImGuiDataType_S16
    {sizeof(unsigned short), "U16",    "%u",    "%u"   },
    {sizeof(int),            "S32",    "%d",    "%d"   }, // ImGuiDataType_S32
    {sizeof(unsigned int),   "U32",    "%u",    "%u"   },
#ifdef _MSC_VER
    {sizeof(ImS64),          "S64",    "%I64d", "%I64d"}, // ImGuiDataType_S64
    {sizeof(ImU64),          "U64",    "%I64u", "%I64u"},
#else
    {sizeof(ImS64), "S64", "%lld", "%lld"},  // ImGuiDataType_S64
    {sizeof(ImU64), "U64", "%llu", "%llu"},
#endif
    {sizeof(float),          "float",  "%.3f",
     "%f"                                              }, // ImGuiDataType_Float (float are promoted to double in va_arg)
    {sizeof(double),         "double", "%f",    "%lf"  }, // ImGuiDataType_Double
    {sizeof(bool),           "bool",   "%d",    "%d"   }, // ImGuiDataType_Bool
    {0,                      "char*",  "%s",    "%s"   }, // ImGuiDataType_String
};
IM_STATIC_ASSERT(IM_ARRAYSIZE(GDataTypeInfo) == ImGuiDataType_COUNT);

// ----------------------------------------------- //

static std::vector<bool *> s_open_stack;

bool ImGui::BeginGroupPanel(const char *name, bool *open, const ImVec2 &size) {
    ImGui::PushID(name);
    ImGui::BeginGroup();

    auto cursorPos   = ImGui::GetCursorScreenPos();
    auto itemSpacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto frameHeight = ImGui::GetFrameHeight();
    ImGui::BeginGroup();

    ImVec2 effectiveSize = size;
    if (size.x <= 0.0f)
        effectiveSize.x = ImGui::GetContentRegionAvail().x;
    else
        effectiveSize.x = size.x;
    ImGui::Dummy(ImVec2(effectiveSize.x, 0.0f));

    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);

    std::string name_view(name);
    auto name_end            = name_view.find("#");
    std::string visible_name = name_view.substr(0, name_end);

    if (open != nullptr) {
        /*ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 50.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {5.0f, 0.0f});
        */

        // Push transparent colors for the button's background in various states

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(
                                                 ImGui::GetStyle().Colors[ImGuiCol_Button]));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));  // Transparent background
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              IM_COL32(0, 0, 0, 0));  // Transparent background when hovered
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              IM_COL32(0, 0, 0, 0));  // Transparent background when active
        if (*open) {
            if (ImGui::ArrowButton("close_frame", ImGuiDir_Down)) {
                *open = false;
            }
        } else {
            if (ImGui::ArrowButton("open_frame", ImGuiDir_Right)) {
                *open = true;
            }
        }
        // ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }
    auto labelMin = ImGui::GetItemRectMin();

    ImGui::SameLine(0.0f, 5.0f);
    ImGui::TextUnformatted(visible_name.c_str());
    auto labelMax = ImGui::GetItemRectMax();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(0.0, frameHeight + itemSpacing.y));

    ImGui::BeginGroup();

    // ImGui::GetWindowDrawList()->AddRect(labelMin, labelMax, IM_COL32(255, 0, 255, 255));

    ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
    ImGui::GetCurrentWindow()->ContentRegionRect.Max.x -= frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->WorkRect.Max.x -= frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->InnerRect.Max.x -= frameHeight * 0.5f;
#else
    ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x -= frameHeight * 0.5f;
#endif
    ImGui::GetCurrentWindow()->Size.x -= frameHeight;

    auto itemWidth = ImGui::CalcItemWidth();
    ImGui::PushItemWidth(ImMax(0.0f, itemWidth - frameHeight));

    s_GroupPanelLabelStack.push_back(ImRect(labelMin, labelMax));
    s_open_stack.push_back(open);

    return open ? *open : true;
}

void ImGui::EndGroupPanel() {
    ImGui::PopItemWidth();

    auto itemSpacing = ImGui::GetStyle().ItemSpacing;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto frameHeight = ImGui::GetFrameHeight();

    ImGui::EndGroup();

    // ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(),
    // ImGui::GetItemRectMax(), IM_COL32(0, 255, 0, 64), 4.0f);

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::Dummy(ImVec2(0.0, frameHeight - frameHeight * 0.5f - itemSpacing.y));

    ImGui::EndGroup();

    auto itemMin = ImGui::GetItemRectMin();
    auto itemMax = ImGui::GetItemRectMax();
    // ImGui::GetWindowDrawList()->AddRectFilled(itemMin, itemMax, IM_COL32(255, 0, 0,
    // 64), 4.0f);

    auto labelRect = s_GroupPanelLabelStack.back();
    s_GroupPanelLabelStack.pop_back();
    s_open_stack.pop_back();

    ImVec2 halfFrame = ImVec2(frameHeight * 0.25f, frameHeight) * 0.5f;
    ImRect frameRect = ImRect(itemMin + halfFrame, itemMax - ImVec2(halfFrame.x, 0.0f));
    labelRect.Min.x -= itemSpacing.x;
    labelRect.Max.x += itemSpacing.x;
    for (int i = 0; i < 4; ++i) {
        switch (i) {
        // left half-plane
        case 0:
            ImGui::PushClipRect(ImVec2(-FLT_MAX, -FLT_MAX), ImVec2(labelRect.Min.x, FLT_MAX), true);
            break;
        // right half-plane
        case 1:
            ImGui::PushClipRect(ImVec2(labelRect.Max.x, -FLT_MAX), ImVec2(FLT_MAX, FLT_MAX), true);
            break;
        // top
        case 2:
            ImGui::PushClipRect(ImVec2(labelRect.Min.x, -FLT_MAX),
                                ImVec2(labelRect.Max.x, labelRect.Min.y), true);
            break;
        // bottom
        case 3:
            ImGui::PushClipRect(ImVec2(labelRect.Min.x, labelRect.Max.y),
                                ImVec2(labelRect.Max.x, FLT_MAX), true);
            break;
        }

        ImGui::GetWindowDrawList()->AddRect(frameRect.Min, frameRect.Max,
                                            ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)),
                                            halfFrame.x);

        ImGui::PopClipRect();
    }

    ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
    ImGui::GetCurrentWindow()->ContentRegionRect.Max.x += frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->WorkRect.Max.x += frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->InnerRect.Max.x += frameHeight * 0.5f;
#else
    ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x += frameHeight * 0.5f;
#endif
    ImGui::GetCurrentWindow()->Size.x += frameHeight;

    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    ImGui::EndGroup();

    ImGui::PopID();
}

std::vector<ImGuiID> s_panel_stack = {};

// Helper to create a child window / scrolling region that looks like a normal widget frame.
bool ImGui::BeginChildPanel(ImGuiID id, const ImVec2 &size, ImGuiWindowFlags extra_flags) {
    s_panel_stack.push_back(id);
    const ImGuiStyle &style = ImGui::GetStyle();
    PushStyleColor(
        ImGuiCol_ChildBg,
        style.Colors[s_panel_stack.size() % 1 ? ImGuiCol_TableRowBgAlt : ImGuiCol_TableRowBg]);
    PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
    PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
    PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    bool ret =
        BeginChild(id, size, true,
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysUseWindowPadding | extra_flags);
    PopStyleVar(3);
    PopStyleColor();
    return ret;
}

void ImGui::EndChildPanel() {
    EndChild();
    s_panel_stack.pop_back();
}

// Render a rectangle shaped with optional rounding and borders
void ImGui::RenderFrame(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool border, float rounding,
                        ImDrawFlags draw_flags) {
    ImGuiContext &g     = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    window->DrawList->AddRectFilled(p_min, p_max, fill_col, rounding, draw_flags);
    const float border_size = g.Style.FrameBorderSize;
    if (border && border_size > 0.0f) {
        window->DrawList->AddRect(p_min + ImVec2(1, 1), p_max + ImVec2(1, 1),
                                  GetColorU32(ImGuiCol_BorderShadow), rounding, draw_flags,
                                  border_size);
        window->DrawList->AddRect(p_min, p_max, GetColorU32(ImGuiCol_Border), rounding, draw_flags,
                                  border_size);
    }
}

bool ImGui::ButtonEx(const char *label, const ImVec2 &size_arg, ImGuiButtonFlags flags,
                     ImDrawFlags draw_flags) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g         = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id        = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
        style.FramePadding.y <
            window->DC.CurrLineTextBaseOffset)  // Try to vertically align buttons that are
                                                // smaller/have no padding so that text baseline
                                                // matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f,
                               label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
                                  : hovered         ? ImGuiCol_ButtonHovered
                                                    : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding, draw_flags);

    if (g.LogEnabled)
        LogSetNextTextDecoration("[", "]");
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL,
                      &label_size, style.ButtonTextAlign, &bb);

    // Automatically close popups
    // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags &
    // ImGuiWindowFlags_Popup))
    //    CloseCurrentPopup();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

bool ImGui::Button(const char *label, float rounding, ImDrawFlags draw_flags) {
    return Button(label, ImVec2(0, 0), rounding, draw_flags);
}

bool ImGui::Button(const char *label, const ImVec2 &size, float rounding, ImDrawFlags draw_flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
    bool ret = ButtonEx(label, size, ImGuiButtonFlags_None, draw_flags);
    ImGui::PopStyleVar();
    return ret;
}

bool ImGui::AlignedButton(const char *label, ImVec2 size, ImGuiButtonFlags flags) {
    return AlignedButton(label, size, flags, ImDrawFlags_RoundCornersNone);
}

bool ImGui::AlignedButton(const char *label, ImVec2 size, ImGuiButtonFlags flags,
                          ImDrawFlags draw_flags) {
    ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
    ImVec2 text_size     = ImGui::CalcTextSize(label);

    bool clicked = false;

    ImGui::PushID(label);
    {
        ImVec2 button_pos     = ImGui::GetCursorPos();
        clicked               = ImGui::ButtonEx("##button", size, flags, draw_flags);
        ImVec2 button_end_pos = ImGui::GetCursorPos();
        ImVec2 button_size    = ImGui::GetItemRectSize();

        ImGui::SameLine();

        ImGui::SetCursorPos(
            {button_pos.x + button_size.x * 0.5f - text_size.x * 0.5f,
             button_pos.y + button_size.y * 0.5f - text_size.y * 0.5f - frame_padding.y * 0.5f});
        if (GImGui->DisabledStackSize > 0) {
            ImGui::TextDisabled(label);
        } else {
            ImGui::Text(label);
        }
        ImGui::SetCursorPos(button_end_pos);
    }
    ImGui::PopID();

    return clicked;
}

bool ImGui::AlignedButton(const char *label, ImVec2 size, ImGuiButtonFlags flags, float rounding,
                          ImDrawFlags draw_flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
    bool ret = AlignedButton(label, size, flags, draw_flags);
    ImGui::PopStyleVar();
    return ret;
}

bool ImGui::SwitchButton(const char *label, bool active, ImVec2 size, ImGuiButtonFlags flags) {
    return SwitchButton(label, active, size, flags, ImDrawFlags_RoundCornersNone);
}

bool ImGui::SwitchButton(const char *label, bool active, ImVec2 size, ImGuiButtonFlags flags,
                         ImDrawFlags draw_flags) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    bool ret = AlignedButton(label, size, flags, draw_flags);
    if (active) {
        ImGui::PopStyleColor();
    }
    return ret;
}

bool ImGui::SwitchButton(const char *label, bool active, ImVec2 size, ImGuiButtonFlags flags,
                         float rounding, ImDrawFlags draw_flags) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    bool ret = AlignedButton(label, size, flags, rounding, draw_flags);
    if (active) {
        ImGui::PopStyleColor();
    }
    return ret;
}

static inline ImGuiInputTextFlags InputScalar_DefaultCharsFilter(ImGuiDataType data_type,
                                                                 const char *format) {
    if (data_type == ImGuiDataType_Float || data_type == ImGuiDataType_Double)
        return ImGuiInputTextFlags_CharsScientific;
    const char format_last_char = format[0] ? format[strlen(format) - 1] : 0;
    return (format_last_char == 'x' || format_last_char == 'X')
               ? ImGuiInputTextFlags_CharsHexadecimal
               : ImGuiInputTextFlags_CharsDecimal;
}

bool ImGui::ArrowButtonEx(const char *str_id, ImGuiDir dir, ImVec2 size, ImGuiButtonFlags flags,
                          float arrow_scale) {
    ImGuiContext &g     = *GImGui;
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiID id = window->GetID(str_id);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const float default_size = GetFrameHeight();
    ItemSize(size, (size.y >= default_size) ? g.Style.FramePadding.y : -1.0f);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 bg_col   = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
                                       : hovered         ? ImGuiCol_ButtonHovered
                                                         : ImGuiCol_Button);
    const ImU32 text_col = GetColorU32(ImGuiCol_Text);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, bg_col, true, g.Style.FrameRounding);
    RenderArrow(window->DrawList,
                bb.Min - ImVec2((size.x - g.FontSize) * -0.5f, (size.y - g.FontSize) * 0.5f),
                text_col, dir, arrow_scale);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, str_id, g.LastItemData.StatusFlags);
    return pressed;
}

// Note: p_data, p_step, p_step_fast are _pointers_ to a memory address holding the data. For an
// Input widget, p_step and p_step_fast are optional. Read code of e.g. InputFloat(), InputInt()
// etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool ImGui::InputScalarCompact(const char *label, ImGuiDataType data_type, void *p_data,
                               const void *p_step, const void *p_step_fast, const char *format,
                               ImGuiInputTextFlags flags) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g   = *GImGui;
    ImGuiStyle &style = g.Style;

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    void *p_data_default = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasRefVal)
                               ? &g.NextItemData.RefVal
                               : &g.DataTypeZeroValue;

    char buf[64];
    if ((flags & ImGuiInputTextFlags_DisplayEmptyRefVal) &&
        DataTypeCompare(data_type, p_data, p_data_default) == 0)
        buf[0] = 0;
    else
        DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, p_data, format);

    // Disable the MarkItemEdited() call in InputText but keep ImGuiItemStatusFlags_Edited.
    // We call MarkItemEdited() ourselves by comparing the actual data rather than the string.
    g.NextItemData.ItemFlags |= ImGuiItemFlags_NoMarkEdited;
    flags |= ImGuiInputTextFlags_AutoSelectAll |
             (ImGuiInputTextFlags)ImGuiInputTextFlags_LocalizeDecimalPoint;

    bool value_changed = false;
    if (p_step == NULL) {
        if (InputText(label, buf, IM_ARRAYSIZE(buf), flags))
            value_changed = DataTypeApplyFromText(buf, data_type, p_data, format);
    } else {
        const float button_size = GetFrameHeight() / 2;

        BeginGroup();  // The only purpose of the group here is to allow the caller to query item
                       // data e.g. IsItemActive()
        PushID(label);

        float base_xpos = GetCursorPosX();
        float base_ypos = GetCursorPosY();

        // Step buttons

        const ImVec2 backup_frame_padding = style.FramePadding;
        style.FramePadding.x              = style.FramePadding.y;
        ImGuiButtonFlags button_flags     = ImGuiButtonFlags_None;
        if (flags & ImGuiInputTextFlags_ReadOnly)
            BeginDisabled();

        ImVec2 window_pos = GetWindowPos();

        if (ArrowButtonEx("__internal_increase_value", ImGuiDir_Up,
                          ImVec2(button_size, button_size), button_flags, 0.5f)) {
            DataTypeApplyOp(data_type, '+', p_data, p_data,
                            g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
            value_changed = true;
        }
        SetCursorPos({base_xpos, base_ypos + button_size});
        if (ArrowButtonEx("__internal_decrease_value", ImGuiDir_Down,
                          ImVec2(button_size, button_size), button_flags, 0.5f)) {
            DataTypeApplyOp(data_type, '-', p_data, p_data,
                            g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
            value_changed = true;
        }

        SetCursorPos({base_xpos + button_size, base_ypos});
        PushStyleVar(ImGuiStyleVar_FramePadding, {6, GetStyle().FramePadding.y});
        SetNextItemWidth(ImMax(1.0f, CalcItemWidth() - button_size));
        if (InputText(
                "", buf, IM_ARRAYSIZE(buf),
                flags))  // PushId(label) + "" gives us the expected ID from outside point of view
            value_changed = DataTypeApplyFromText(buf, data_type, p_data, format);
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label,
                                    g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Inputable);
        PopStyleVar();

        if (flags & ImGuiInputTextFlags_ReadOnly)
            EndDisabled();

        const char *label_end = FindRenderedTextEnd(label);
        if (label != label_end) {
            SameLine(0, style.ItemInnerSpacing.x);
            TextEx(label, label_end);
        }
        style.FramePadding = backup_frame_padding;

        PopID();
        EndGroup();
    }
    if (value_changed)
        MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

bool ImGui::InputScalarCompactN(const char *label, ImGuiDataType data_type, void *p_data,
                                int components, const void *p_step, const void *p_step_fast,
                                const char *format, ImGuiInputTextFlags flags) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g    = *GImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++) {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |=
            InputScalarCompact("", data_type, p_data, p_step, p_step_fast, format, flags);
        PopID();
        PopItemWidth();
        p_data = (void *)((char *)p_data + type_size);
    }
    PopID();

    const char *label_end = FindRenderedTextEnd(label);
    if (label != label_end) {
        SameLine(0.0f, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool ImGui::TreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, bool focused) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    return TreeNodeBehavior(window->GetID(label), flags, label, NULL, focused);
}

// Store ImGuiTreeNodeStackData for just submitted node.
// Currently only supports 32 level deep and we are fine with (1 << Depth) overflowing into a zero,
// easy to increase.
static void TreeNodeStoreStackData(ImGuiTreeNodeFlags flags, float x1) {
    ImGuiContext &g     = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;

    g.TreeNodeStack.resize(g.TreeNodeStack.Size + 1);
    ImGuiTreeNodeStackData *tree_node_data = &g.TreeNodeStack.Data[g.TreeNodeStack.Size - 1];
    tree_node_data->ID                     = g.LastItemData.ID;
    tree_node_data->TreeFlags              = flags;
    tree_node_data->ItemFlags              = g.LastItemData.ItemFlags;
    tree_node_data->NavRect                = g.LastItemData.NavRect;

    // Initially I tried to latch value for GetColorU32(ImGuiCol_TreeLines) but it's not a good
    // trade-off for very large trees.
    const bool draw_lines =
        (flags & (ImGuiTreeNodeFlags_DrawLinesFull | ImGuiTreeNodeFlags_DrawLinesToNodes)) != 0;
    tree_node_data->DrawLinesX1 = draw_lines ? (x1 + g.FontSize * 0.5f + g.Style.FramePadding.x)
                                             : +FLT_MAX;
    tree_node_data->DrawLinesTableColumn =
        (draw_lines && g.CurrentTable) ? (ImGuiTableColumnIdx)g.CurrentTable->CurrentColumn : -1;
    tree_node_data->DrawLinesToNodesY2 = -FLT_MAX;
    window->DC.TreeHasStackDataDepthMask |= (1 << window->DC.TreeDepth);
    if (flags & ImGuiTreeNodeFlags_DrawLinesToNodes)
        window->DC.TreeRecordsClippedNodesY2Mask |= (1 << window->DC.TreeDepth);
}

bool ImGui::TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                             const char *label_end, bool focused) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g          = *GImGui;
    const ImGuiStyle &style  = g.Style;
    const bool display_frame = (flags & ImGuiTreeNodeFlags_Framed) != 0;
    const ImVec2 padding =
        (display_frame || (flags & ImGuiTreeNodeFlags_FramePadding))
            ? style.FramePadding
            : ImVec2(style.FramePadding.x,
                     ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));

    if (!label_end)
        label_end = FindRenderedTextEnd(label);
    const ImVec2 label_size = CalcTextSize(label, label_end, false);

    const float text_offset_x =
        g.FontSize +
        (display_frame ? padding.x * 3 : padding.x * 2);  // Collapsing arrow width + Spacing
    const float text_offset_y =
        ImMax(padding.y, window->DC.CurrLineTextBaseOffset);  // Latch before ItemSize changes it
    const float text_width = g.FontSize + label_size.x + padding.x * 2;  // Include collapsing arrow

    // We vertically grow up to current line height up the typical widget height.
    const float frame_height =
        ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2),
              label_size.y + padding.y * 2);
    const bool span_all_columns =
        (flags & ImGuiTreeNodeFlags_SpanAllColumns) != 0 && (g.CurrentTable != NULL);
    ImRect frame_bb;
    frame_bb.Min.x = span_all_columns                             ? window->ParentWorkRect.Min.x
                     : (flags & ImGuiTreeNodeFlags_SpanFullWidth) ? window->WorkRect.Min.x
                                                                  : window->DC.CursorPos.x;
    frame_bb.Min.y = window->DC.CursorPos.y;
    frame_bb.Max.x = span_all_columns ? window->ParentWorkRect.Max.x
                     : (flags & ImGuiTreeNodeFlags_SpanTextWidth)
                         ? window->DC.CursorPos.x + text_width + padding.x
                         : window->WorkRect.Max.x;
    frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
    if (display_frame) {
        const float outer_extend =
            IM_TRUNC(window->WindowPadding.x *
                     0.5f);  // Framed header expand a little outside of current limits
        frame_bb.Min.x -= outer_extend;
        frame_bb.Max.x += outer_extend;
    }

    ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
    ItemSize(ImVec2(text_width, frame_height), padding.y);

    // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
    ImRect interact_bb = frame_bb;
    if ((flags & (ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
                  ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanTextWidth |
                  ImGuiTreeNodeFlags_SpanAllColumns)) == 0)
        interact_bb.Max.x =
            frame_bb.Min.x + text_width + (label_size.x > 0.0f ? style.ItemSpacing.x * 2.0f : 0.0f);

    // Compute open and multi-select states before ItemAdd() as it clear NextItem data.
    ImGuiID storage_id = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasStorageID)
                             ? g.NextItemData.StorageId
                             : id;
    bool is_open       = TreeNodeUpdateNextOpen(storage_id, flags);

    bool is_visible;
    if (span_all_columns) {
        // Modify ClipRect for the ItemAdd(), faster than doing a
        // PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
        const float backup_clip_rect_min_x = window->ClipRect.Min.x;
        const float backup_clip_rect_max_x = window->ClipRect.Max.x;
        window->ClipRect.Min.x             = window->ParentWorkRect.Min.x;
        window->ClipRect.Max.x             = window->ParentWorkRect.Max.x;
        is_visible                         = ItemAdd(interact_bb, id);
        window->ClipRect.Min.x             = backup_clip_rect_min_x;
        window->ClipRect.Max.x             = backup_clip_rect_max_x;
    } else {
        is_visible = ItemAdd(interact_bb, id);
    }
    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
    g.LastItemData.DisplayRect = frame_bb;

    // If a NavLeft request is happening and ImGuiTreeNodeFlags_NavLeftJumpsBackHere enabled:
    // Store data for the current depth to allow returning to this node from any child item.
    // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode()
    // and TreePop(). It will become tempting to enable ImGuiTreeNodeFlags_NavLeftJumpsBackHere by
    // default or move it to ImGuiStyle.
    bool store_tree_node_stack_data = false;
    if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        if ((flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) && is_open && !g.NavIdIsAlive)
            if (g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window &&
                NavMoveRequestButNoResultYet())
                store_tree_node_stack_data = true;
    }

    const bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
    if (!is_visible) {
        if (store_tree_node_stack_data && is_open)
            TreeNodeStoreStackData(flags,
                                   text_pos.x - text_offset_x);  // Call before TreePushOverrideID()
        if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            TreePushOverrideID(id);
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label,
                                    g.LastItemData.StatusFlags |
                                        (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                        (is_open ? ImGuiItemStatusFlags_Opened : 0));
        return is_open;
    }

    if (span_all_columns) {
        TablePushBackgroundChannel();
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasClipRect;
        g.LastItemData.ClipRect = window->ClipRect;
    }

    ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
    if ((flags & ImGuiTreeNodeFlags_AllowOverlap) ||
        (g.LastItemData.ItemFlags & ImGuiItemFlags_AllowOverlap))
        button_flags |= ImGuiButtonFlags_AllowOverlap;
    if (!is_leaf)
        button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

    // We allow clicking on the arrow section with keyboard modifiers held, in order to easily
    // allow browsing a tree while preserving selection with code implementing multi-selection
    // patterns. When clicking on the rest of the tree node we always disallow keyboard modifiers.
    const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
    const float arrow_hit_x2 =
        (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
    const bool is_mouse_x_over_arrow =
        (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);

    const bool is_multi_select = (g.LastItemData.ItemFlags & ImGuiItemFlags_IsMultiSelect) != 0;
    if (is_multi_select)  // We absolutely need to distinguish open vs select so _OpenOnArrow comes
                          // by default
        flags |= (flags & ImGuiTreeNodeFlags_OpenOnMask_) == 0
                     ? ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                     : ImGuiTreeNodeFlags_OpenOnArrow;

    // Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
    // Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to
    // requirements for multi-selection and drag and drop support.
    // - Single-click on label = Toggle on MouseUp (default, when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
    // - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
    // - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and
    // _OpenOnArrow=0) It is rather standard that arrow click react on Down rather than Up. We set
    // ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be
    // active on the initial MouseDown in order for drag and drop to work.
    if (is_mouse_x_over_arrow)
        button_flags |= ImGuiButtonFlags_PressedOnClick;
    else if (flags & ImGuiTreeNodeFlags_OpenOnDoubleClick)
        button_flags |=
            ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
    else
        button_flags |= ImGuiButtonFlags_PressedOnClickRelease;

    bool selected           = (flags & ImGuiTreeNodeFlags_Selected) != 0;
    const bool was_selected = selected;

    // Multi-selection support (header)
    if (is_multi_select) {
        // Handle multi-select + alter button flags for it
        MultiSelectItemHeader(id, &selected, &button_flags);
        if (is_mouse_x_over_arrow)
            button_flags = (button_flags | ImGuiButtonFlags_PressedOnClick) &
                           ~ImGuiButtonFlags_PressedOnClickRelease;
    } else {
        if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
            button_flags |= ImGuiButtonFlags_NoKeyModsAllowed;
    }

    bool hovered, held;
    bool pressed = ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
    bool toggled = false;
    if (!is_leaf) {
        if (pressed && g.DragDropHoldJustPressedId != id) {
            if ((flags & ImGuiTreeNodeFlags_OpenOnMask_) == 0 ||
                (g.NavActivateId == id && !is_multi_select))
                toggled = true;  // Single click
            if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
                toggled |=
                    is_mouse_x_over_arrow &&
                    !g.NavHighlightItemUnderNav;  // Lightweight equivalent of IsMouseHoveringRect()
                                                  // since ButtonBehavior() already did the job
            if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseClickedCount[0] == 2)
                toggled = true;  // Double click
        } else if (pressed && g.DragDropHoldJustPressedId == id) {
            IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
            if (!is_open)  // When using Drag and Drop "hold to open" we keep the node highlighted
                           // after opening, but never close it again.
                toggled = true;
            else
                pressed = false;  // Cancel press so it doesn't trigger selection.
        }

        if (g.NavId == id && g.NavMoveDir == ImGuiDir_Left && is_open) {
            toggled = true;
            NavClearPreferredPosForAxis(ImGuiAxis_X);
            NavMoveRequestCancel();
        }
        if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right &&
            !is_open)  // If there's something upcoming on the line we may want to give it the
                       // priority?
        {
            toggled = true;
            NavClearPreferredPosForAxis(ImGuiAxis_X);
            NavMoveRequestCancel();
        }

        if (toggled) {
            is_open = !is_open;
            window->DC.StateStorage->SetInt(storage_id, is_open);
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
        }
    }

    // Multi-selection support (footer)
    if (is_multi_select) {
        bool pressed_copy = pressed && !toggled;
        MultiSelectItemFooter(id, &selected, &pressed_copy);
        if (pressed)
            SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, interact_bb);
    }

    if (selected != was_selected)
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

    // Render
    {
        const ImU32 text_col                          = GetColorU32(ImGuiCol_Text);
        ImGuiNavRenderCursorFlags nav_highlight_flags = ImGuiNavHighlightFlags_Compact;
        if (is_multi_select)
            nav_highlight_flags |=
                ImGuiNavHighlightFlags_AlwaysDraw;  // Always show the nav rectangle
        if (display_frame) {
            // Framed type
            const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                                             : hovered         ? ImGuiCol_HeaderHovered
                                                               : ImGuiCol_Header);
            RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
            RenderNavHighlight(frame_bb, id, nav_highlight_flags);
            if (flags & ImGuiTreeNodeFlags_Bullet)
                RenderBullet(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f),
                    text_col);
            else if (!is_leaf)
                RenderArrow(window->DrawList,
                            ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y), text_col,
                            is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up
                                                                                    : ImGuiDir_Down)
                                    : ImGuiDir_Right,
                            1.0f);
            else  // Leaf without bullet, left-adjusted text
                text_pos.x -= text_offset_x - padding.x;
            if (flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
                frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
            if (g.LogEnabled)
                LogSetNextTextDecoration("###", "###");
        } else {
            // Unframed typed for tree nodes
            if (hovered || selected || focused) {
                const ImU32 bg_col = GetColorU32((held && hovered)    ? ImGuiCol_HeaderActive
                                                 : hovered || focused ? ImGuiCol_HeaderHovered
                                                                      : ImGuiCol_Header);
                RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
            }
            RenderNavHighlight(frame_bb, id, nav_highlight_flags);
            if (flags & ImGuiTreeNodeFlags_Bullet)
                RenderBullet(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f),
                    text_col);
            else if (!is_leaf)
                RenderArrow(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f),
                    text_col,
                    is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up
                                                                            : ImGuiDir_Down)
                            : ImGuiDir_Right,
                    0.70f);
            if (g.LogEnabled)
                LogSetNextTextDecoration(">", NULL);
        }

        if (span_all_columns)
            TablePopBackgroundChannel();

        // Label
        if (display_frame)
            RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
        else
            RenderText(text_pos, label, label_end, false);
    }

    if (store_tree_node_stack_data && is_open)
        TreeNodeStoreStackData(flags,
                               text_pos.x - text_offset_x);  // Call before TreePushOverrideID()
    if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        TreePushOverrideID(id);  // Could use TreePush(label) but this avoid computing twice

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                g.LastItemData.StatusFlags |
                                    (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                    (is_open ? ImGuiItemStatusFlags_Opened : 0));
    return is_open;
}

bool ImGui::TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                             const char *label_end, bool focused, bool *visible) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g          = *GImGui;
    const ImGuiStyle &style  = g.Style;
    const bool display_frame = (flags & ImGuiTreeNodeFlags_Framed) != 0;
    const ImVec2 padding =
        (display_frame || (flags & ImGuiTreeNodeFlags_FramePadding))
            ? style.FramePadding
            : ImVec2(style.FramePadding.x,
                     ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));

    if (!label_end)
        label_end = FindRenderedTextEnd(label);
    const ImVec2 label_size = CalcTextSize(label, label_end, false);
    const ImVec2 eye_size   = CalcTextSize(ICON_FK_EYE, nullptr, false);

    const float text_offset_x =
        g.FontSize + (display_frame
                          ? padding.x * 3
                          : padding.x * 2);  // Eye width + Collapsing arrow width + Spacing
    const float text_offset_y =
        ImMax(padding.y, window->DC.CurrLineTextBaseOffset);  // Latch before ItemSize changes it
    const float text_width = g.FontSize + label_size.x + padding.x * 2;  // Include collapsing arrow

    // We vertically grow up to current line height up the typical widget height.
    const float frame_height =
        ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2),
              label_size.y + padding.y * 2);
    const bool span_all_columns =
        (flags & ImGuiTreeNodeFlags_SpanAllColumns) != 0 && (g.CurrentTable != NULL);
    ImRect frame_bb;
    frame_bb.Min.x = span_all_columns                             ? window->ParentWorkRect.Min.x
                     : (flags & ImGuiTreeNodeFlags_SpanFullWidth) ? window->WorkRect.Min.x
                                                                  : window->DC.CursorPos.x;
    frame_bb.Min.y = window->DC.CursorPos.y;
    frame_bb.Max.x = span_all_columns ? window->ParentWorkRect.Max.x
                     : (flags & ImGuiTreeNodeFlags_SpanTextWidth)
                         ? window->DC.CursorPos.x + text_width + padding.x
                         : window->WorkRect.Max.x;
    frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
    if (display_frame) {
        const float outer_extend =
            IM_TRUNC(window->WindowPadding.x *
                     0.5f);  // Framed header expand a little outside of current limits
        frame_bb.Min.x -= outer_extend;
        frame_bb.Max.x += outer_extend;
    }

    ImVec2 eye_pos(frame_bb.Min.x + padding.x, window->DC.CursorPos.y);

    ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x + ImGui::GetFontSize(),
                    window->DC.CursorPos.y + text_offset_y);
    ItemSize(ImVec2(text_width, frame_height), padding.y);

    ImRect eye_interact_bb = frame_bb;
    eye_interact_bb.Max.x  = frame_bb.Min.x + eye_size.x + style.ItemSpacing.x;

    ImGuiID eye_interact_id = GetID("eye_button##internal");
    ItemAdd(eye_interact_bb, eye_interact_id);

    // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
    ImRect interact_bb = frame_bb;
    if ((flags & (ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
                  ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanTextWidth |
                  ImGuiTreeNodeFlags_SpanAllColumns)) == 0)
        interact_bb.Max.x =
            frame_bb.Min.x + text_width + (label_size.x > 0.0f ? style.ItemSpacing.x * 2.0f : 0.0f);

    // Compute open and multi-select states before ItemAdd() as it clear NextItem data.
    ImGuiID storage_id = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasStorageID)
                             ? g.NextItemData.StorageId
                             : id;
    bool is_open       = TreeNodeUpdateNextOpen(storage_id, flags);

    bool is_visible;
    if (span_all_columns) {
        // Modify ClipRect for the ItemAdd(), faster than doing a
        // PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
        const float backup_clip_rect_min_x = window->ClipRect.Min.x;
        const float backup_clip_rect_max_x = window->ClipRect.Max.x;
        window->ClipRect.Min.x             = window->ParentWorkRect.Min.x;
        window->ClipRect.Max.x             = window->ParentWorkRect.Max.x;
        is_visible                         = ItemAdd(interact_bb, id);
        window->ClipRect.Min.x             = backup_clip_rect_min_x;
        window->ClipRect.Max.x             = backup_clip_rect_max_x;
    } else {
        is_visible = ItemAdd(interact_bb, id);
    }

    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
    g.LastItemData.DisplayRect = frame_bb;

    // If a NavLeft request is happening and ImGuiTreeNodeFlags_NavLeftJumpsBackHere enabled:
    // Store data for the current depth to allow returning to this node from any child item.
    // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode()
    // and TreePop(). It will become tempting to enable ImGuiTreeNodeFlags_NavLeftJumpsBackHere by
    // default or move it to ImGuiStyle.
    bool store_tree_node_stack_data = false;
    if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        if ((flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) && is_open && !g.NavIdIsAlive)
            if (g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window &&
                NavMoveRequestButNoResultYet())
                store_tree_node_stack_data = true;
    }

    const bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
    if (!is_visible) {
        if (store_tree_node_stack_data && is_open)
            TreeNodeStoreStackData(flags,
                                   text_pos.x - text_offset_x);  // Call before TreePushOverrideID()
        if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            TreePushOverrideID(id);
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label,
                                    g.LastItemData.StatusFlags |
                                        (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                        (is_open ? ImGuiItemStatusFlags_Opened : 0));
        return is_open;
    }

    if (span_all_columns) {
        TablePushBackgroundChannel();
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasClipRect;
        g.LastItemData.ClipRect = window->ClipRect;
    }

    const float eye_hit_x1 = eye_pos.x - style.TouchExtraPadding.x;
    const float eye_hit_x2 = eye_pos.x + (eye_size.x) + style.TouchExtraPadding.x;
    const bool is_mouse_x_over_eye =
        (g.IO.MousePos.x >= eye_hit_x1 && g.IO.MousePos.x < eye_hit_x2);

    ImGuiButtonFlags eye_button_flags = ImGuiTreeNodeFlags_None;
    if ((flags & ImGuiTreeNodeFlags_AllowOverlap) ||
        (g.LastItemData.ItemFlags & ImGuiItemFlags_AllowOverlap))
        eye_button_flags |= ImGuiButtonFlags_AllowOverlap;
    eye_button_flags |= ImGuiButtonFlags_PressedOnClick;

    bool eye_hovered, eye_held;
    bool eye_pressed = ImGui::ButtonBehavior(eye_interact_bb, eye_interact_id, &eye_hovered,
                                             &eye_held, eye_button_flags);
    if (eye_pressed && is_mouse_x_over_eye) {
        *visible ^= true;
    }

    ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
    if ((flags & ImGuiTreeNodeFlags_AllowOverlap) ||
        (g.LastItemData.ItemFlags & ImGuiItemFlags_AllowOverlap))
        button_flags |= ImGuiButtonFlags_AllowOverlap;
    if (!is_leaf)
        button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

    // We allow clicking on the arrow section with keyboard modifiers held, in order to easily
    // allow browsing a tree while preserving selection with code implementing multi-selection
    // patterns. When clicking on the rest of the tree node we always disallow keyboard modifiers.
    const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
    const float arrow_hit_x2 =
        (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
    const bool is_mouse_x_over_arrow =
        (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);

    const bool is_multi_select = (g.LastItemData.ItemFlags & ImGuiItemFlags_IsMultiSelect) != 0;
    if (is_multi_select)  // We absolutely need to distinguish open vs select so _OpenOnArrow comes
                          // by default
        flags |= (flags & ImGuiTreeNodeFlags_OpenOnMask_) == 0
                     ? ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                     : ImGuiTreeNodeFlags_OpenOnArrow;

    // Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
    // Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to
    // requirements for multi-selection and drag and drop support.
    // - Single-click on label = Toggle on MouseUp (default, when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
    // - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
    // - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and
    // _OpenOnArrow=0) It is rather standard that arrow click react on Down rather than Up. We set
    // ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be
    // active on the initial MouseDown in order for drag and drop to work.
    if (is_mouse_x_over_arrow)
        button_flags |= ImGuiButtonFlags_PressedOnClick;
    else if (flags & ImGuiTreeNodeFlags_OpenOnDoubleClick)
        button_flags |=
            ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
    else
        button_flags |= ImGuiButtonFlags_PressedOnClickRelease;

    bool selected           = (flags & ImGuiTreeNodeFlags_Selected) != 0;
    const bool was_selected = selected;

    // Multi-selection support (header)
    if (is_multi_select) {
        // Handle multi-select + alter button flags for it
        MultiSelectItemHeader(id, &selected, &button_flags);
        if (is_mouse_x_over_arrow)
            button_flags = (button_flags | ImGuiButtonFlags_PressedOnClick) &
                           ~ImGuiButtonFlags_PressedOnClickRelease;
    } else {
        if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
            button_flags |= ImGuiButtonFlags_NoKeyModsAllowed;
    }

    bool hovered, held;
    bool pressed = ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
    bool toggled = false;
    if (!is_leaf) {
        if (pressed && g.DragDropHoldJustPressedId != id) {
            if ((flags & ImGuiTreeNodeFlags_OpenOnMask_) == 0 ||
                (g.NavActivateId == id && !is_multi_select))
                toggled = true;  // Single click
            if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
                toggled |=
                    is_mouse_x_over_arrow &&
                    !g.NavHighlightItemUnderNav;  // Lightweight equivalent of IsMouseHoveringRect()
                                                  // since ButtonBehavior() already did the job
            if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseClickedCount[0] == 2)
                toggled = true;  // Double click
        } else if (pressed && g.DragDropHoldJustPressedId == id) {
            IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
            if (!is_open)  // When using Drag and Drop "hold to open" we keep the node highlighted
                           // after opening, but never close it again.
                toggled = true;
            else
                pressed = false;  // Cancel press so it doesn't trigger selection.
        }

        if (g.NavId == id && g.NavMoveDir == ImGuiDir_Left && is_open) {
            toggled = true;
            NavClearPreferredPosForAxis(ImGuiAxis_X);
            NavMoveRequestCancel();
        }
        if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right &&
            !is_open)  // If there's something upcoming on the line we may want to give it the
                       // priority?
        {
            toggled = true;
            NavClearPreferredPosForAxis(ImGuiAxis_X);
            NavMoveRequestCancel();
        }

        if (toggled) {
            is_open = !is_open;
            window->DC.StateStorage->SetInt(storage_id, is_open);
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
        }
    }

    // Multi-selection support (footer)
    if (is_multi_select) {
        bool pressed_copy = pressed && !toggled;
        MultiSelectItemFooter(id, &selected, &pressed_copy);
        if (pressed)
            SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, interact_bb);
    }

    if (selected != was_selected)
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

    // Render
    {
        const ImU32 text_col                       = GetColorU32(ImGuiCol_Text);
        ImGuiNavRenderCursorFlags nav_highlight_flags = ImGuiNavHighlightFlags_Compact;
        if (is_multi_select)
            nav_highlight_flags |=
                ImGuiNavHighlightFlags_AlwaysDraw;  // Always show the nav rectangle
        if (display_frame) {
            // Framed type
            const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                                             : hovered         ? ImGuiCol_HeaderHovered
                                                               : ImGuiCol_Header);
            RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
            RenderNavHighlight(frame_bb, id, nav_highlight_flags);
            if (flags & ImGuiTreeNodeFlags_Bullet)
                RenderBullet(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f),
                    text_col);
            else if (!is_leaf)
                RenderArrow(window->DrawList,
                            ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y), text_col,
                            is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up
                                                                                    : ImGuiDir_Down)
                                    : ImGuiDir_Right,
                            1.0f);
            else  // Leaf without bullet, left-adjusted text
                text_pos.x -= text_offset_x - padding.x;
            if (flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
                frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
            if (g.LogEnabled)
                LogSetNextTextDecoration("###", "###");
        } else {
            // Unframed typed for tree nodes
            if (hovered || selected || focused) {
                const ImU32 bg_col = GetColorU32((held && hovered)    ? ImGuiCol_HeaderActive
                                                 : hovered || focused ? ImGuiCol_HeaderHovered
                                                                      : ImGuiCol_Header);
                RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
            }
            RenderNavHighlight(frame_bb, id, nav_highlight_flags);
            if (flags & ImGuiTreeNodeFlags_Bullet)
                RenderBullet(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f),
                    text_col);
            else if (!is_leaf)
                RenderArrow(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f),
                    text_col,
                    is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up
                                                                            : ImGuiDir_Down)
                            : ImGuiDir_Right,
                    0.70f);
            if (g.LogEnabled)
                LogSetNextTextDecoration(">", NULL);
        }

        if (span_all_columns)
            TablePopBackgroundChannel();

        // Label
        if (display_frame)
            RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
        else
            RenderText(text_pos, label, label_end, false);

        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0, 0, 0, 0});

        ImGui::RenderText(eye_pos, *visible ? ICON_FK_EYE : ICON_FK_EYE_SLASH, nullptr, false);

        ImGui::PopStyleColor(3);
    }

    if (store_tree_node_stack_data && is_open)
        TreeNodeStoreStackData(flags,
                               text_pos.x - text_offset_x);  // Call before TreePushOverrideID()
    if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        TreePushOverrideID(id);  // Could use TreePush(label) but this avoid computing twice

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                g.LastItemData.StatusFlags |
                                    (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                    (is_open ? ImGuiItemStatusFlags_Opened : 0));
    return is_open;
}

bool ImGui::BeginPopupContextItem(const char *str_id, ImGuiPopupFlags popup_flags,
                                  ImGuiHoveredFlags hover_flags) {
    ImGuiContext &g     = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    if (window->SkipItems)
        return false;
    ImGuiID id =
        str_id ? window->GetID(str_id)
               : g.LastItemData.ID;  // If user hasn't passed an ID, we can use the LastItemID.
                                     // Using LastItemID as a Popup ID won't conflict!
    IM_ASSERT(id != 0);  // You cannot pass a NULL str_id if the last item has no identifier (e.g. a
                         // Text() item)
    int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
    if (IsMouseReleased(mouse_button) && IsItemHovered(hover_flags))
        OpenPopupEx(id, popup_flags);
    return BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoSavedSettings);
}

bool ImGui::DrawCircle(const ImVec2 &center, float radius, ImU32 color, ImU32 fill_color,
                       float thickness) {
    ImVec2 window_pos     = ImGui::GetWindowPos();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    if ((fill_color & IM_COL32_A_MASK) != 0) {
        // Draw border circle
        draw_list->AddCircleFilled(center + window_pos, radius, fill_color);
        draw_list->AddCircle(center + window_pos, radius, color, 0, thickness);
    } else {
        draw_list->AddCircle(center + window_pos, radius, color, 0, thickness);
    }
    return true;
}

bool ImGui::DrawSquare(const ImVec2 &center, float size, ImU32 color, ImU32 fill_color,
                       float thickness) {
    ImVec2 window_pos     = ImGui::GetWindowPos();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 p0(center.x - size / 2, center.y - size / 2);
    ImVec2 p1(center.x + size / 2, center.y + size / 2);
    if ((fill_color & IM_COL32_A_MASK) != 0) {
        draw_list->AddRectFilled(p0 + window_pos, p1 + window_pos, fill_color);
        draw_list->AddRect(p0 + window_pos, p1 + window_pos, color, 0, 0, thickness);
    } else {
        draw_list->AddRect(p0 + window_pos, p1 + window_pos, color, 0, 0, thickness);
    }
    return true;
}

bool ImGui::DrawNgon(int num_sides, const ImVec2 &center, float radius, ImU32 color,
                     ImU32 fill_color, float thickness, float angle) {
    if (num_sides < 3)
        return false;

    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImVec2 true_center = center + ImGui::GetWindowPos();

    const float angle_step = IM_PI * 2.0f / num_sides;
    ImVec2 p0(true_center.x + cosf(angle) * radius, true_center.y + sinf(angle) * radius);
    if ((fill_color & IM_COL32_A_MASK) != 0) {
        draw_list->PathLineTo(p0);
        for (int i = 1; i <= num_sides; i++) {
            const ImVec2 p1(true_center.x + cosf(angle + angle_step * i) * radius,
                            true_center.y + sinf(angle + angle_step * i) * radius);
            draw_list->PathLineTo(p1);
        }
        draw_list->PathFillConvex(fill_color);
    }
    for (int i = 1; i <= num_sides; i++) {
        const ImVec2 p1(true_center.x + cosf(angle + angle_step * i) * radius,
                        true_center.y + sinf(angle + angle_step * i) * radius);
        draw_list->AddLine(p0, p1, color, thickness);
        p0 = p1;
    }
    return true;
}

bool ImGui::DrawConvexPolygon(const ImVec2 *points, int num_points, ImU32 color, ImU32 fill_color,
                              float thickness) {
    if (num_points < 3)
        return false;

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos     = ImGui::GetWindowPos();

    ImVec2 *true_points = new ImVec2[num_points];
    {
        for (int i = 0; i < num_points; i++) {
            true_points[i] = points[i] + window_pos;
        }

        draw_list->AddConvexPolyFilled(true_points, num_points, fill_color);
        if (thickness > 0.0f) {
            draw_list->AddPolyline(true_points, num_points, color, ImDrawFlags_Closed, thickness);
        }
    }
    delete[] true_points;

    return true;
}

bool ImGui::DrawConcavePolygon(const ImVec2 *points, int num_points, ImU32 color, ImU32 fill_color,
                               float thickness) {
    if (num_points < 3)
        return false;

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos     = ImGui::GetWindowPos();

    ImVec2 *true_points = new ImVec2[num_points];
    {
        for (int i = 0; i < num_points; i++) {
            true_points[i] = points[i] + window_pos;
        }
        draw_list->AddConcavePolyFilled(true_points, num_points, fill_color);
        if (thickness > 0.0f) {
            draw_list->AddPolyline(true_points, num_points, color, ImDrawFlags_Closed, thickness);
        }
    }
    delete[] true_points;

    return true;
}

bool ImGui::IsDragDropSource(ImGuiDragDropFlags flags) {
    ImGuiContext &g     = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;

    // FIXME-DRAGDROP: While in the common-most "drag from non-zero active id" case we can tell the
    // mouse button, in both SourceExtern and id==0 cases we may requires something else (explicit
    // flags or some heuristic).
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;

    bool source_drag_active  = false;
    ImGuiID source_id        = 0;
    ImGuiID source_parent_id = 0;
    if (!(flags & ImGuiDragDropFlags_SourceExtern)) {
        source_id = g.LastItemData.ID;
        if (source_id != 0) {
            // Common path: items with ID
            if (g.ActiveId != source_id)
                return false;
            if (g.ActiveIdMouseButton != -1)
                mouse_button = g.ActiveIdMouseButton;
            if (g.IO.MouseDown[mouse_button] == false || window->SkipItems)
                return false;
            g.ActiveIdAllowOverlap = false;
        } else {
            // Uncommon path: items without ID
            if (g.IO.MouseDown[mouse_button] == false || window->SkipItems)
                return false;
            if ((g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredRect) == 0 &&
                (g.ActiveId == 0 || g.ActiveIdWindow != window))
                return false;

            // If you want to use BeginDragDropSource() on an item with no unique identifier for
            // interaction, such as Text() or Image(), you need to: A) Read the explanation below,
            // B) Use the ImGuiDragDropFlags_SourceAllowNullID flag.
            if (!(flags & ImGuiDragDropFlags_SourceAllowNullID)) {
                IM_ASSERT(0);
                return false;
            }

            // Magic fallback to handle items with no assigned ID, e.g. Text(), Image()
            // We build a throwaway ID based on current ID stack + relative AABB of items in window.
            // THE IDENTIFIER WON'T SURVIVE ANY REPOSITIONING/RESIZINGG OF THE WIDGET, so if your
            // widget moves your dragging operation will be canceled. We don't need to maintain/call
            // ClearActiveID() as releasing the button will early out this function and trigger
            // !ActiveIdIsAlive. Rely on keeping other window->LastItemXXX fields intact.
            source_id = g.LastItemData.ID = window->GetIDFromRectangle(g.LastItemData.Rect);
            KeepAliveID(source_id);
            bool is_hovered =
                ItemHoverable(g.LastItemData.Rect, source_id, g.LastItemData.ItemFlags);
            if (is_hovered && g.IO.MouseClicked[mouse_button]) {
                SetActiveID(source_id, window);
                FocusWindow(window);
            }
            if (g.ActiveId ==
                source_id)  // Allow the underlying widget to display/return hovered during the
                            // mouse release frame, else we would get a flicker.
                g.ActiveIdAllowOverlap = is_hovered;
        }
        if (g.ActiveId != source_id)
            return false;
        source_parent_id   = window->IDStack.back();
        source_drag_active = IsMouseDragging(mouse_button);

        // Disable navigation and key inputs while dragging + cancel existing request if any
        SetActiveIdUsingAllKeyboardKeys();
    } else {
        window             = NULL;
        source_id          = ImHashStr("#SourceExtern");
        source_drag_active = true;
    }

    IM_ASSERT(g.DragDropWithinTarget ==
              false);  // Can't nest BeginDragDropSource() and BeginDragDropTarget()
    return source_drag_active;
}

void ImGui::RenderDragDropTargetRect(const ImRect &bb, const ImRect &item_clip_rect,
                                     ImGuiDropFlags flags) {
    ImGuiContext &g     = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    ImRect bb_display   = bb;
    bb_display.ClipWith(item_clip_rect);  // Clip THEN expand so we have a way to visualize that
                                          // target is not entirely visible.
    bb_display.Expand(3.5f);
    bool push_clip_rect = !window->ClipRect.Contains(bb_display);
    if (push_clip_rect)
        window->DrawList->PushClipRectFullScreen();

    if (flags == ImGuiDropFlags_None || flags == ImGuiDropFlags_InsertChild) {
        window->DrawList->AddRect(bb_display.Min, bb_display.Max,
                                  GetColorU32(ImGuiCol_DragDropTarget), 0.0f, ImDrawFlags_None,
                                  2.0f);
    } else if (flags == ImGuiDropFlags_InsertBefore) {
        float y = bb_display.Min.y;
        window->DrawList->AddLine(ImVec2(bb_display.Min.x, y), ImVec2(bb_display.Max.x, y),
                                  GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
    } else if (flags == ImGuiDropFlags_InsertAfter) {
        float y = bb_display.Max.y;
        window->DrawList->AddLine(ImVec2(bb_display.Min.x, y), ImVec2(bb_display.Max.x, y),
                                  GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
    }

    if (push_clip_rect)
        window->DrawList->PopClipRect();
}

bool ImGui::TreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, bool focused, bool *visible) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    return TreeNodeBehavior(window->GetID(label), flags, label, NULL, focused, visible);
}

void ImGui_ImplGlfw_CreateWindow_Ex(ImGuiViewport *viewport) {
    ImGui_ImplGlfw_Data *bd         = ImGui_ImplGlfw_GetBackendData();
    ImGui_ImplGlfw_ViewportData *vd = IM_NEW(ImGui_ImplGlfw_ViewportData)();
    viewport->PlatformUserData      = vd;

    // Workaround for Linux: ignore mouse up events corresponding to losing focus of the previously
    // focused window (#7733, #3158, #7922)
#ifdef __linux__
    bd->MouseIgnoreButtonUpWaitForFocusLoss = true;
#endif

    // GLFW 3.2 unfortunately always set focus on glfwCreateWindow() if GLFW_VISIBLE is set,
    // regardless of GLFW_FOCUSED With GLFW 3.3, the hint GLFW_FOCUS_ON_SHOW fixes this problem
    glfwWindowHint(GLFW_VISIBLE, false);
    glfwWindowHint(GLFW_FOCUSED, false);
#if GLFW_HAS_FOCUS_ON_SHOW
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);
#endif
    glfwWindowHint(GLFW_DECORATED,
                   (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? false : true);
#if GLFW_HAS_WINDOW_TOPMOST
    glfwWindowHint(GLFW_FLOATING, (viewport->Flags & ImGuiViewportFlags_TopMost) ? true : false);
#endif
    GLFWwindow *share_window = (bd->ClientApi == GlfwClientApi_OpenGL) ? bd->Window : nullptr;
    vd->Window      = glfwCreateWindow((int)viewport->Size.x, (int)viewport->Size.y, "No Title Yet",
                                       nullptr, share_window);
    vd->WindowOwned = true;
    ImGui_ImplGlfw_ContextMap_Add(vd->Window, bd->Context);
    viewport->PlatformHandle = (void *)vd->Window;
#ifdef IMGUI_GLFW_HAS_SETWINDOWFLOATING
    ImGui_ImplGlfw_SetWindowFloating(vd->Window);
#endif
#ifdef _WIN32
    viewport->PlatformHandleRaw = glfwGetWin32Window(vd->Window);
    ::SetPropA((HWND)viewport->PlatformHandleRaw, "IMGUI_BACKEND_DATA", bd);
#elif defined(__APPLE__)
    viewport->PlatformHandleRaw = (void *)glfwGetCocoaWindow(vd->Window);
#endif
    glfwSetWindowPos(vd->Window, (int)viewport->Pos.x, (int)viewport->Pos.y);

    // Install GLFW callbacks for secondary viewports
    glfwSetWindowFocusCallback(vd->Window, ImGui_ImplGlfw_WindowFocusCallback);
    glfwSetCursorEnterCallback(vd->Window, ImGui_ImplGlfw_CursorEnterCallback);
    glfwSetCursorPosCallback(vd->Window, ImGui_ImplGlfw_CursorPosCallback);
    glfwSetMouseButtonCallback(vd->Window, ImGui_ImplGlfw_MouseButtonCallback);
    glfwSetScrollCallback(vd->Window, ImGui_ImplGlfw_ScrollCallback);
    glfwSetKeyCallback(vd->Window, ImGui_ImplGlfw_KeyCallback);
    glfwSetCharCallback(vd->Window, ImGui_ImplGlfw_CharCallback);
    glfwSetWindowCloseCallback(vd->Window, ImGui_ImplGlfw_WindowCloseCallback);
    glfwSetWindowPosCallback(vd->Window, ImGui_ImplGlfw_WindowPosCallback);
    glfwSetWindowSizeCallback(vd->Window, ImGui_ImplGlfw_WindowSizeCallback);
    if (bd->ClientApi == GlfwClientApi_OpenGL) {
        glfwMakeContextCurrent(vd->Window);
        glfwSwapInterval(0);
    }
}

void ImGui_ImplOpenGL3_RenderWindow_Ex(ImGuiViewport *viewport, void *) {
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear)) {
        ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        if ((viewport->Flags & ImGuiViewportFlags_TransparentFrameBuffer)) {
            clear_color.w = 0.0f;
        }
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    ImGui_ImplOpenGL3_RenderDrawData(viewport->DrawData);
}