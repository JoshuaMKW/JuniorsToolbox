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

bool ImGui::ArrowButtonEx(const char *str_id, ImGuiDir dir, ImVec2 size, ImGuiButtonFlags flags, float arrow_scale) {
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
                bb.Min - ImVec2((size.x - g.FontSize) * -0.5f,
                                (size.y - g.FontSize) * 0.5f),
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

        ImVec2 window_pos   = GetWindowPos();

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
        PushStyleVar(ImGuiStyleVar_FramePadding,
                            {6, GetStyle().FramePadding.y});
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
                                int components,
                         const void *p_step, const void *p_step_fast, const char *format,
                         ImGuiInputTextFlags flags) {
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
        value_changed |= InputScalarCompact("", data_type, p_data, p_step, p_step_fast, format, flags);
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
