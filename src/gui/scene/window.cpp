#if defined(_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include <cmath>

#include "gui/logging/errors.hpp"
#include "gui/logging/logger.hpp"
#include "gui/scene/window.hpp"

#include "gui/input.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include "gui/imgui_ext.hpp"
#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include "gui/application.hpp"
#include "gui/util.hpp"
#include <gui/IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iconv.h>

#if WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <gui/context_menu.hpp>

#include <J3D/Material/J3DMaterialTableLoader.hpp>
#include <J3D/Material/J3DUniformBufferObject.hpp>

using namespace Toolbox::Scene;

namespace Toolbox::UI {

    void PacketSort(J3DRendering::SortFunctionArgs packets) {
        std::sort(packets.begin(), packets.end(),
                  [](const J3DRenderPacket &a, const J3DRenderPacket &b) -> bool {
                      if ((a.SortKey & 0x01000000) != (b.SortKey & 0x01000000)) {
                          return (a.SortKey & 0x01000000) > (b.SortKey & 0x01000000);
                      } else {
                          return a.Material->Name < b.Material->Name;
                      }
                  });
    }

    /* icu

    includes:

    #include <unicode/ucnv.h>
    #include <unicode/unistr.h>
    #include <unicode/ustring.h>

    std::string Utf8ToSjis(const std::string& value)
    {
        icu::UnicodeString src(value.c_str(), "utf8");
        int length = src.extract(0, src.length(), NULL, "shift_jis");

        std::vector<char> result(length + 1);
        src.extract(0, src.length(), &result[0], "shift_jis");

        return std::string(result.begin(), result.end() - 1);
    }

    std::string SjisToUtf8(const std::string& value)
    {
        icu::UnicodeString src(value.c_str(), "shift_jis");
        int length = src.extract(0, src.length(), NULL, "utf8");

        std::vector<char> result(length + 1);
        src.extract(0, src.length(), &result[0], "utf8");

        return std::string(result.begin(), result.end() - 1);
    }
    */

    static std::string getNodeUID(std::shared_ptr<Toolbox::Object::ISceneObject> node) {
        std::string node_name = std::format("{} ({})##{}", node->type(), node->getNameRef().name(),
                                            node->getQualifiedName().toString());
        return node_name;
    }

