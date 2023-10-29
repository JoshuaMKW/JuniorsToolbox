#include "gui/imgui_ext.hpp"
#include <gui/IconsForkAwesome.h>
#include <string>
#include <vector>
// #include <imgui.h>

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

    char buf[64];
    DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, p_data, format);

    // Testing ActiveId as a minor optimization as filtering is not needed until active
    if (g.ActiveId == 0 &&
        (flags & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsHexadecimal |
                  ImGuiInputTextFlags_CharsScientific)) == 0)
        flags |= InputScalar_DefaultCharsFilter(data_type, format);
    flags |= ImGuiInputTextFlags_AutoSelectAll |
             ImGuiInputTextFlags_NoMarkEdited;  // We call MarkItemEdited() ourselves by comparing
                                                // the actual data rather than the string.

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
        ImGuiButtonFlags button_flags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
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
    frame_bb.Max.x = span_all_columns ? window->ParentWorkRect.Max.x : window->WorkRect.Max.x;
    frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
    if (display_frame) {
        // Framed header expand a little outside the default padding, to the edge of InnerClipRect
        // (FIXME: May remove this at some point and make InnerClipRect align with WindowPadding.x
        // instead of WindowPadding.x*0.5f)
        frame_bb.Min.x -= IM_TRUNC(window->WindowPadding.x * 0.5f - 1.0f);
        frame_bb.Max.x += IM_TRUNC(window->WindowPadding.x * 0.5f);
    }

    const float text_offset_x =
        g.FontSize +
        (display_frame ? padding.x * 3 : padding.x * 2);  // Collapsing arrow width + Spacing
    const float text_offset_y =
        ImMax(padding.y, window->DC.CurrLineTextBaseOffset);  // Latch before ItemSize changes it
    const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2
                                                               : 0.0f);  // Include collapsing
    ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
    ItemSize(ImVec2(text_width, frame_height), padding.y);

    // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
    ImRect interact_bb = frame_bb;
    if (!display_frame &&
        (flags & (ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth |
                  ImGuiTreeNodeFlags_SpanAllColumns)) == 0)
        interact_bb.Max.x = frame_bb.Min.x + text_width + style.ItemSpacing.x * 2.0f;

    // Modify ClipRect for the ItemAdd(), faster than doing a
    // PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
    const float backup_clip_rect_min_x = window->ClipRect.Min.x;
    const float backup_clip_rect_max_x = window->ClipRect.Max.x;
    if (span_all_columns) {
        window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
        window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
    }

    // Compute open and multi-select states before ItemAdd() as it clear NextItem data.
    bool is_open  = TreeNodeUpdateNextOpen(id, flags);
    bool item_add = ItemAdd(interact_bb, id);
    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
    g.LastItemData.DisplayRect = frame_bb;

    if (span_all_columns) {
        window->ClipRect.Min.x = backup_clip_rect_min_x;
        window->ClipRect.Max.x = backup_clip_rect_max_x;
    }

    // If a NavLeft request is happening and ImGuiTreeNodeFlags_NavLeftJumpsBackHere enabled:
    // Store data for the current depth to allow returning to this node from any child item.
    // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode()
    // and TreePop(). It will become tempting to enable ImGuiTreeNodeFlags_NavLeftJumpsBackHere by
    // default or move it to ImGuiStyle. Currently only supports 32 level deep and we are fine with
    // (1 << Depth) overflowing into a zero, easy to increase.
    if (is_open && !g.NavIdIsAlive && (flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) &&
        !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        if (g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window &&
            NavMoveRequestButNoResultYet()) {
            g.NavTreeNodeStack.resize(g.NavTreeNodeStack.Size + 1);
            ImGuiNavTreeNodeData *nav_tree_node_data = &g.NavTreeNodeStack.back();
            nav_tree_node_data->ID                   = id;
            nav_tree_node_data->InFlags              = g.LastItemData.InFlags;
            nav_tree_node_data->NavRect              = g.LastItemData.NavRect;
            window->DC.TreeJumpToParentOnPopMask |= (1 << window->DC.TreeDepth);
        }

    const bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
    if (!item_add) {
        if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            TreePushOverrideID(id);
        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label,
                                    g.LastItemData.StatusFlags |
                                        (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                        (is_open ? ImGuiItemStatusFlags_Opened : 0));
        return is_open;
    }

    if (span_all_columns)
        TablePushBackgroundChannel();

    ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
    if ((flags & ImGuiTreeNodeFlags_AllowOverlap) ||
        (g.LastItemData.InFlags & ImGuiItemFlags_AllowOverlap))
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
    if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
        button_flags |= ImGuiButtonFlags_NoKeyModifiers;

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

    bool hovered, held;
    bool pressed = ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
    bool toggled = false;
    if (!is_leaf) {
        if (pressed && g.DragDropHoldJustPressedId != id) {
            if ((flags & (ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) ==
                    0 ||
                (g.NavActivateId == id))
                toggled = true;
            if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
                toggled |=
                    is_mouse_x_over_arrow &&
                    !g.NavDisableMouseHover;  // Lightweight equivalent of IsMouseHoveringRect()
                                              // since ButtonBehavior() already did the job
            if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseClickedCount[0] == 2)
                toggled = true;
        } else if (pressed && g.DragDropHoldJustPressedId == id) {
            IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
            if (!is_open)  // When using Drag and Drop "hold to open" we keep the node highlighted
                           // after opening, but never close it again.
                toggled = true;
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
            window->DC.StateStorage->SetInt(id, is_open);
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
        }
    }

    // In this branch, TreeNodeBehavior() cannot toggle the selection so this will never trigger.
    if (selected != was_selected)  //-V547
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

    // Render
    const ImU32 text_col                       = GetColorU32(ImGuiCol_Text);
    ImGuiNavHighlightFlags nav_highlight_flags = ImGuiNavHighlightFlags_TypeThin;
    if (display_frame) {
        // Framed type
        const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                                         : hovered         ? ImGuiCol_HeaderHovered
                                                           : ImGuiCol_Header);
        RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
        RenderNavHighlight(frame_bb, id, nav_highlight_flags);
        if (flags & ImGuiTreeNodeFlags_Bullet)
            RenderBullet(window->DrawList,
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
            RenderBullet(window->DrawList,
                         ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f),
                         text_col);
        else if (!is_leaf)
            RenderArrow(
                window->DrawList,
                ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f),
                text_col,
                is_open
                    ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up : ImGuiDir_Down)
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

    if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        TreePushOverrideID(id);
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                g.LastItemData.StatusFlags |
                                    (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                    (is_open ? ImGuiItemStatusFlags_Opened : 0));
    return is_open;
}