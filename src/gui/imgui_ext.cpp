#include "gui/imgui_ext.hpp"
#include <gui/IconsForkAwesome.h>
#include <string>
#include <vector>
// #include <imgui.h>

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