    bool SceneTreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label,
                               const char *label_end, bool focused, bool *visible) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const f32 row_y = ImGui::GetCursorPosY();

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

    SceneWindow::~SceneWindow() {
        // J3DRendering::Cleanup();
        m_renderables.erase(m_renderables.begin(), m_renderables.end());
        m_resource_cache.m_model.clear();
        m_resource_cache.m_material.clear();
    }

    SceneWindow::SceneWindow() : DockWindow() {
        m_properties_render_handler = renderEmptyProperties;

        buildContextMenuVirtualObj();
        buildContextMenuGroupObj();
        buildContextMenuPhysicalObj();
        buildContextMenuMultiObj();

        buildCreateObjDialog();
        buildRenameObjDialog();

        buildContextMenuRail();
        buildContextMenuMultiRail();
        buildContextMenuRailNode();
        buildContextMenuMultiRailNode();

        buildCreateRailDialog();
        buildRenameRailDialog();
    }

    bool SceneWindow::loadData(const std::filesystem::path &path) {
        if (!Toolbox::exists(path)) {
            return false;
        }

        if (Toolbox::is_directory(path)) {
            if (path.filename() != "scene") {
                return false;
            }

            m_resource_cache.m_model.clear();
            m_resource_cache.m_material.clear();

            auto scene_result = SceneInstance::FromPath(path);
            if (!scene_result) {
                const SerialError &error = scene_result.error();
                for (auto &line : error.m_message) {
                    Log::AppLogger::instance().error(line);
                }
#ifndef NDEBUG
                for (auto &entry : error.m_stacktrace) {
                    Log::AppLogger::instance().debugLog(
                        std::format("{} at line {}", entry.source_file(), entry.source_line()));
                }
#endif
                return false;
            } else {
                m_current_scene = std::move(scene_result.value());
                if (m_current_scene != nullptr) {
                    m_renderer.initializeData(*m_current_scene);
                }
            }

            return true;
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool SceneWindow::update(f32 deltaTime) { return m_renderer.inputUpdate(); }

    void SceneWindow::renderBody(f32 deltaTime) {
        renderHierarchy();
        renderProperties();
        renderRailEditor();
        renderScene(deltaTime);
    }

    void SceneWindow::renderHierarchy() {
        ImGuiWindowClass hierarchyOverride;
        hierarchyOverride.ClassId =
            ImGui::GetID(getWindowChildUID(*this, "Hierarchy Editor").c_str());
        hierarchyOverride.ParentViewportId = m_window_id;
        hierarchyOverride.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingOverOther;
        ImGui::SetNextWindowClass(&hierarchyOverride);

        ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

        if (ImGui::Begin(getWindowChildUID(*this, "Hierarchy Editor").c_str())) {
            if (ImGui::IsWindowFocused()) {
                m_focused_window = EditorWindow::OBJECT_TREE;
            }

            ImGui::Text("Find Objects");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            m_hierarchy_filter.Draw("##obj_filter");

            ImGui::Text("Map Objects");
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) /* Check if scene is loaded here*/) {
                // Add Object
            }

            ImGui::Separator();

            // Render Objects

            if (m_current_scene != nullptr) {
                auto root = m_current_scene->getObjHierarchy().getRoot();
                renderTree(root);
            }

            ImGui::Spacing();
            ImGui::Text("Scene Info");
            ImGui::Separator();

            if (m_current_scene != nullptr) {
                auto root = m_current_scene->getTableHierarchy().getRoot();
                renderTree(root);
            }
        }
        ImGui::End();

        if (m_hierarchy_selected_nodes.size() > 0) {
            m_create_obj_dialog.render(m_hierarchy_selected_nodes.back());
            m_rename_obj_dialog.render(m_hierarchy_selected_nodes.back());
        }
    }

    void SceneWindow::renderTree(std::shared_ptr<Toolbox::Object::ISceneObject> node) {
        constexpr auto dir_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanFullWidth;

        constexpr auto file_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth;

        constexpr auto node_flags = dir_flags | ImGuiTreeNodeFlags_DefaultOpen;

        bool multi_select     = Input::GetKey(GLFW_KEY_LEFT_CONTROL);
        bool needs_scene_sync = node->getTransform() ? false : true;

        std::string display_name = std::format("{} ({})", node->type(), node->getNameRef().name());
        bool is_filtered_out     = !m_hierarchy_filter.PassFilter(display_name.c_str());

        std::string node_uid_str = getNodeUID(node);
        ImGuiID tree_node_id     = ImGui::GetCurrentWindow()->GetID(node_uid_str.c_str());

        auto node_it =
            std::find_if(m_hierarchy_selected_nodes.begin(), m_hierarchy_selected_nodes.end(),
                         [&](const SelectionNodeInfo<Object::ISceneObject> &other) {
                             return other.m_node_id == tree_node_id;
                         });
        bool node_already_clicked = node_it != m_hierarchy_selected_nodes.end();

        bool node_visible    = node->getIsPerforming();
        bool node_visibility = node->getCanPerform();

        bool node_open = false;

        SelectionNodeInfo<Object::ISceneObject> node_info = {
            .m_selected      = node,
            .m_node_id       = tree_node_id,
            .m_parent_synced = true,
            .m_scene_synced  = needs_scene_sync};  // Only spacial objects get scene selection

        if (node->isGroupObject()) {
            if (is_filtered_out) {
                auto objects = std::static_pointer_cast<Toolbox::Object::GroupSceneObject>(node)
                                   ->getChildren();
                if (objects.has_value()) {
                    for (auto object : objects.value()) {
                        renderTree(object);
                    }
                }
            } else {
                if (node_visibility) {
                    node_open = SceneTreeNodeEx(node_uid_str.c_str(),
                                                node->getParent() ? dir_flags : node_flags,
                                                node_already_clicked, &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_renderer.markDirty();
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(),
                                                  node->getParent() ? dir_flags : node_flags,
                                                  node_already_clicked);
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                    ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    ImGui::FocusWindow(ImGui::GetCurrentWindow());

                    m_selected_properties.clear();

                    if (multi_select) {
                        if (node_it == m_hierarchy_selected_nodes.end())
                            m_hierarchy_selected_nodes.push_back(node_info);
                    } else {
                        m_hierarchy_selected_nodes.clear();
                        m_hierarchy_selected_nodes.push_back(node_info);
                        for (auto &member : node->getMembers()) {
                            member->syncArray();
                            auto prop = createProperty(member);
                            if (prop) {
                                m_selected_properties.push_back(std::move(prop));
                            }
                        }
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderHierarchyContextMenu(node_uid_str, node_info);

                if (node_open) {
                    auto objects = std::static_pointer_cast<Toolbox::Object::GroupSceneObject>(node)
                                       ->getChildren();
                    if (objects.has_value()) {
                        for (auto object : objects.value()) {
                            renderTree(object);
                        }
                    }
                    ImGui::TreePop();
                }
            }
        } else {
            if (!is_filtered_out) {
                if (node_visibility) {
                    node_open = SceneTreeNodeEx(node_uid_str.c_str(), file_flags,
                                                node_already_clicked, &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_renderer.markDirty();
                    }
                } else {
                    node_open =
                        ImGui::TreeNodeEx(node_uid_str.c_str(), file_flags, node_already_clicked);
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                    ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    ImGui::FocusWindow(ImGui::GetCurrentWindow());

                    m_selected_properties.clear();

                    if (multi_select) {
                        if (node_it == m_hierarchy_selected_nodes.end())
                            m_hierarchy_selected_nodes.push_back(node_info);
                    } else {
                        m_hierarchy_selected_nodes.clear();
                        m_hierarchy_selected_nodes.push_back(node_info);
                        for (auto &member : node->getMembers()) {
                            member->syncArray();
                            auto prop = createProperty(member);
                            if (prop) {
                                m_selected_properties.push_back(std::move(prop));
                            }
                        }
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderHierarchyContextMenu(node_uid_str, node_info);

                if (node_open) {
                    ImGui::TreePop();
                }
            }
        }
    }

    void SceneWindow::renderProperties() {
        ImGuiWindowClass propertiesOverride;
        propertiesOverride.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingOverOther;
        ImGui::SetNextWindowClass(&propertiesOverride);

        ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

        if (ImGui::Begin(getWindowChildUID(*this, "Properties Editor").c_str())) {
            if (ImGui::IsWindowFocused()) {
                m_focused_window = EditorWindow::PROPERTY_EDITOR;
            }
            if (m_properties_render_handler(*this)) {
                m_renderer.markDirty();
            }
        }
        ImGui::End();
    }

    bool SceneWindow::renderObjectProperties(SceneWindow &window) {
        float label_width = 0;
        for (auto &prop : window.m_selected_properties) {
            label_width = std::max(label_width, prop->labelSize().x);
        }

        bool is_updated = false;
        for (auto &prop : window.m_selected_properties) {
            if (prop->render(label_width)) {
                is_updated = true;
            }
            ImGui::ItemSize({0, 2});
        }

        return is_updated;
    }

    bool SceneWindow::renderRailProperties(SceneWindow &window) { return false; }

    bool SceneWindow::renderRailNodeProperties(SceneWindow &window) {
        auto &logger = Log::AppLogger::instance();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        const float label_width = ImGui::CalcTextSize("ConnectionCount").x;

        auto &node = window.m_rail_node_list_selected_nodes[0].m_selected;
        auto *rail = node->rail();

        bool is_updated = false;

        /* Position */
        {
            ImGui::Text("Position");

            if (!collapse_lines) {
                ImGui::SameLine();
                ImGui::Dummy({label_width - ImGui::CalcTextSize("Position").x, 0});
                ImGui::SameLine();
            }

            std::string label = "##Position";

            s16 pos[3];
            node->getPosition(pos[0], pos[1], pos[2]);

            s16 step = 1, step_fast = 10;

            if (ImGui::InputScalarCompactN(
                    label.c_str(), ImGuiDataType_S16, pos, 3, &step, &step_fast, nullptr,
                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                auto result = rail->setNodePosition(node, pos[0], pos[1], pos[2]);
                if (!result) {
                    logMetaError(result.error());
                }
                is_updated = true;
            }
        }

        /* Flags */
        {
            ImGui::Text("Flags");

            if (!collapse_lines) {
                ImGui::SameLine();
                ImGui::Dummy({label_width - ImGui::CalcTextSize("Flags").x, 0});
                ImGui::SameLine();
            }

            std::string label = "##Flags";

            u32 step = 1, step_fast = 10;

            u32 flags = node->getFlags();
            if (ImGui::InputScalarCompact(
                    label.c_str(), ImGuiDataType_U32, &flags, &step, &step_fast, nullptr,
                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                rail->setNodeFlag(node, flags);
            }
        }

        u16 connection_count = node->getConnectionCount();
        {
            ImGui::Text("ConnectionCount");

            if (!collapse_lines) {
                ImGui::SameLine();
                ImGui::Dummy({label_width - ImGui::CalcTextSize("ConnectionCount").x, 0});
                ImGui::SameLine();
            }

            std::string label = "##ConnectionCount";

            u16 step = 1, step_fast = 10;

            u16 connection_count_old = connection_count;
            if (ImGui::InputScalarCompact(
                    label.c_str(), ImGuiDataType_U16, &connection_count, &step, &step_fast, nullptr,
                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                connection_count = std::clamp<u16>(connection_count, 1, 8);
                if (connection_count < connection_count_old) {
                    for (u16 j = connection_count; j < connection_count_old; ++j) {
                        auto result = rail->removeConnection(node, j);
                        if (!result) {
                            logMetaError(result.error());
                            connection_count = j;
                            break;
                        }
                    }
                } else if (connection_count > connection_count_old) {
                    for (u16 j = connection_count_old; j < connection_count; ++j) {
                        auto result = rail->addConnection(node, rail->nodes()[0]);
                        if (!result) {
                            logMetaError(result.error());
                            connection_count = j;
                            break;
                        }
                    }
                }
                is_updated = true;
            }
        }

        if (ImGui::BeginGroupPanel("Connections", &window.m_connections_open, {})) {
            for (u16 i = 0; i < connection_count; ++i) {
                ImGui::Text("%i", i);

                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy({label_width - ImGui::CalcTextSize("Flags").x, 0});
                    ImGui::SameLine();
                }

                std::string label = std::format("##Connection-{}", i);

                s16 step = 1, step_fast = 10;

                s16 connection_value   = 0;
                auto connection_result = node->getConnectionValue(i);
                if (!connection_result) {
                    logMetaError(connection_result.error());
                } else {
                    connection_value = connection_result.value();
                }

                s16 connection_value_old = connection_value;
                if (ImGui::InputScalarCompact(label.c_str(), ImGuiDataType_S16, &connection_value,
                                              &step, &step_fast, nullptr,
                                              ImGuiInputTextFlags_CharsDecimal |
                                                  ImGuiInputTextFlags_CharsNoBlank)) {
                    connection_value = std::clamp<s16>(connection_value, 0,
                                                       static_cast<s16>(rail->nodes().size()) - 1);
                    if (connection_value != connection_value_old) {
                        auto result =
                            rail->replaceConnection(node, i, rail->nodes()[connection_value]);
                        if (!result) {
                            logMetaError(result.error());
                        }
                        is_updated = true;
                    }
                }
            }
        }
        ImGui::EndGroupPanel();

        if (is_updated) {
            window.m_renderer.updatePaths(window.m_current_scene->getRailData());
        }

        return is_updated;
    }

    void SceneWindow::renderRailEditor() {
        const ImGuiTreeNodeFlags rail_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                              ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                              ImGuiTreeNodeFlags_SpanAvailWidth;
        const ImGuiTreeNodeFlags node_flags = rail_flags | ImGuiTreeNodeFlags_Leaf;
        ImGuiWindowClass hierarchyOverride;
        hierarchyOverride.ClassId = ImGui::GetID(getWindowChildUID(*this, "Rail Editor").c_str());
        hierarchyOverride.ParentViewportId = m_window_id;
        hierarchyOverride.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingOverOther;
        ImGui::SetNextWindowClass(&hierarchyOverride);

        ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

        if (ImGui::Begin(getWindowChildUID(*this, "Rail Editor").c_str())) {
            bool multi_select = Input::GetKey(GLFW_KEY_LEFT_CONTROL);

            if (ImGui::IsWindowFocused()) {
                m_focused_window = EditorWindow::RAIL_TREE;
            }

            for (auto &rail : m_current_scene->getRailData()) {
                SelectionNodeInfo<Rail::Rail> rail_info = {.m_selected = rail,
                                                           .m_node_id =
                                                               ImGui::GetID(rail->name().data()),
                                                           .m_parent_synced = true,
                                                           .m_scene_synced  = false};

                bool is_rail_selected = std::any_of(
                    m_rail_list_selected_nodes.begin(), m_rail_list_selected_nodes.end(),
                    [&](auto &info) { return info.m_node_id == rail_info.m_node_id; });

                bool is_rail_open =
                    ImGui::TreeNodeEx(rail->name().data(), rail_flags, is_rail_selected);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                    ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    ImGui::FocusWindow(ImGui::GetCurrentWindow());
                    if (!multi_select) {
                        m_rail_list_selected_nodes.clear();
                        m_rail_node_list_selected_nodes.clear();
                    }
                    m_rail_list_selected_nodes.push_back(rail_info);

                    m_properties_render_handler = renderRailProperties;
                }

                renderRailContextMenu(rail->name(), rail_info);

                if (is_rail_open) {
                    for (size_t i = 0; i < rail->nodes().size(); ++i) {
                        auto &node            = rail->nodes()[i];
                        std::string node_name = std::format("Node {}", i);
                        std::string qual_name = std::string(rail->name()) + "##" + node_name;

                        SelectionNodeInfo<Rail::RailNode> node_info = {
                            .m_selected      = node,
                            .m_node_id       = ImGui::GetID(qual_name.c_str()),
                            .m_parent_synced = true,
                            .m_scene_synced  = false};

                        bool is_node_selected =
                            std::any_of(m_rail_node_list_selected_nodes.begin(),
                                        m_rail_node_list_selected_nodes.end(), [&](auto &info) {
                                            return info.m_node_id == node_info.m_node_id;
                                        });

                        bool is_node_open =
                            ImGui::TreeNodeEx(node_name.c_str(), node_flags, is_node_selected);

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                            ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                            ImGui::FocusWindow(ImGui::GetCurrentWindow());
                            if (!multi_select) {
                                m_rail_list_selected_nodes.clear();
                                m_rail_node_list_selected_nodes.clear();
                            }
                            m_rail_node_list_selected_nodes.push_back(node_info);

                            m_properties_render_handler = renderRailNodeProperties;
                        }

                        renderRailNodeContextMenu(node_name, node_info);

                        if (is_node_open) {
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
            /*}
            ImGui::EndChild();*/
        }
        ImGui::End();

        if (m_rail_list_selected_nodes.size() > 0) {
            m_create_rail_dialog.render(m_rail_list_selected_nodes.back());
            m_rename_rail_dialog.render(m_rail_list_selected_nodes.back());
        }
    }

    void SceneWindow::renderScene(f32 delta_time) {
        m_renderables.clear();

        std::vector<J3DLight> lights;

        // perhaps find a way to limit this so it only happens when we need to re-render?
        if (m_current_scene != nullptr) {
            auto perform_result = m_current_scene->getObjHierarchy().getRoot()->performScene(
                delta_time, m_renderables, m_resource_cache, lights);
            if (!perform_result) {
                const ObjectError &error = perform_result.error();
                logObjectError(error);
            }
        }

        m_is_render_window_open = ImGui::Begin(getWindowChildUID(*this, "Scene View").c_str());
        if (m_is_render_window_open) {
            if (ImGui::IsWindowFocused()) {
                m_focused_window = EditorWindow::RENDER_VIEW;
            }
            m_renderer.render(m_renderables, delta_time);
        }
        ImGui::End();
    }

    void SceneWindow::renderHierarchyContextMenu(std::string str_id,
                                                 SelectionNodeInfo<Object::ISceneObject> &info) {
        if (m_hierarchy_selected_nodes.size() > 0) {
            SelectionNodeInfo<Object::ISceneObject> &info = m_hierarchy_selected_nodes.back();
            if (m_hierarchy_selected_nodes.size() > 1) {
                m_hierarchy_multi_node_menu.render({}, m_hierarchy_selected_nodes);
            } else if (info.m_selected->isGroupObject()) {
                m_hierarchy_group_node_menu.render(str_id, info);
            } else if (info.m_selected->hasMember("Transform")) {
                m_hierarchy_physical_node_menu.render(str_id, info);
            } else {
                m_hierarchy_virtual_node_menu.render(str_id, info);
            }
        }
    }

    void SceneWindow::renderRailContextMenu(std::string str_id,
                                            SelectionNodeInfo<Rail::Rail> &info) {
        if (m_rail_list_selected_nodes.size() > 0) {
            SelectionNodeInfo<Rail::Rail> &info = m_rail_list_selected_nodes.back();
            if (m_rail_list_selected_nodes.size() > 1) {
                m_rail_list_multi_node_menu.render({}, m_rail_list_selected_nodes);
            } else {
                m_rail_list_single_node_menu.render(str_id, info);
            }
        }
    }

    void SceneWindow::renderRailNodeContextMenu(std::string str_id,
                                                SelectionNodeInfo<Rail::RailNode> &info) {
        if (m_rail_node_list_selected_nodes.size() > 0) {
            SelectionNodeInfo<Rail::RailNode> &info = m_rail_node_list_selected_nodes.back();
            if (m_rail_node_list_selected_nodes.size() > 1) {
                m_rail_node_list_multi_node_menu.render({}, m_rail_node_list_selected_nodes);
            } else {
                m_rail_node_list_single_node_menu.render(str_id, info);
            }
        }
    }

    void SceneWindow::buildContextMenuVirtualObj() {
        m_hierarchy_virtual_node_menu = ContextMenu<SelectionNodeInfo<Object::ISceneObject>>();

        m_hierarchy_virtual_node_menu.addOption(
            "Insert Object Here...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_I},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_create_obj_dialog.open();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_virtual_node_menu.addDivider();

        m_hierarchy_virtual_node_menu.addOption(
            "Rename...",
            {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_rename_obj_dialog.open();
                m_rename_obj_dialog.setOriginalName(info.m_selected->getNameRef().name());
                return std::expected<void, BaseError>();
            });

        m_hierarchy_virtual_node_menu.addDivider();

        m_hierarchy_virtual_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                MainApplication::instance().getSceneObjectClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_hierarchy_virtual_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto nodes = MainApplication::instance().getSceneObjectClipboard().getData();
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to get parent node for pasting");
                }
                for (auto &node : nodes) {
                    this_parent->addChild(node.m_selected);
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_virtual_node_menu.addDivider();

        m_hierarchy_virtual_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to get parent node for pasting");
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());
                auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                         m_hierarchy_selected_nodes.end(), info);
                m_hierarchy_selected_nodes.erase(node_it);
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildContextMenuGroupObj() {
        m_hierarchy_group_node_menu = ContextMenu<SelectionNodeInfo<Object::ISceneObject>>();

        m_hierarchy_group_node_menu.addOption("Add Child Object...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
                                              [this](SelectionNodeInfo<Object::ISceneObject> info) {
                                                  m_create_obj_dialog.open();
                                                  return std::expected<void, BaseError>();
                                              });

        m_hierarchy_group_node_menu.addDivider();

        m_hierarchy_group_node_menu.addOption("Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R},
                                              [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_rename_obj_dialog.open();
                m_rename_obj_dialog.setOriginalName(info.m_selected->getNameRef().name());
                return std::expected<void, BaseError>();
            });

        m_hierarchy_group_node_menu.addDivider();

        m_hierarchy_group_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                MainApplication::instance().getSceneObjectClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_hierarchy_group_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto nodes       = MainApplication::instance().getSceneObjectClipboard().getData();
                auto this_parent = std::reinterpret_pointer_cast<GroupSceneObject>(info.m_selected);
                if (!this_parent) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to get parent node for pasting");
                }
                for (auto &node : nodes) {
                    this_parent->addChild(node.m_selected);
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_group_node_menu.addDivider();

        m_hierarchy_group_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to get parent node for pasting");
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());
                auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                         m_hierarchy_selected_nodes.end(), info);
                m_hierarchy_selected_nodes.erase(node_it);
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildContextMenuPhysicalObj() {
        m_hierarchy_physical_node_menu = ContextMenu<SelectionNodeInfo<Object::ISceneObject>>();

        m_hierarchy_physical_node_menu.addOption(
            "Insert Object Here...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_create_obj_dialog.open();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_rename_obj_dialog.open();
                m_rename_obj_dialog.setOriginalName(info.m_selected->getNameRef().name());
                return std::expected<void, BaseError>();
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "View in Scene", {ImGuiMod_Alt, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto member_result = info.m_selected->getMember("Transform");
                if (!member_result) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to find transform member of physical object");
                }
                auto member_ptr = member_result.value();
                if (!member_ptr) {
                    return make_error<void>("Scene Hierarchy",
                                            "Found the transform member but it was null");
                }
                Transform transform = getMetaValue<Transform>(member_ptr).value();
                f32 max_scale       = std::max(transform.m_scale.x, transform.m_scale.y);
                max_scale           = std::max(max_scale, transform.m_scale.z);

                m_renderer.setCameraOrientation({0, 1, 0}, transform.m_translation,
                                                {transform.m_translation.x,
                                                 transform.m_translation.y,
                                                 transform.m_translation.z + 1000 * max_scale});
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_physical_node_menu.addOption(
            "Move to Camera", {ImGuiMod_Alt, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto member_result = info.m_selected->getMember("Transform");
                if (!member_result) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to find transform member of physical object");
                }
                auto member_ptr = member_result.value();
                if (!member_ptr) {
                    return make_error<void>("Scene Hierarchy",
                                            "Found the transform member but it was null");
                }

                Transform transform = getMetaValue<Transform>(member_ptr).value();
                m_renderer.getCameraTranslation(transform.m_translation);
                setMetaValue<Transform>(member_ptr, 0, transform);

                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                MainApplication::instance().getSceneObjectClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_hierarchy_physical_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto nodes = MainApplication::instance().getSceneObjectClipboard().getData();
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to get parent node for pasting");
                }
                for (auto &node : nodes) {
                    this_parent->addChild(node.m_selected);
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    return make_error<void>("Scene Hierarchy",
                                            "Failed to get parent node for pasting");
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());
                auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                         m_hierarchy_selected_nodes.end(), info);
                m_hierarchy_selected_nodes.erase(node_it);
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildContextMenuMultiObj() {
        m_hierarchy_multi_node_menu =
            ContextMenu<std::vector<SelectionNodeInfo<Object::ISceneObject>>>();

        m_hierarchy_multi_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
                for (auto &info : infos) {
                    info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                }
                MainApplication::instance().getSceneObjectClipboard().setData(infos);
                return std::expected<void, BaseError>();
            });

        m_hierarchy_multi_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
                auto nodes = MainApplication::instance().getSceneObjectClipboard().getData();
                for (auto &info : infos) {
                    auto this_parent =
                        reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                    if (!this_parent) {
                        return make_error<void>("Scene Hierarchy",
                                                "Failed to get parent node for pasting");
                    }
                    for (auto &node : nodes) {
                        this_parent->addChild(node.m_selected);
                    }
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_hierarchy_multi_node_menu.addDivider();

        m_hierarchy_multi_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
                for (auto &info : infos) {
                    auto this_parent =
                        reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                    if (!this_parent) {
                        return make_error<void>("Scene Hierarchy",
                                                "Failed to get parent node for pasting");
                    }
                    this_parent->removeChild(info.m_selected->getNameRef().name());
                    auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                             m_hierarchy_selected_nodes.end(), info);
                    m_hierarchy_selected_nodes.erase(node_it);
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildContextMenuRail() {
        m_rail_list_single_node_menu = ContextMenu<SelectionNodeInfo<Rail::Rail>>();

        m_rail_list_single_node_menu.addOption("Insert Rail Here...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
                                               [this](SelectionNodeInfo<Rail::Rail> info) {
                                                   m_create_rail_dialog.open();
                                                   return std::expected<void, BaseError>();
                                               });

        m_rail_list_single_node_menu.addDivider();

        m_rail_list_single_node_menu.addOption(
            "Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R}, [this](SelectionNodeInfo<Rail::Rail> info) {
                m_rename_rail_dialog.open();
                m_rename_rail_dialog.setOriginalName(info.m_selected->name());
                return std::expected<void, BaseError>();
            });

        // TODO: Add rail manipulators here.

        m_rail_list_single_node_menu.addDivider();

        m_rail_list_single_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C}, [this](SelectionNodeInfo<Rail::Rail> info) {
            info.m_selected = make_deep_clone<Rail::Rail>(info.m_selected);
            MainApplication::instance().getSceneRailClipboard().setData(info);
            return std::expected<void, BaseError>();
        });

        m_rail_list_single_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V}, [this](SelectionNodeInfo<Rail::Rail> info) {
            auto nodes = MainApplication::instance().getSceneRailClipboard().getData();
            if (nodes.size() > 0) {
                RailData data         = m_current_scene->getRailData();
                size_t selected_index = data.getRailCount();
                auto result           = data.getRailIndex(info.m_selected->name());
                if (result) {
                    selected_index = result.value();
                }
                for (auto &node : nodes) {
                    data.insertRail(selected_index + 1, node.m_selected);
                    selected_index += 1;
                }
                m_current_scene->setRailData(data);
            }
            m_renderer.markDirty();
            return std::expected<void, BaseError>();
        });

        m_rail_list_single_node_menu.addOption("Delete", {GLFW_KEY_DELETE},
                                               [this](SelectionNodeInfo<Rail::Rail> info) {
                                                   RailData data = m_current_scene->getRailData();
                                                   data.removeRail(info.m_selected->name());
                                                   m_current_scene->setRailData(data);
                                                   m_renderer.markDirty();
                                                   return std::expected<void, BaseError>();
                                               });
    }

    void SceneWindow::buildContextMenuMultiRail() {
        m_rail_list_multi_node_menu = ContextMenu<std::vector<SelectionNodeInfo<Rail::Rail>>>();

        m_rail_list_multi_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
                for (auto &select : info) {
                    select.m_selected = make_deep_clone<Rail::Rail>(select.m_selected);
                }
                MainApplication::instance().getSceneRailClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_rail_list_multi_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
                auto nodes = MainApplication::instance().getSceneRailClipboard().getData();
                if (nodes.size() > 0) {
                    RailData data         = m_current_scene->getRailData();
                    size_t selected_index = data.getRailCount();
                    auto result           = data.getRailIndex(info[0].m_selected->name());
                    if (result) {
                        selected_index = result.value();
                    }
                    for (auto &node : nodes) {
                        data.insertRail(selected_index + 1, node.m_selected);
                        selected_index += 1;
                    }
                    m_current_scene->setRailData(data);
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_rail_list_multi_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
                RailData data = m_current_scene->getRailData();
                for (auto &select : info) {
                    data.removeRail(select.m_selected->name());
                }
                m_current_scene->setRailData(data);
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildContextMenuRailNode() {
        m_rail_node_list_single_node_menu = ContextMenu<SelectionNodeInfo<Rail::RailNode>>();

        m_rail_node_list_single_node_menu.addOption("Insert Node Here...",
                                                    {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
                                                    [this](SelectionNodeInfo<Rail::RailNode> info) {
                                                        m_create_rail_dialog.open();
                                                        return std::expected<void, BaseError>();
                                                    });

        m_rail_node_list_single_node_menu.addDivider();

        m_rail_node_list_single_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C}, [this](SelectionNodeInfo<Rail::RailNode> info) {
                info.m_selected = make_deep_clone<Rail::RailNode>(info.m_selected);
                MainApplication::instance().getSceneRailNodeClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_single_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V}, [this](SelectionNodeInfo<Rail::RailNode> info) {
                Rail::Rail *rail      = info.m_selected->rail();
                size_t selected_index = rail->getNodeCount();
                auto result           = rail->getNodeIndex(info.m_selected);
                if (result) {
                    selected_index = result.value();
                }
                auto nodes = MainApplication::instance().getSceneRailNodeClipboard().getData();
                for (auto &node : nodes) {
                    rail->insertNode(selected_index + 1, node.m_selected);
                    selected_index += 1;
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_single_node_menu.addOption("Delete", {GLFW_KEY_DELETE},
                                                    [this](SelectionNodeInfo<Rail::RailNode> info) {
                                                        Rail::Rail *rail = info.m_selected->rail();
                                                        rail->removeNode(info.m_selected);
                                                        m_renderer.markDirty();
                                                        return std::expected<void, BaseError>();
                                                    });
    }

    void SceneWindow::buildContextMenuMultiRailNode() {
        m_rail_node_list_multi_node_menu =
            ContextMenu<std::vector<SelectionNodeInfo<Rail::RailNode>>>();

        m_rail_node_list_multi_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
                for (auto &select : info) {
                    select.m_selected = make_deep_clone<Rail::RailNode>(select.m_selected);
                }
                MainApplication::instance().getSceneRailNodeClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_multi_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
                Rail::Rail *rail      = info[0].m_selected->rail();
                size_t selected_index = rail->getNodeCount();
                auto result           = rail->getNodeIndex(info[0].m_selected);
                if (result) {
                    selected_index = result.value();
                }
                auto nodes = MainApplication::instance().getSceneRailNodeClipboard().getData();
                for (auto &node : nodes) {
                    rail->insertNode(selected_index + 1, node.m_selected);
                    selected_index += 1;
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_multi_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
                for (auto &select : info) {
                    Rail::Rail *rail = select.m_selected->rail();
                    rail->removeNode(select.m_selected);
                }
                m_renderer.markDirty();
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildCreateObjDialog() {
        m_create_obj_dialog.setup();
        m_create_obj_dialog.setActionOnAccept(
            [this](std::string_view name, const Object::Template &template_,
                   std::string_view wizard_name, SelectionNodeInfo<Object::ISceneObject> info) {
                auto new_object_result = Object::ObjectFactory::create(template_, wizard_name);
                if (!name.empty()) {
                    new_object_result->setNameRef(name);
                }

                ISceneObject *this_parent;
                if (info.m_selected->isGroupObject()) {
                    this_parent = info.m_selected.get();
                } else {
                    this_parent = info.m_selected->getParent();
                }

                if (!this_parent) {
                    auto result = make_error<void>("Scene Hierarchy",
                                                   "Failed to get parent node for obj creation");
                    logError(result.error());
                    return;
                }

                auto result = this_parent->addChild(std::move(new_object_result));
                if (!result) {
                    logObjectGroupError(result.error());
                    return;
                }

                m_renderer.markDirty();
                return;
            });
        m_create_obj_dialog.setActionOnReject([](SelectionNodeInfo<Object::ISceneObject>) {});
    }

    void SceneWindow::buildRenameObjDialog() {
        m_rename_obj_dialog.setup();
        m_rename_obj_dialog.setActionOnAccept([this](std::string_view new_name,
                                                     SelectionNodeInfo<Object::ISceneObject> info) {
            if (new_name.empty()) {
                auto result = make_error<void>("Rail Data", "Can not rename rail to empty string");
                logError(result.error());
                return;
            }
            info.m_selected->setNameRef(new_name);
        });
        m_rename_obj_dialog.setActionOnReject([](SelectionNodeInfo<Object::ISceneObject>) {});
    }

    void SceneWindow::buildCreateRailDialog() {
        m_create_rail_dialog.setup();
        m_create_rail_dialog.setActionOnAccept([this](std::string_view name, u16 node_count,
                                                      s16 node_distance, bool loop) {
            if (name.empty()) {
                auto result = make_error<void>("Rail Data", "Can not name rail as empty string");
                logError(result.error());
                return;
            }

            RailData rail_data = m_current_scene->getRailData();

            f64 angle      = 0;
            f64 angle_step = node_count == 0 ? 0 : (M_PI * 2) / node_count;

            std::vector<Rail::Rail::node_ptr_t> new_nodes;
            for (u16 i = 0; i < node_count; ++i) {
                s16 x = 0;
                s16 y = 0;
                s16 z = 0;
                if (loop) {
                    x = static_cast<s16>(std::cos(angle) * node_distance);
                    y = 0;
                    z = static_cast<s16>(std::sin(angle) * node_distance);
                } else {
                    x = 0;
                    y = 0;
                    z = i * node_distance;
                }

                auto node = std::make_shared<Rail::RailNode>(x, y, z, 0);
                new_nodes.push_back(node);

                angle += angle_step;
            }

            auto new_rail = std::make_shared<Rail::Rail>(name, new_nodes);

            for (u16 i = 0; i < node_count; ++i) {
                auto result = new_rail->connectNodeToNeighbors(i, true);
                if (!result) {
                    logMetaError(result.error());
                }
            }

            if (!loop) {
                // First
                {
                    auto result = new_rail->removeConnection(0, 1);
                    if (!result) {
                        logMetaError(result.error());
                    }
                }

                // Last
                {
                    auto result = new_rail->removeConnection(node_count - 1, 1);
                    if (!result) {
                        logMetaError(result.error());
                    }
                }
            }

            rail_data.addRail(new_rail);

            m_current_scene->setRailData(rail_data);
            m_renderer.updatePaths(rail_data);
            m_renderer.markDirty();
        });
    }

    void SceneWindow::buildRenameRailDialog() {
        m_rename_rail_dialog.setup();
        m_rename_rail_dialog.setActionOnAccept(
            [this](std::string_view new_name, SelectionNodeInfo<Rail::Rail> info) {
                if (new_name.empty()) {
                    auto result =
                        make_error<void>("Scene Hierarchy", "Can not rename rail to empty string");
                    logError(result.error());
                    return;
                }
                info.m_selected->setName(new_name);
            });
        m_rename_rail_dialog.setActionOnReject([](SelectionNodeInfo<Rail::Rail>) {});
    }

    void SceneWindow::onDeleteKey() {
        switch (m_focused_window) {
        case EditorWindow::OBJECT_TREE: {
            break;
        }
        case EditorWindow::PROPERTY_EDITOR: {
            break;
        }
        case EditorWindow::RAIL_TREE: { 
            break;
        }
        case EditorWindow::RENDER_VIEW: {
            break;
        }
        case EditorWindow::NONE:
        default:
            break;
        }
    }

    void SceneWindow::onPageDownKey() {}

    void SceneWindow::onPageUpKey() {}

    void SceneWindow::onHomeKey() {}

    void SceneWindow::buildDockspace(ImGuiID dockspace_id) {
        m_dock_node_left_id =
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        m_dock_node_up_left_id   = ImGui::DockBuilderSplitNode(m_dock_node_left_id, ImGuiDir_Down,
                                                               0.5f, nullptr, &m_dock_node_left_id);
        m_dock_node_down_left_id = ImGui::DockBuilderSplitNode(
            m_dock_node_up_left_id, ImGuiDir_Down, 0.5f, nullptr, &m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Scene View").c_str(), dockspace_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Hierarchy Editor").c_str(),
                                     m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Rail Editor").c_str(),
                                     m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Properties Editor").c_str(),
                                     m_dock_node_down_left_id);
    }

    void SceneWindow::renderMenuBar() {}

};  // namespace Toolbox::UI