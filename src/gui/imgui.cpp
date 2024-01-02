#include "gui/imgui_ext.hpp"
#include <gui/IconsForkAwesome.h>

namespace Toolbox::UI {

    bool SceneTreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                               const char *label_end, bool focused, bool *visible) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const float row_y = ImGui::GetCursorPosY();

        ImGuiContext &g          = *GImGui;
        const ImGuiStyle &style  = g.Style;
        const bool display_frame = (flags & ImGuiTreeNodeFlags_Framed) != 0;
        ImVec2 padding           = (display_frame || (flags & ImGuiTreeNodeFlags_FramePadding))
                                       ? style.FramePadding
                                       : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset,
                                                                            style.FramePadding.y));
        padding.y *= 1.2f;

        if (!label_end)
            label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
        const ImVec2 eye_size   = ImGui::CalcTextSize(ICON_FK_EYE, nullptr, false);

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
            // Framed header expand a little outside the default padding, to the edge of
            // InnerClipRect (FIXME: May remove this at some point and make InnerClipRect align with
            // WindowPadding.x instead of WindowPadding.x*0.5f)
            frame_bb.Min.x -= IM_TRUNC(window->WindowPadding.x * 0.5f - 1.0f);
            frame_bb.Max.x += IM_TRUNC(window->WindowPadding.x * 0.5f);
        }

        const float text_offset_x =
            g.FontSize +
            (display_frame ? padding.x * 3 : padding.x * 2);  // Collapsing arrow width + Spacing
        const float text_offset_y = ImMax(
            padding.y, window->DC.CurrLineTextBaseOffset);  // Latch before ItemSize changes it
        const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2
                                                                   : 0.0f);  // Include collapsing
        ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x + label_size.y * 1.5f,
                        window->DC.CursorPos.y + text_offset_y);
        ImVec2 eye_pos(frame_bb.Min.x + padding.x, window->DC.CursorPos.y);

        ImGui::ItemSize(ImVec2(text_width, frame_height), padding.y);

        ImRect eye_interact_bb = frame_bb;
        eye_interact_bb.Max.x  = frame_bb.Min.x + eye_size.x + style.ItemSpacing.x;

        // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
        ImRect interact_bb = frame_bb;
        if (!display_frame &&
            (flags & (ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth |
                      ImGuiTreeNodeFlags_SpanAllColumns)) == 0)
            interact_bb.Max.x = frame_bb.Min.x + text_width + style.ItemSpacing.x * 2.0f;
        interact_bb.Min.x += eye_size.x + style.TouchExtraPadding.x;

        // Modify ClipRect for the ItemAdd(), faster than doing a
        // PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
        const float backup_clip_rect_min_x = window->ClipRect.Min.x;
        const float backup_clip_rect_max_x = window->ClipRect.Max.x;
        if (span_all_columns) {
            window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
            window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
        }

        // ImGui::SetActiveID(0, ImGui::GetCurrentWindow());
        ImGuiID eye_interact_id = ImGui::GetID("eye_button##internal");
        ImGui::ItemAdd(eye_interact_bb, eye_interact_id);

        // Compute open and multi-select states before ItemAdd() as it clear NextItem data.
        bool is_open  = ImGui::TreeNodeUpdateNextOpen(id, flags);
        bool item_add = ImGui::ItemAdd(interact_bb, id);
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
        g.LastItemData.DisplayRect = frame_bb;

        if (span_all_columns) {
            window->ClipRect.Min.x = backup_clip_rect_min_x;
            window->ClipRect.Max.x = backup_clip_rect_max_x;
        }

        // If a NavLeft request is happening and ImGuiTreeNodeFlags_NavLeftJumpsBackHere enabled:
        // Store data for the current depth to allow returning to this node from any child item.
        // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between
        // TreeNode() and TreePop(). It will become tempting to enable
        // ImGuiTreeNodeFlags_NavLeftJumpsBackHere by default or move it to ImGuiStyle. Currently
        // only supports 32 level deep and we are fine with (1 << Depth) overflowing into a zero,
        // easy to increase.
        if (is_open && !g.NavIdIsAlive && (flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) &&
            !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            if (g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window &&
                ImGui::NavMoveRequestButNoResultYet()) {
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
                ImGui::TreePushOverrideID(id);
            IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label,
                                        g.LastItemData.StatusFlags |
                                            (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                            (is_open ? ImGuiItemStatusFlags_Opened : 0));
            return is_open;
        }

        if (span_all_columns)
            ImGui::TablePushBackgroundChannel();

        const float eye_hit_x1 = eye_pos.x - style.TouchExtraPadding.x;
        const float eye_hit_x2 = eye_pos.x + (eye_size.x) + style.TouchExtraPadding.x;
        const bool is_mouse_x_over_eye =
            (g.IO.MousePos.x >= eye_hit_x1 && g.IO.MousePos.x < eye_hit_x2);

        ImGuiButtonFlags eye_button_flags = ImGuiTreeNodeFlags_None;
        if ((flags & ImGuiTreeNodeFlags_AllowOverlap) ||
            (g.LastItemData.InFlags & ImGuiItemFlags_AllowOverlap))
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
            (g.LastItemData.InFlags & ImGuiItemFlags_AllowOverlap))
            button_flags |= ImGuiButtonFlags_AllowOverlap;
        if (!is_leaf)
            button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

        // We allow clicking on the arrow section with keyboard modifiers held, in order to easily
        // allow browsing a tree while preserving selection with code implementing multi-selection
        // patterns. When clicking on the rest of the tree node we always disallow keyboard
        // modifiers.
        const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
        const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) +
                                   style.TouchExtraPadding.x;
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
        // _OpenOnArrow=0) It is rather standard that arrow click react on Down rather than Up. We
        // set ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item
        // to be active on the initial MouseDown in order for drag and drop to work.
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
        bool pressed = ImGui::ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
        bool toggled = false;
        if (!is_leaf) {
            if (pressed && g.DragDropHoldJustPressedId != id) {
                if ((flags & (ImGuiTreeNodeFlags_OpenOnArrow |
                              ImGuiTreeNodeFlags_OpenOnDoubleClick)) == 0 ||
                    (g.NavActivateId == id))
                    toggled = true;
                if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
                    toggled |=
                        is_mouse_x_over_arrow &&
                        !g.NavDisableMouseHover;  // Lightweight equivalent of IsMouseHoveringRect()
                                                  // since ButtonBehavior() already did the job
                if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) &&
                    g.IO.MouseClickedCount[0] == 2)
                    toggled = true;
            } else if (pressed && g.DragDropHoldJustPressedId == id) {
                IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
                if (!is_open)  // When using Drag and Drop "hold to open" we keep the node
                               // highlighted after opening, but never close it again.
                    toggled = true;
            }

            if (g.NavId == id && g.NavMoveDir == ImGuiDir_Left && is_open) {
                toggled = true;
                ImGui::NavClearPreferredPosForAxis(ImGuiAxis_X);
                ImGui::NavMoveRequestCancel();
            }
            if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right &&
                !is_open)  // If there's something upcoming on the line we may want to give it the
                           // priority?
            {
                toggled = true;
                ImGui::NavClearPreferredPosForAxis(ImGuiAxis_X);
                ImGui::NavMoveRequestCancel();
            }

            if (toggled) {
                is_open = !is_open;
                window->DC.StateStorage->SetInt(id, is_open);
                g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
            }
        }

        // In this branch, TreeNodeBehavior() cannot toggle the selection so this will never
        // trigger.
        if (selected != was_selected)  //-V547
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

        // Render
        const ImU32 text_col                       = ImGui::GetColorU32(ImGuiCol_Text);
        ImGuiNavHighlightFlags nav_highlight_flags = ImGuiNavHighlightFlags_TypeThin;
        if (display_frame) {
            // Framed type
            const ImU32 bg_col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                                                    : hovered         ? ImGuiCol_HeaderHovered
                                                                      : ImGuiCol_Header);
            ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
            ImGui::RenderNavHighlight(frame_bb, id, nav_highlight_flags);

            if (flags & ImGuiTreeNodeFlags_Bullet)
                ImGui::RenderBullet(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f),
                    text_col);
            else if (!is_leaf)
                ImGui::RenderArrow(
                    window->DrawList, ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y),
                    text_col,
                    is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up
                                                                            : ImGuiDir_Down)
                            : ImGuiDir_Right,
                    1.0f);
            else  // Leaf without bullet, left-adjusted text
                text_pos.x -= text_offset_x - padding.x;
            if (flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
                frame_bb.Max.x -= g.FontSize + style.FramePadding.x;

            if (g.LogEnabled)
                ImGui::LogSetNextTextDecoration("###", "###");
        } else {
            // Unframed typed for tree nodes
            if (hovered || selected || focused) {
                const ImU32 bg_col =
                    ImGui::GetColorU32((held && hovered)    ? ImGuiCol_HeaderActive
                                       : hovered || focused ? ImGuiCol_HeaderHovered
                                                            : ImGuiCol_Header);
                ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
            }
            ImGui::RenderNavHighlight(frame_bb, id, nav_highlight_flags);

            if (flags & ImGuiTreeNodeFlags_Bullet)
                ImGui::RenderBullet(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f),
                    text_col);
            else if (!is_leaf)
                ImGui::RenderArrow(
                    window->DrawList,
                    ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f),
                    text_col,
                    is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up
                                                                            : ImGuiDir_Down)
                            : ImGuiDir_Right,
                    0.70f);
            if (g.LogEnabled)
                ImGui::LogSetNextTextDecoration(">", NULL);
        }

        if (span_all_columns)
            ImGui::TablePopBackgroundChannel();

        // Label
        if (display_frame)
            ImGui::RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
        else
            ImGui::RenderText(text_pos, label, label_end, false);

        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0, 0, 0, 0});

        ImGui::RenderText(eye_pos, *visible ? ICON_FK_EYE : ICON_FK_EYE_SLASH, nullptr, false);

        ImGui::PopStyleColor(3);

        if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            ImGui::TreePushOverrideID(id);
        IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                    g.LastItemData.StatusFlags |
                                        (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) |
                                        (is_open ? ImGuiItemStatusFlags_Opened : 0));

        return is_open;
    }

    bool SceneTreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, bool focused, bool *visible) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        return SceneTreeNodeBehavior(window->GetID(label), flags, label, NULL, focused, visible);
    }

}  // namespace Toolbox::UI