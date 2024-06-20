#if defined(_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cmath>
#include <execution>
#include <vector>

#include "core/input/input.hpp"
#include "core/log.hpp"
#include "core/timing.hpp"

#include "IconsForkAwesome.h"
#include "gui/application.hpp"
#include "gui/font.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/logging/errors.hpp"
#include "gui/modelcache.hpp"
#include "gui/scene/window.hpp"
#include "gui/settings.hpp"
#include "gui/util.hpp"

#include "gui/imgui_ext.hpp"

#include "platform/capture.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iconv.h>

#if WIN32
#include <windows.h>
#endif
#include <gui/context_menu.hpp>

#include <J3D/Material/J3DMaterialTableLoader.hpp>
#include <J3D/Material/J3DUniformBufferObject.hpp>

#include "gui/scene/window.hpp"
#include <glm/gtx/euler_angles.hpp>

using namespace Toolbox;

namespace Toolbox::UI {

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

    static std::unordered_set<std::string> s_game_blacklist = {"Map", "Sky"};

    static std::string getNodeUID(RefPtr<Toolbox::Object::ISceneObject> node) {
        std::string node_name =
            std::format("{} ({}) [{:08X}]###{}", node->type(), node->getNameRef().name(),
                        node->getGamePtr(), node->getUUID());
        return node_name;
    }

    static std::string getNodeUID(RefPtr<Rail::Rail> rail) {
        return std::format("{}###{}", rail->name(), rail->getUUID());
    }

    static std::string getNodeUID(RefPtr<Rail::Rail> rail, size_t node_index) {
        if (node_index >= rail->nodes().size()) {
            return std::format("(orphan)###{}", node_index);
        }
        return std::format("Node {}###{}", node_index, rail->nodes()[node_index]->getUUID());
    }

    SceneCreateRailEvent::SceneCreateRailEvent(const UUID64 &target_id, const Rail::Rail &rail)
        : BaseEvent(target_id, SCENE_CREATE_RAIL_EVENT), m_rail(rail) {}

    ScopePtr<ISmartResource> SceneCreateRailEvent::clone(bool deep) const {
        return make_scoped<SceneCreateRailEvent>(*this);
    }

    SceneWindow::SceneWindow(const std::string &name) : ImWindow(name) {}

    bool SceneWindow::onLoadData(const std::filesystem::path &path) {
        if (!Toolbox::Filesystem::exists(path)) {
            return false;
        }

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();

        if (Toolbox::Filesystem::is_directory(path)) {
            if (path.filename() != "scene") {
                return false;
            }

            bool result = true;

            SceneInstance::FromPath(path)
                .and_then([&](ScopePtr<SceneInstance> &&scene) {
                    m_current_scene = std::move(scene);
                    m_renderer.initializeData(*m_current_scene);

                    m_resource_cache.m_model.clear();
                    m_resource_cache.m_material.clear();

                    // Initialize the rail visibility map
                    for (RefPtr<Rail::Rail> rail : m_current_scene->getRailData().rails()) {
                        m_rail_visible_map[rail->getUUID()] = true;
                    }

                    if (task_communicator.isSceneLoaded(1, 2)) {
                        reassignAllActorPtrs(0);
                    }

                    return Result<void, SerialError>();
                })
                .or_else([&result](const SerialError &error) {
                    LogError(error);
                    result = false;
                    return Result<void, SerialError>();
                });

            return result;
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool SceneWindow::onSaveData(std::optional<std::filesystem::path> path) {
        std::filesystem::path root_path;

        if (!path) {
            auto opt_path = m_current_scene->rootPath();
            if (!opt_path) {
                TOOLBOX_ERROR("(SCENE) Failed to save the scene due to lack of a root path.");
                return false;
            }
            root_path = opt_path.value();
        } else {
            root_path = path.value();
        }

        auto result = m_current_scene->saveToPath(root_path);
        if (!result) {
            LogError(result.error());
            return false;
        }

        return true;
    }

    void SceneWindow::onAttach() {
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

    void SceneWindow::onDetach() {
        if (unsaved()) {
            TOOLBOX_WARN_V("[SCENE_WINDOW] Scene closed with unsaved changes ({}).", context());
        }

        m_hierarchy_filter.Clear();
        m_hierarchy_selected_nodes.clear();
        m_selected_properties.clear();
        m_properties_render_handler = renderEmptyProperties;

        m_renderables.clear();
        m_resource_cache = {};

        m_rail_visible_map.clear();
        m_rail_list_selected_nodes.clear();
        m_rail_node_list_selected_nodes.clear();

        m_drop_target_buffer.free();
        m_current_scene.reset();
    }

    void SceneWindow::onImGuiUpdate(TimeStep delta_time) {
        m_renderer.inputUpdate(delta_time);

        calcDolphinVPMatrix();

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();

        if (task_communicator.isSceneLoaded()) {
            if (Input::GetKeyDown(KeyCode::KEY_E)) {
                m_is_game_edit_mode ^= true;
            }
        } else {
            m_is_game_edit_mode = false;
        }

        std::vector<RefPtr<Rail::RailNode>> rendered_nodes;
        for (auto &rail : m_current_scene->getRailData().rails()) {
            if (!m_rail_visible_map[rail->getUUID()])
                continue;
            rendered_nodes.insert(rendered_nodes.end(), rail->nodes().begin(), rail->nodes().end());
        }

        if (ImGuizmo::IsOver()) {
            return;
        }

        bool should_reset = false;
        Renderer::selection_variant_t selection =
            m_renderer.findSelection(m_renderables, rendered_nodes, should_reset);

        bool multi_select = Input::GetKey(KeyCode::KEY_LEFTCONTROL);

        if (should_reset && !multi_select) {
            m_hierarchy_selected_nodes.clear();
            m_rail_node_list_selected_nodes.clear();
            m_selected_properties.clear();
            m_properties_render_handler = renderEmptyProperties;
        }

        if (std::holds_alternative<RefPtr<ISceneObject>>(selection)) {
            RefPtr<ISceneObject> obj = std::get<RefPtr<ISceneObject>>(selection);
            processObjectSelection(obj, multi_select);
        } else if (std::holds_alternative<RefPtr<Rail::RailNode>>(selection)) {
            RefPtr<Rail::RailNode> node = std::get<RefPtr<Rail::RailNode>>(selection);

            // In this circumstance, select the whole rail
            bool rail_selection_mode = Input::GetKey(KeyCode::KEY_LEFTALT);
            if (rail_selection_mode) {
                RailData &rail_data = m_current_scene->getRailData();

                RefPtr<Rail::Rail> rail = rail_data.getRail(node->getRailUUID());
                if (!rail) {
                    TOOLBOX_ERROR("Failed to find rail for node.");
                    return;
                }

                processRailSelection(rail, multi_select);
            } else {
                processRailNodeSelection(node, multi_select);
            }
        }

        if (m_hierarchy_selected_nodes.empty() && m_rail_list_selected_nodes.empty() &&
            m_rail_node_list_selected_nodes.empty()) {
            m_renderer.setGizmoVisible(false);
        } else {
            m_renderer.setGizmoVisible(true);
        }

        return;
    }

    void SceneWindow::onImGuiPostUpdate(TimeStep delta_time) {
        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();

        if (m_renderer.isGizmoManipulated() && m_hierarchy_selected_nodes.size() > 0) {
            glm::mat4x4 gizmo_transform = m_renderer.getGizmoTransform();
            Transform obj_transform;

            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(gizmo_transform), glm::value_ptr(obj_transform.m_translation),
                glm::value_ptr(obj_transform.m_rotation), glm::value_ptr(obj_transform.m_scale));

            // RefPtr<ISceneObject> obj = m_hierarchy_selected_nodes[0].m_selected;
            // BoundingBox obj_old_bb            = obj->getBoundingBox().value();
            // Transform obj_old_transform       = obj->getTransform().value();

            //// Since the translation is for the gizmo, offset it back to the actual transform
            // obj_transform.m_translation = obj_old_transform.m_translation + (translation -
            // obj_old_bb.m_center);

            RefPtr<PhysicalSceneObject> object =
                ref_cast<PhysicalSceneObject>(m_hierarchy_selected_nodes[0].m_selected);
            m_hierarchy_selected_nodes[0].m_selected->setTransform(obj_transform);
        }

        if (m_is_game_edit_mode) {
            for (auto &renderable : m_renderables) {
                if (s_game_blacklist.contains(renderable.m_object->type())) {
                    continue;
                }
                task_communicator.setObjectTransform(
                    ref_cast<PhysicalSceneObject>(renderable.m_object), renderable.m_transform);
            }
        }

        if (m_object_drop_target != -1) {
            loadMimeObject(m_drop_target_buffer, m_object_drop_target, m_object_parent_uuid);
            m_object_drop_target = -1;
            m_object_parent_uuid = 0;
        }

        if (m_rail_drop_target != -1) {
            loadMimeRail(m_drop_target_buffer, m_rail_drop_target);
            m_rail_drop_target = -1;
        }

        if (m_rail_node_drop_target != -1) {
            loadMimeRailNode(m_drop_target_buffer, m_rail_node_drop_target, m_rail_node_rail_uuid);
            m_rail_node_drop_target = -1;
            m_rail_node_rail_uuid   = 0;
        }

        m_update_render_objs = true;
    }

    void SceneWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    static std::unordered_set<std::string> s_scene_mime_formats = {
        "toolbox/scene/object", "toolbox/scene/rail", "toolbox/scene/railnode"};

    void SceneWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void SceneWindow::onDropEvent(RefPtr<DropEvent> ev) {}

    void SceneWindow::onEvent(RefPtr<BaseEvent> ev) {
        ImWindow::onEvent(ev);
        if (!isTargetOfEvent(ev)) {
            return;
        }
        switch (ev->getType()) {
        case SCENE_CREATE_RAIL_EVENT: {
            auto event = std::static_pointer_cast<SceneCreateRailEvent>(ev);
            if (event) {
                const Rail::Rail &rail = event->getRail();
                m_current_scene->getRailData().addRail(rail);
                m_rail_visible_map[rail.getUUID()] = true;
                ev->accept();
            }
            break;
        }
        case SCENE_DISABLE_CONTROL_EVENT: {
            m_control_disable_requested = true;
            break;
        }
        case SCENE_ENABLE_CONTROL_EVENT: {
            m_control_disable_requested = false;
            break;
        }
        default:
            break;
        }
    }
}  // namespace Toolbox::UI

void SceneWindow::onRenderBody(TimeStep deltaTime) {
    renderHierarchy();
    renderProperties();
    renderRailEditor();
    renderScene(deltaTime);
    renderDolphin(deltaTime);
}

void SceneWindow::renderHierarchy() {
    std::string hierarchy_window_name = ImWindowComponentTitle(*this, "Hierarchy Editor");

    ImGuiWindowClass hierarchyOverride;
    hierarchyOverride.ClassId               = static_cast<ImGuiID>(getUUID());
    hierarchyOverride.ParentViewportId      = ImGui::GetCurrentWindow()->ViewportId;
    hierarchyOverride.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&hierarchyOverride);

    ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

    if (ImGui::Begin(hierarchy_window_name.c_str())) {
        m_hierarchy_window = ImGui::GetCurrentWindow();

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
            renderTree(0, root);
        }

        ImGui::Spacing();
        ImGui::Text("Scene Info");
        ImGui::Separator();

        if (m_current_scene != nullptr) {
            auto root = m_current_scene->getTableHierarchy().getRoot();
            renderTree(0, root);
        }
    }
    ImGui::End();

    if (m_hierarchy_selected_nodes.size() > 0) {
        m_create_obj_dialog.render(m_hierarchy_selected_nodes.back());
        m_rename_obj_dialog.render(m_hierarchy_selected_nodes.back());
    }
}

void SceneWindow::renderTree(size_t node_index, RefPtr<Toolbox::Object::ISceneObject> node) {
    constexpr auto dir_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanFullWidth;

    constexpr auto file_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth;

    constexpr auto node_flags = dir_flags | ImGuiTreeNodeFlags_DefaultOpen;

    bool multi_select     = Input::GetKey(KeyCode::KEY_LEFTCONTROL);
    bool needs_scene_sync = node->getTransform() ? false : true;

    std::string display_name = std::format("{} ({})", node->type(), node->getNameRef().name());
    bool is_filtered_out     = !m_hierarchy_filter.PassFilter(display_name.c_str());

    std::string node_uid_str = getNodeUID(node);
    ImGuiID tree_node_id     = static_cast<ImGuiID>(node->getUUID());

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
            for (size_t i = 0; i < node->getChildren().size(); ++i) {
                renderTree(i, node->getChildren()[i]);
            }
        } else {
            if (node_visibility) {
                node_open = ImGui::TreeNodeEx(node_uid_str.c_str(),
                                              node->getParent() ? dir_flags : node_flags,
                                              node_already_clicked, &node_visible);
                if (node->getIsPerforming() != node_visible) {
                    node->setIsPerforming(node_visible);
                    m_update_render_objs = true;
                }
            } else {
                node_open = ImGui::TreeNodeEx(node_uid_str.c_str(),
                                              node->getParent() ? dir_flags : node_flags,
                                              node_already_clicked);
            }

            // Drag and drop for OBJECT
            {
                ImVec2 mouse_pos = ImGui::GetMousePos();
                ImVec2 item_size = ImGui::GetItemRectSize();
                ImVec2 item_pos  = ImGui::GetItemRectMin();

                if (ImGui::BeginDragDropSource()) {
                    Toolbox::Buffer buffer;
                    saveMimeObject(buffer, node_index, get_shared_ptr(*node->getParent()));
                    ImGui::SetDragDropPayload("toolbox/scene/object", buffer.buf(), buffer.size(),
                                              ImGuiCond_Once);
                    ImGui::Text("Object: %s", node->getNameRef().name().data());
                    ImGui::EndDragDropSource();
                }

                ImGuiDropFlags drop_flags = ImGuiDropFlags_None;
                if (mouse_pos.y < item_pos.y + (item_size.y / 4)) {
                    drop_flags = ImGuiDropFlags_InsertBefore;
                } else if (mouse_pos.y > item_pos.y + 3 * (item_size.y / 4)) {
                    drop_flags = ImGuiDropFlags_InsertAfter;
                } else {
                    drop_flags = ImGuiDropFlags_InsertChild;
                }

                if (node->getParent() == nullptr) {
                    drop_flags = ImGuiDropFlags_InsertChild;
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                            "toolbox/scene/object",
                            ImGuiDragDropFlags_AcceptBeforeDelivery |
                                ImGuiDragDropFlags_AcceptNoDrawDefaultRect |
                                ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {

                        ImGui::RenderDragDropTargetRect(
                            ImGui::GetCurrentContext()->DragDropTargetRect,
                            ImGui::GetCurrentContext()->DragDropTargetClipRect, drop_flags);

                        if (payload->IsDelivery()) {
                            Toolbox::Buffer buffer;
                            buffer.setBuf(payload->Data, payload->DataSize);
                            buffer.copyTo(m_drop_target_buffer);

                            // Calculate index based on position relative to center
                            switch (drop_flags) {
                            case ImGuiDropFlags_InsertBefore:
                                m_object_drop_target = node_index;
                                m_object_parent_uuid = node->getParent()->getUUID();
                                break;
                            case ImGuiDropFlags_InsertAfter:
                                m_object_drop_target = node_index + 1;
                                m_object_parent_uuid = node->getParent()->getUUID();
                                break;
                            case ImGuiDropFlags_InsertChild:
                                m_object_drop_target = node->getChildren().size();
                                m_object_parent_uuid = node->getUUID();
                                break;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
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
                for (size_t i = 0; i < node->getChildren().size(); ++i) {
                    renderTree(i, node->getChildren()[i]);
                }
                ImGui::TreePop();
            }
        }
    } else {
        if (!is_filtered_out) {
            if (node_visibility) {
                node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), file_flags,
                                              node_already_clicked, &node_visible);
                if (node->getIsPerforming() != node_visible) {
                    node->setIsPerforming(node_visible);
                    m_update_render_objs = true;
                }
            } else {
                node_open =
                    ImGui::TreeNodeEx(node_uid_str.c_str(), file_flags, node_already_clicked);
            }

            // Drag and drop for OBJECT
            {
                ImVec2 mouse_pos = ImGui::GetMousePos();
                ImVec2 item_size = ImGui::GetItemRectSize();
                ImVec2 item_pos  = ImGui::GetItemRectMin();

                if (ImGui::BeginDragDropSource()) {
                    Toolbox::Buffer buffer;
                    saveMimeObject(buffer, node_index, get_shared_ptr(*node->getParent()));
                    ImGui::SetDragDropPayload("toolbox/scene/object", buffer.buf(), buffer.size(),
                                              ImGuiCond_Once);
                    ImGui::Text("Object: %s", node->getNameRef().name().data());
                    ImGui::EndDragDropSource();
                }

                ImGuiDropFlags drop_flags = ImGuiDropFlags_None;
                if (mouse_pos.y < item_pos.y + (item_size.y / 2)) {
                    drop_flags = ImGuiDropFlags_InsertBefore;
                } else {
                    drop_flags = ImGuiDropFlags_InsertAfter;
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                            "toolbox/scene/object",
                            ImGuiDragDropFlags_AcceptBeforeDelivery |
                                ImGuiDragDropFlags_AcceptNoDrawDefaultRect |
                                ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {

                        ImGui::RenderDragDropTargetRect(
                            ImGui::GetCurrentContext()->DragDropTargetRect,
                            ImGui::GetCurrentContext()->DragDropTargetClipRect, drop_flags);

                        if (payload->IsDelivery()) {
                            Toolbox::Buffer buffer;
                            buffer.setBuf(payload->Data, payload->DataSize);
                            buffer.copyTo(m_drop_target_buffer);

                            // Calculate index based on position relative to center
                            m_object_drop_target = node_index;
                            if (drop_flags == ImGuiDropFlags_InsertAfter) {
                                m_object_drop_target++;
                            }
                            m_object_parent_uuid = node->getParent()->getUUID();
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
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
    std::string properties_editor_str = ImWindowComponentTitle(*this, "Properties Editor");

    ImGuiWindowClass propertiesOverride;
    propertiesOverride.ClassId                  = static_cast<ImGuiID>(getUUID());
    propertiesOverride.ParentViewportId         = ImGui::GetCurrentWindow()->ViewportId;
    propertiesOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
    propertiesOverride.DockingAllowUnclassed    = false;
    ImGui::SetNextWindowClass(&propertiesOverride);

    ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

    if (ImGui::Begin(properties_editor_str.c_str())) {
        if (ImGui::IsWindowFocused()) {
            m_focused_window = EditorWindow::PROPERTY_EDITOR;
        }
        if (m_properties_render_handler(*this)) {
            m_update_render_objs = true;
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
    ImVec2 window_size        = ImGui::GetWindowSize();
    const bool collapse_lines = window_size.x < 350;

    const float label_width = ImGui::CalcTextSize("ConnectionCount").x;

    RefPtr<Rail::RailNode> node = window.m_rail_node_list_selected_nodes[0].m_selected;
    RefPtr<Rail::Rail> rail = window.m_current_scene->getRailData().getRail(node->getRailUUID());

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
                LogError(result.error());
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
                        LogError(result.error());
                        connection_count = j;
                        break;
                    }
                }
            } else if (connection_count > connection_count_old) {
                for (u16 j = connection_count_old; j < connection_count; ++j) {
                    auto result = rail->addConnection(node, rail->nodes()[0]);
                    if (!result) {
                        LogError(result.error());
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
                LogError(connection_result.error());
            } else {
                connection_value = connection_result.value();
            }

            s16 connection_value_old = connection_value;
            if (ImGui::InputScalarCompact(
                    label.c_str(), ImGuiDataType_S16, &connection_value, &step, &step_fast, nullptr,
                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                connection_value = std::clamp<s16>(connection_value, 0,
                                                   static_cast<s16>(rail->nodes().size()) - 1);
                if (connection_value != connection_value_old) {
                    auto result = rail->replaceConnection(node, i, rail->nodes()[connection_value]);
                    if (!result) {
                        LogError(result.error());
                    }
                    is_updated = true;
                }
            }
        }
    }
    ImGui::EndGroupPanel();

    if (is_updated) {
        window.m_renderer.updatePaths(window.m_current_scene->getRailData(),
                                      window.m_rail_visible_map);
    }

    return is_updated;
}

static void recursiveFlattenActorTree(RefPtr<ISceneObject> actor,
                                      std::vector<RefPtr<ISceneObject>> &out) {
    out.push_back(actor);
    for (auto &child : actor->getChildren()) {
        recursiveFlattenActorTree(child, out);
    }
}

static void recursiveAssignActorPtrs(Game::TaskCommunicator &communicator,
                                     std::vector<RefPtr<ISceneObject>> objects) {
    std::for_each(std::execution::par, objects.begin(), objects.end(),
                  [&communicator](RefPtr<ISceneObject> object) {
                      u32 actor_ptr = communicator.getActorPtr(object);
                      object->setGamePtr(actor_ptr);
                      if (actor_ptr == 0) {
                          TOOLBOX_WARN_V("[Scene] Failed to find ptr for object \"{}\"",
                                         object->getNameRef().name());
                      } else {
                          TOOLBOX_INFO_V("[Scene] Found ptr for object \"{}\" at 0x{:08X}",
                                         object->getNameRef().name(), actor_ptr);
                      }
                  });
}

void SceneWindow::calcDolphinVPMatrix() {
    m_dolphin_vp_mtx = glm::identity<glm::mat4x4>();

    DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
    if (!communicator.manager().isHooked()) {
        return;
    }

    u32 camera_ptr = communicator.read<u32>(0x8040D0A8).value();
    if (!camera_ptr) {
        return;
    }

    u32 proj_mtx_ptr     = camera_ptr + 0x16C;
    u32 view_mtx_ptr     = camera_ptr + 0x1EC;
    glm::mat4x4 proj_mtx = glm::identity<glm::mat4x4>();
    glm::mat4x4 view_mtx = glm::identity<glm::mat4x4>();

    f32 fovy   = communicator.read<f32>(camera_ptr + 0x48).value();
    f32 aspect = communicator.read<f32>(camera_ptr + 0x4C).value();
    proj_mtx   = glm::perspectiveRH_NO(
        glm::radians(fovy), aspect > FLT_EPSILON ? aspect : (4.0f / 3.0f), 1.0f, 100000.0f);

    /*glm::vec3 camera_pos = {
        communicator.read<f32>(camera_ptr + 0x10).value(),
        communicator.read<f32>(camera_ptr + 0x14).value(),
        communicator.read<f32>(camera_ptr + 0x18).value(),
    };

    glm::vec3 camera_up = {
        communicator.read<f32>(camera_ptr + 0x30).value(),
        communicator.read<f32>(camera_ptr + 0x34).value(),
        communicator.read<f32>(camera_ptr + 0x38).value(),
    };

    glm::vec3 camera_target = {
        communicator.read<f32>(camera_ptr + 0x3C).value(),
        communicator.read<f32>(camera_ptr + 0x40).value(),
        communicator.read<f32>(camera_ptr + 0x44).value(),
    };

    view_mtx = glm::lookAtRH(camera_pos, camera_target, camera_up);*/

    view_mtx[0][0] = communicator.read<f32>(view_mtx_ptr + 0x00).value();
    view_mtx[1][0] = communicator.read<f32>(view_mtx_ptr + 0x04).value();
    view_mtx[2][0] = communicator.read<f32>(view_mtx_ptr + 0x08).value();
    view_mtx[0][1] = communicator.read<f32>(view_mtx_ptr + 0x10).value();
    view_mtx[1][1] = communicator.read<f32>(view_mtx_ptr + 0x14).value();
    view_mtx[2][1] = communicator.read<f32>(view_mtx_ptr + 0x18).value();
    view_mtx[0][2] = -communicator.read<f32>(view_mtx_ptr + 0x20).value();
    view_mtx[1][2] = -communicator.read<f32>(view_mtx_ptr + 0x24).value();
    view_mtx[2][2] = -communicator.read<f32>(view_mtx_ptr + 0x28).value();
    view_mtx[3][0] = communicator.read<f32>(view_mtx_ptr + 0x0C).value();
    view_mtx[3][1] = communicator.read<f32>(view_mtx_ptr + 0x1C).value();
    view_mtx[3][2] = -communicator.read<f32>(view_mtx_ptr + 0x2C).value();

    m_dolphin_vp_mtx = proj_mtx * view_mtx;
}

void SceneWindow::reassignAllActorPtrs(u32 param) {
    Game::TaskCommunicator &task_communicator = GUIApplication::instance().getTaskCommunicator();
    RefPtr<ISceneObject> root                 = m_current_scene->getObjHierarchy().getRoot();
    std::vector<RefPtr<ISceneObject>> objects;
    recursiveFlattenActorTree(root, objects);
    double timing = Timing::measure(recursiveAssignActorPtrs, task_communicator, objects);
    TOOLBOX_INFO_V("[SCENE] Acquired all actor ptrs in {} seconds", timing);
}

void SceneWindow::renderRailEditor() {
    const std::string rail_editor_str = ImWindowComponentTitle(*this, "Rail Editor");

    const ImGuiTreeNodeFlags rail_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                          ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                          ImGuiTreeNodeFlags_SpanAvailWidth;
    const ImGuiTreeNodeFlags node_flags = rail_flags | ImGuiTreeNodeFlags_Leaf;
    ImGuiWindowClass hierarchyOverride;
    hierarchyOverride.ClassId          = static_cast<ImGuiID>(getUUID());
    hierarchyOverride.ParentViewportId = ImGui::GetCurrentWindow()->ViewportId;
    /*hierarchyOverride.DockNodeFlagsOverrideSet =
        ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingOverOther;*/
    hierarchyOverride.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&hierarchyOverride);

    ImGui::SetNextWindowSizeConstraints({300, 500}, {FLT_MAX, FLT_MAX});

    if (ImGui::Begin(rail_editor_str.c_str())) {
        bool multi_select = Input::GetKey(KeyCode::KEY_LEFTCONTROL);

        if (ImGui::IsWindowFocused()) {
            m_focused_window = EditorWindow::RAIL_TREE;
        }

        const RailData &rail_data = m_current_scene->getRailData();
        for (size_t i = 0; i < rail_data.rails().size(); ++i) {
            RefPtr<Rail::Rail> rail = rail_data.rails()[i];

            std::string uid_str = getNodeUID(rail);
            ImGuiID rail_id     = static_cast<ImGuiID>(rail->getUUID());

            bool is_rail_selected =
                std::any_of(m_rail_list_selected_nodes.begin(), m_rail_list_selected_nodes.end(),
                            [&](auto &info) { return info.m_node_id == rail_id; });

            SelectionNodeInfo<Rail::Rail> rail_info = {.m_selected      = rail,
                                                       .m_node_id       = rail_id,
                                                       .m_parent_synced = true,
                                                       .m_scene_synced  = false};

            auto node_it =
                std::find_if(m_rail_list_selected_nodes.begin(), m_rail_list_selected_nodes.end(),
                             [&](const SelectionNodeInfo<Rail::Rail> &other) {
                                 return other.m_node_id == rail_id;
                             });

            if (!m_rail_visible_map.contains(rail_id)) {
                m_rail_visible_map[rail_id] = true;
            }
            bool &rail_visibility = m_rail_visible_map[rail_id];
            bool is_rail_visible  = rail_visibility;

            bool is_rail_open =
                ImGui::TreeNodeEx(uid_str.data(), rail_flags, is_rail_selected, &is_rail_visible);

            // Drag and drop for RAIL
            {
                ImVec2 mouse_pos = ImGui::GetMousePos();
                ImVec2 item_size = ImGui::GetItemRectSize();
                ImVec2 item_pos  = ImGui::GetItemRectMin();

                if (ImGui::BeginDragDropSource()) {
                    Toolbox::Buffer buffer;
                    saveMimeRail(buffer, i);
                    ImGui::SetDragDropPayload("toolbox/scene/rail", buffer.buf(), buffer.size(),
                                              ImGuiCond_Once);
                    ImGui::Text("Rail: %s", rail->name().c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                            "toolbox/scene/rail", ImGuiDragDropFlags_AcceptBeforeDelivery |
                                                      ImGuiDragDropFlags_SourceNoHoldToOpenOthers);
                        payload && payload->IsDelivery()) {
                        Toolbox::Buffer buffer;
                        buffer.setBuf(payload->Data, payload->DataSize);
                        buffer.copyTo(m_drop_target_buffer);

                        // Calculate index based on position relative to center
                        m_rail_drop_target = i;
                        if (mouse_pos.y > item_pos.y + (item_size.y / 2)) {
                            m_rail_drop_target++;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            if (rail_visibility != is_rail_visible) {
                rail_visibility = is_rail_visible;
                m_renderer.updatePaths(m_current_scene->getRailData(), m_rail_visible_map);
                m_update_render_objs = true;
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                ImGui::FocusWindow(ImGui::GetCurrentWindow());

                if (multi_select) {
                    m_selected_properties.clear();
                    if (!is_rail_selected)
                        m_rail_list_selected_nodes.push_back(rail_info);
                } else {
                    m_rail_list_selected_nodes.clear();
                    m_rail_list_selected_nodes.push_back(rail_info);
                }

                // Since a rail is selected, we should clear the rail nodes
                m_rail_node_list_selected_nodes.clear();

                m_properties_render_handler = renderRailProperties;
            }

            renderRailContextMenu(rail->name(), rail_info);

            if (is_rail_open) {
                for (size_t j = 0; j < rail->nodes().size(); ++j) {
                    RefPtr<Rail::RailNode> node = rail->nodes()[j];
                    std::string node_uid_str    = getNodeUID(rail, j);
                    ImGuiID node_id             = static_cast<ImGuiID>(node->getUUID());

                    bool is_rail_node_selected =
                        std::any_of(m_rail_node_list_selected_nodes.begin(),
                                    m_rail_node_list_selected_nodes.end(),
                                    [&](auto &info) { return info.m_node_id == node_id; });

                    SelectionNodeInfo<Rail::RailNode> node_info = {.m_selected      = node,
                                                                   .m_node_id       = node_id,
                                                                   .m_parent_synced = true,
                                                                   .m_scene_synced  = false};

                    bool is_node_open =
                        ImGui::TreeNodeEx(node_uid_str.c_str(), node_flags, is_rail_node_selected);

                    // Drag and drop for RAIL NODE
                    {
                        ImVec2 mouse_pos = ImGui::GetMousePos();
                        ImVec2 item_size = ImGui::GetItemRectSize();
                        ImVec2 item_pos  = ImGui::GetItemRectMin();

                        if (ImGui::BeginDragDropSource()) {
                            int drag_from_index = j;

                            Toolbox::Buffer buffer;
                            saveMimeRailNode(buffer, j, rail);
                            ImGui::SetDragDropPayload("toolbox/scene/railnode", buffer.buf(),
                                                      buffer.size(), ImGuiCond_Once);

                            /*ImVec2 mouse_pos = ImGui::GetMousePos();

                            DragAction action(getUUID());
                            action.setHotSpot(mouse_pos);
                            action.setPayload(ImGui::GetDragDropPayload());

                            GUIApplication::instance().dispatchEvent<DragEvent, false>(
                                EVENT_DRAG_MOVE, mouse_pos.x, mouse_pos.y, std::move(action));*/

                            ImGui::Text("Node %d (%s)", j, rail->name().c_str());

                            ImGui::EndDragDropSource();
                        }

                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                                    "toolbox/scene/railnode",
                                    ImGuiDragDropFlags_AcceptBeforeDelivery);
                                payload && payload->IsDelivery()) {
                                Toolbox::Buffer buffer;
                                buffer.setBuf(payload->Data, payload->DataSize);
                                buffer.copyTo(m_drop_target_buffer);
                                m_rail_node_rail_uuid = node->getRailUUID();

                                // Calculate index based on position relative to center
                                m_rail_node_drop_target = j;
                                if (mouse_pos.y > item_pos.y + (item_size.y / 2)) {
                                    m_rail_node_drop_target++;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                        ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        ImGui::FocusWindow(ImGui::GetCurrentWindow());

                        if (multi_select) {
                            m_selected_properties.clear();
                            if (!is_rail_node_selected)
                                m_rail_node_list_selected_nodes.push_back(node_info);
                        } else {
                            m_rail_node_list_selected_nodes.clear();
                            m_rail_node_list_selected_nodes.push_back(node_info);
                        }

                        // Since a rail node is selected, we should clear the rails
                        m_rail_list_selected_nodes.clear();

                        m_properties_render_handler = renderRailNodeProperties;
                    }

                    renderRailNodeContextMenu(node_uid_str, node_info);

                    if (is_node_open) {
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();

    if (m_rail_list_selected_nodes.size() > 0) {
        m_create_rail_dialog.render(m_rail_list_selected_nodes.back());
        m_rename_rail_dialog.render(m_rail_list_selected_nodes.back());
    }
}

void SceneWindow::renderScene(TimeStep delta_time) {
    const AppSettings &settings = SettingsManager::instance().getCurrentProfile();

    std::vector<J3DLight> lights;

    // perhaps find a way to limit this so it only happens when we need to re-render?
    if (m_current_scene != nullptr) {
        if (m_update_render_objs || !settings.m_is_rendering_simple) {
            m_renderables.clear();
            auto perform_result = m_current_scene->getObjHierarchy().getRoot()->performScene(
                delta_time, !settings.m_is_rendering_simple, m_renderables, m_resource_cache,
                lights);
            if (!perform_result) {
                const ObjectError &error = perform_result.error();
                LogError(error);
            }
            m_update_render_objs = false;
            m_renderer.markDirty();
        }
    }

    std::string scene_view_str = ImWindowComponentTitle(*this, "Scene View");

    ImGuiWindowClass sceneViewOverride;
    sceneViewOverride.ClassId                  = static_cast<ImGuiID>(getUUID());
    sceneViewOverride.ParentViewportId         = ImGui::GetCurrentWindow()->ViewportId;
    sceneViewOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
    sceneViewOverride.DockingAllowUnclassed    = false;
    ImGui::SetNextWindowClass(&sceneViewOverride);

    m_is_render_window_open = ImGui::Begin(scene_view_str.c_str());
    if (m_is_render_window_open) {
        if (ImGui::IsWindowFocused()) {
            m_focused_window = EditorWindow::RENDER_VIEW;
        }

        m_renderer.render(m_renderables, delta_time);

        renderScenePeripherals(delta_time);
        renderPlaybackButtons(delta_time);
    }
    ImGui::End();
}

void SceneWindow::renderPlaybackButtons(TimeStep delta_time) {
    Game::TaskCommunicator &task_communicator = GUIApplication::instance().getTaskCommunicator();

    float window_bar_height = ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
    ImGui::SetCursorPos({0, window_bar_height + 10});

    const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
    const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

    const ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 cmd_button_size   = ImGui::CalcTextSize(ICON_FK_UNDO) + frame_padding;
    cmd_button_size.x        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.5f;
    cmd_button_size.y        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.f;

    bool is_dolphin_running = DolphinHookManager::instance().isProcessRunning();
    if (is_dolphin_running) {
        ImGui::BeginDisabled();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.35f, 0.1f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.7f, 0.2f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.7f, 0.2f, 1.0f});

    ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2);
    if (ImGui::AlignedButton(ICON_FK_PLAY, cmd_button_size)) {
        DolphinHookManager::instance().startProcess();
        task_communicator.taskLoadScene(1, 2, TOOLBOX_BIND_EVENT_FN(reassignAllActorPtrs));
    }

    ImGui::PopStyleColor(3);

    if (is_dolphin_running) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, {0.35f, 0.1f, 0.1f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.7f, 0.2f, 0.2f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.7f, 0.2f, 0.2f, 1.0f});

    bool context_controls_disabled = !is_dolphin_running || m_control_disable_requested;

    if (context_controls_disabled) {
        ImGui::BeginDisabled();
    }

    ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 + cmd_button_size.x);
    if (ImGui::AlignedButton(ICON_FK_STOP, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                             ImDrawFlags_RoundCornersBottomRight)) {
        DolphinHookManager::instance().stopProcess();
    }

    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.2f, 0.4f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.4f, 0.8f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.4f, 0.8f, 1.0f});

    ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 - cmd_button_size.x);
    if (ImGui::AlignedButton(ICON_FK_UNDO, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                             ImDrawFlags_RoundCornersBottomLeft)) {
        task_communicator.taskLoadScene(1, 2, TOOLBOX_BIND_EVENT_FN(reassignAllActorPtrs));
    }

    ImGui::PopStyleColor(3);

    if (context_controls_disabled) {
        ImGui::EndDisabled();
    }
}

void SceneWindow::renderScenePeripherals(TimeStep delta_time) {
    float window_bar_height = ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
    ImGui::SetCursorPos({0, window_bar_height + 10});

    const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
    const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

    ImGui::SetCursorPosX(window_padding.x);
    if (ImGui::Button(m_is_game_edit_mode ? "Game: Edit Mode" : "Game: View Mode")) {
        m_is_game_edit_mode ^= true;
    }
}

void SceneWindow::renderDolphin(TimeStep delta_time) {
    Game::TaskCommunicator &task_communicator = GUIApplication::instance().getTaskCommunicator();

    std::string dolphin_view_str = ImWindowComponentTitle(*this, "Dolphin View");

    ImGuiWindowClass dolphinViewOverride;
    dolphinViewOverride.ClassId                  = static_cast<ImGuiID>(getUUID());
    dolphinViewOverride.ParentViewportId         = ImGui::GetCurrentWindow()->ViewportId;
    dolphinViewOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
    dolphinViewOverride.DockingAllowUnclassed    = false;
    ImGui::SetNextWindowClass(&dolphinViewOverride);

    if (ImGui::Begin(dolphin_view_str.c_str())) {
        ImVec2 window_pos = ImGui::GetWindowPos();

        ImVec2 render_size = {
            ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2,
            ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y * 2 -
                (ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight())};

        ImVec2 cursor_pos = ImGui::GetCursorPos() + ImGui::GetStyle().FramePadding;
        // ImGui::PushClipRect(window_pos + cursor_pos, window_pos + cursor_pos + render_size,
        // true);

        // Render the Dolphin view and overlays
        {
            DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();

            m_dolphin_image = std::move(
                task_communicator.captureXFBAsTexture(static_cast<int>(ImGui::GetWindowWidth()),
                                                      static_cast<int>(ImGui::GetWindowHeight())));
            if (!m_dolphin_image) {
                ImGui::Text("Start a Dolphin process running\nSuper Mario Sunshine to get started");
            } else {
                m_dolphin_painter.render(m_dolphin_image, render_size);

                ImGui::SetCursorPos(cursor_pos);
                for (const auto &[layer_name, render_layer] : m_render_layers) {
                    render_layer(delta_time, std::string_view(layer_name), ImGui::GetWindowSize().x,
                                 ImGui::GetWindowSize().y, m_dolphin_vp_mtx, getUUID());
                    ImGui::SetCursorPos(cursor_pos);
                }
            }
        }

        // ImGui::PopClipRect();

        renderPlaybackButtons(delta_time);
    }
    ImGui::End();
}  // namespace Toolbox::UI

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

void SceneWindow::renderRailContextMenu(std::string str_id, SelectionNodeInfo<Rail::Rail> &info) {
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
        "Insert Object Before...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_BEFORE);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_virtual_node_menu.addOption(
        "Insert Object After...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_AFTER);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_virtual_node_menu.addDivider();

    m_hierarchy_virtual_node_menu.addOption("Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                                            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                                                m_rename_obj_dialog.open();
                                                m_rename_obj_dialog.setOriginalName(
                                                    info.m_selected->getNameRef().name());
                                                return;
                                            });

    m_hierarchy_virtual_node_menu.addDivider();

    m_hierarchy_virtual_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
            GUIApplication::instance().getSceneObjectClipboard().setData(info);
            return;
        });

    m_hierarchy_virtual_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto nodes       = GUIApplication::instance().getSceneObjectClipboard().getData();
            auto this_parent = info.m_selected->getParent();
            if (!this_parent) {
                LogError(
                    make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                        .error());
                return;
            }
            std::vector<std::string> sibling_names;
            auto children = this_parent->getChildren();
            for (auto &child : children) {
                sibling_names.push_back(std::string(child->getNameRef().name()));
            }
            for (auto &node : nodes) {
                RefPtr<ISceneObject> new_object = make_deep_clone<ISceneObject>(node.m_selected);
                NameRef node_ref                = new_object->getNameRef();
                node_ref.setName(Util::MakeNameUnique(node_ref.name(), sibling_names));
                new_object->setNameRef(node_ref);
                this_parent->addChild(new_object);
                sibling_names.push_back(std::string(node_ref.name()));
            }
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_virtual_node_menu.addDivider();

    m_hierarchy_virtual_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto this_parent = reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
            if (!this_parent) {
                LogError(
                    make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                        .error());
                return;
            }
            this_parent->removeChild(info.m_selected->getNameRef().name());

            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            task_communicator.taskRemoveSceneObject(info.m_selected, get_shared_ptr(*this_parent));

            auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                     m_hierarchy_selected_nodes.end(), info);
            m_hierarchy_selected_nodes.erase(node_it);
            m_update_render_objs = true;
            return;
        });
}

void SceneWindow::buildContextMenuGroupObj() {
    m_hierarchy_group_node_menu = ContextMenu<SelectionNodeInfo<Object::ISceneObject>>();

    m_hierarchy_group_node_menu.addOption(
        "Add Child Object...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_CHILD);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_group_node_menu.addOption(
        "Insert Object Before...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_BEFORE);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_group_node_menu.addOption(
        "Insert Object After...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_AFTER);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_group_node_menu.addDivider();

    m_hierarchy_group_node_menu.addOption("Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                                          [this](SelectionNodeInfo<Object::ISceneObject> info) {
                                              m_rename_obj_dialog.open();
                                              m_rename_obj_dialog.setOriginalName(
                                                  info.m_selected->getNameRef().name());
                                              return;
                                          });

    m_hierarchy_group_node_menu.addDivider();

    m_hierarchy_group_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
            GUIApplication::instance().getSceneObjectClipboard().setData(info);
            return;
        });

    m_hierarchy_group_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto nodes       = GUIApplication::instance().getSceneObjectClipboard().getData();
            auto this_parent = std::reinterpret_pointer_cast<GroupSceneObject>(info.m_selected);
            if (!this_parent) {
                LogError(
                    make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                        .error());
                return;
            }
            std::vector<std::string> sibling_names;
            auto children = this_parent->getChildren();
            for (auto &child : children) {
                sibling_names.push_back(std::string(child->getNameRef().name()));
            }
            for (auto &node : nodes) {
                auto new_object  = make_deep_clone<ISceneObject>(node.m_selected);
                NameRef node_ref = new_object->getNameRef();
                node_ref.setName(Util::MakeNameUnique(node_ref.name(), sibling_names));
                new_object->setNameRef(node_ref);
                this_parent->addChild(new_object);
                sibling_names.push_back(std::string(node_ref.name()));
            }
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_group_node_menu.addDivider();

    m_hierarchy_group_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto this_parent = reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
            if (!this_parent) {
                LogError(
                    make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                        .error());
                return;
            }
            this_parent->removeChild(info.m_selected->getNameRef().name());

            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            task_communicator.taskRemoveSceneObject(info.m_selected, get_shared_ptr(*this_parent));

            auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                     m_hierarchy_selected_nodes.end(), info);
            m_hierarchy_selected_nodes.erase(node_it);
            m_update_render_objs = true;
            return;
        });
}

void SceneWindow::buildContextMenuPhysicalObj() {
    m_hierarchy_physical_node_menu = ContextMenu<SelectionNodeInfo<Object::ISceneObject>>();

    m_hierarchy_physical_node_menu.addOption(
        "Insert Object Before...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_BEFORE);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_physical_node_menu.addOption(
        "Insert Object After...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_create_obj_dialog.setInsertPolicy(CreateObjDialog::InsertPolicy::INSERT_AFTER);
            m_create_obj_dialog.open();
            return;
        });

    m_hierarchy_physical_node_menu.addDivider();

    m_hierarchy_physical_node_menu.addOption(
        "Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            m_rename_obj_dialog.open();
            m_rename_obj_dialog.setOriginalName(info.m_selected->getNameRef().name());
            return;
        });

    m_hierarchy_physical_node_menu.addDivider();

    m_hierarchy_physical_node_menu.addOption(
        "View in Scene", {KeyCode::KEY_LEFTALT, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto member_result = info.m_selected->getMember("Transform");
            if (!member_result) {
                LogError(make_error<void>("Scene Hierarchy",
                                          "Failed to find transform member of physical object")
                             .error());
                return;
            }
            auto member_ptr = member_result.value();
            if (!member_ptr) {
                LogError(make_error<void>("Scene Hierarchy",
                                          "Found the transform member but it was null")
                             .error());
                return;
            }
            Transform transform = getMetaValue<Transform>(member_ptr).value();
            f32 max_scale       = std::max(transform.m_scale.x, transform.m_scale.y);
            max_scale           = std::max(max_scale, transform.m_scale.z);

            m_renderer.setCameraOrientation({0, 1, 0}, transform.m_translation,
                                            {transform.m_translation.x, transform.m_translation.y,
                                             transform.m_translation.z + 1000 * max_scale});
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_physical_node_menu.addOption(
        "Move to Camera", {KeyCode::KEY_LEFTALT, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto member_result = info.m_selected->getMember("Transform");
            if (!member_result) {
                LogError(make_error<void>("Scene Hierarchy",
                                          "Failed to find transform member of physical object")
                             .error());
                return;
            }
            auto member_ptr = member_result.value();
            if (!member_ptr) {
                LogError(make_error<void>("Scene Hierarchy",
                                          "Found the transform member but it was null")
                             .error());
                return;
            }

            Transform transform = getMetaValue<Transform>(member_ptr).value();
            m_renderer.getCameraTranslation(transform.m_translation);
            auto result = setMetaValue<Transform>(member_ptr, 0, transform);
            if (!result) {
                LogError(make_error<void>("Scene Hierarchy",
                                          "Failed to set the transform member of physical object")
                             .error());
                return;
            }

            m_update_render_objs = true;
            return;
        });

    m_hierarchy_physical_node_menu.addDivider();

    m_hierarchy_physical_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
            GUIApplication::instance().getSceneObjectClipboard().setData(info);
            return;
        });

    m_hierarchy_physical_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto nodes       = GUIApplication::instance().getSceneObjectClipboard().getData();
            auto this_parent = reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
            if (!this_parent) {
                LogError(
                    make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                        .error());
                return;
            }
            std::vector<std::string> sibling_names;
            auto children = this_parent->getChildren();
            for (auto &child : children) {
                sibling_names.push_back(std::string(child->getNameRef().name()));
            }
            for (auto &node : nodes) {
                auto new_object  = make_deep_clone<ISceneObject>(node.m_selected);
                NameRef node_ref = new_object->getNameRef();
                node_ref.setName(Util::MakeNameUnique(node_ref.name(), sibling_names));
                new_object->setNameRef(node_ref);
                this_parent->addChild(new_object);
                sibling_names.push_back(std::string(node_ref.name()));
            }
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_physical_node_menu.addDivider();

    m_hierarchy_physical_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
            auto this_parent = reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
            if (!this_parent) {
                LogError(
                    make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                        .error());
                return;
            }
            this_parent->removeChild(info.m_selected->getNameRef().name());

            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            task_communicator.taskRemoveSceneObject(info.m_selected, get_shared_ptr(*this_parent));

            auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                     m_hierarchy_selected_nodes.end(), info);
            m_hierarchy_selected_nodes.erase(node_it);
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_physical_node_menu.addDivider();

    m_hierarchy_physical_node_menu.addOption(
        "Copy Player Transform", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_P},
        [this]() {
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            return task_communicator.isSceneLoaded();
        },
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            task_communicator.setObjectTransformToMario(
                ref_cast<PhysicalSceneObject>(info.m_selected));
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_physical_node_menu.addOption(
        "Copy Player Position", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_P},
        [this]() {
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            return task_communicator.isSceneLoaded();
        },
        [this](SelectionNodeInfo<Object::ISceneObject> info) {
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            task_communicator.setObjectTranslationToMario(
                ref_cast<PhysicalSceneObject>(info.m_selected));
            m_update_render_objs = true;
            return;
        });
}

void SceneWindow::buildContextMenuMultiObj() {
    m_hierarchy_multi_node_menu =
        ContextMenu<std::vector<SelectionNodeInfo<Object::ISceneObject>>>();

    m_hierarchy_multi_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
            for (auto &info : infos) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
            }
            GUIApplication::instance().getSceneObjectClipboard().setData(infos);
            return;
        });

    m_hierarchy_multi_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
            auto nodes = GUIApplication::instance().getSceneObjectClipboard().getData();
            for (auto &info : infos) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    LogError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                std::vector<std::string> sibling_names;
                auto children = this_parent->getChildren();
                for (auto &child : children) {
                    sibling_names.push_back(std::string(child->getNameRef().name()));
                }
                for (auto &node : nodes) {
                    auto new_object  = make_deep_clone<ISceneObject>(node.m_selected);
                    NameRef node_ref = new_object->getNameRef();
                    node_ref.setName(Util::MakeNameUnique(node_ref.name(), sibling_names));
                    new_object->setNameRef(node_ref);
                    this_parent->addChild(new_object);
                    sibling_names.push_back(std::string(node_ref.name()));
                }
            }
            m_update_render_objs = true;
            return;
        });

    m_hierarchy_multi_node_menu.addDivider();

    m_hierarchy_multi_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE},
        [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
            for (auto &info : infos) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    LogError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());

                Game::TaskCommunicator &task_communicator =
                    GUIApplication::instance().getTaskCommunicator();
                task_communicator.taskRemoveSceneObject(info.m_selected,
                                                        get_shared_ptr(*this_parent));

                auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                         m_hierarchy_selected_nodes.end(), info);
                m_hierarchy_selected_nodes.erase(node_it);
            }
            m_update_render_objs = true;
            return;
        });
}

void SceneWindow::buildContextMenuRail() {
    m_rail_list_single_node_menu = ContextMenu<SelectionNodeInfo<Rail::Rail>>();

    m_rail_list_single_node_menu.addOption("Insert Rail Here...",
                                           {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                                           [this](SelectionNodeInfo<Rail::Rail> info) {
                                               m_create_rail_dialog.open();
                                               return;
                                           });

    m_rail_list_single_node_menu.addDivider();

    m_rail_list_single_node_menu.addOption("Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                                           [this](SelectionNodeInfo<Rail::Rail> info) {
                                               m_rename_rail_dialog.open();
                                               m_rename_rail_dialog.setOriginalName(
                                                   info.m_selected->name());
                                               return;
                                           });

    // m_rail_list_single_node_menu.addDivider();
    //
    // TODO: Add rail manipulators here.

    m_rail_list_single_node_menu.addDivider();

    m_rail_list_single_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Rail::Rail> info) {
            info.m_selected = make_deep_clone<Rail::Rail>(info.m_selected);
            GUIApplication::instance().getSceneRailClipboard().setData(info);
            return;
        });

    m_rail_list_single_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Rail::Rail> info) {
            auto nodes = GUIApplication::instance().getSceneRailClipboard().getData();
            if (nodes.size() > 0) {
                RailData &data        = m_current_scene->getRailData();
                size_t selected_index = data.getRailCount();
                auto result           = data.getRailIndex(info.m_selected->name());
                if (result) {
                    selected_index = result.value();
                }
                std::vector<std::string> sibling_names;
                for (auto &rail : data.rails()) {
                    sibling_names.push_back(rail->name());
                }
                for (auto &node : nodes) {
                    Rail::Rail new_rail = *node.m_selected;
                    new_rail.setName(Util::MakeNameUnique(new_rail.name(), sibling_names));
                    data.insertRail(selected_index + 1, new_rail);
                    selected_index += 1;
                }
            }
            m_update_render_objs = true;
            return;
        });

    m_rail_list_single_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE}, [this](SelectionNodeInfo<Rail::Rail> info) {
            m_rail_visible_map.erase(info.m_node_id);
            m_current_scene->getRailData().removeRail(info.m_selected->getUUID());
            m_update_render_objs = true;
            return;
        });
}

void SceneWindow::buildContextMenuMultiRail() {
    m_rail_list_multi_node_menu = ContextMenu<std::vector<SelectionNodeInfo<Rail::Rail>>>();

    m_rail_list_multi_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
            for (auto &select : info) {
                select.m_selected = make_deep_clone<Rail::Rail>(select.m_selected);
            }
            GUIApplication::instance().getSceneRailClipboard().setData(info);
            return;
        });

    m_rail_list_multi_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
            auto nodes = GUIApplication::instance().getSceneRailClipboard().getData();
            if (nodes.size() > 0) {
                RailData &data        = m_current_scene->getRailData();
                size_t selected_index = data.getRailCount();
                auto result           = data.getRailIndex(info[0].m_selected->name());
                if (result) {
                    selected_index = result.value();
                }
                std::vector<std::string> sibling_names;
                for (auto &rail : data.rails()) {
                    sibling_names.push_back(rail->name());
                }
                for (auto &node : nodes) {
                    Rail::Rail new_rail = *node.m_selected;
                    new_rail.setName(Util::MakeNameUnique(new_rail.name(), sibling_names));
                    data.insertRail(selected_index + 1, new_rail);
                    selected_index += 1;
                }
            }
            m_update_render_objs = true;
            return;
        });

    m_rail_list_multi_node_menu.addOption("Delete", {KeyCode::KEY_DELETE},
                                          [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
                                              RailData &data = m_current_scene->getRailData();
                                              for (auto &select : info) {
                                                  m_rail_visible_map.erase(select.m_node_id);
                                                  data.removeRail(select.m_selected->getUUID());
                                              }
                                              m_update_render_objs = true;
                                              return;
                                          });
}

void SceneWindow::buildContextMenuRailNode() {
    m_rail_node_list_single_node_menu = ContextMenu<SelectionNodeInfo<Rail::RailNode>>();

    m_rail_node_list_single_node_menu.addOption("Insert Node Here...",
                                                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                                                [this](SelectionNodeInfo<Rail::RailNode> info) {
                                                    m_create_rail_dialog.open();
                                                    return Result<void>();
                                                });

    m_rail_node_list_single_node_menu.addDivider();

    m_rail_node_list_single_node_menu.addOption(
        "View in Scene", {KeyCode::KEY_LEFTALT, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Rail::RailNode> info) {
            glm::vec3 translation = info.m_selected->getPosition();

            m_renderer.setCameraOrientation({0, 1, 0}, translation,
                                            {translation.x, translation.y, translation.z + 1000});
            m_update_render_objs = true;
            return Result<void>();
        });

    m_rail_node_list_single_node_menu.addOption(
        "Move to Camera", {KeyCode::KEY_LEFTALT, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Rail::RailNode> info) {
            RefPtr<Rail::Rail> rail =
                m_current_scene->getRailData().getRail(info.m_selected->getRailUUID());

            glm::vec3 translation;
            m_renderer.getCameraTranslation(translation);
            auto result = rail->setNodePosition(info.m_selected, translation);
            if (!result) {
                LogError(result.error());
                return;
            }

            m_renderer.updatePaths(m_current_scene->getRailData(), m_rail_visible_map);
            m_update_render_objs = true;
            return;
        });

    m_rail_node_list_single_node_menu.addDivider();

    m_rail_node_list_single_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](SelectionNodeInfo<Rail::RailNode> info) {
            info.m_selected = make_deep_clone<Rail::RailNode>(info.m_selected);
            GUIApplication::instance().getSceneRailNodeClipboard().setData(info);
            return Result<void>();
        });

    m_rail_node_list_single_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](SelectionNodeInfo<Rail::RailNode> info) {
            RefPtr<Rail::Rail> rail =
                m_current_scene->getRailData().getRail(info.m_selected->getRailUUID());
            size_t selected_index = rail->getNodeCount();
            auto result           = rail->getNodeIndex(info.m_selected);
            if (result) {
                selected_index = result.value();
            }
            auto nodes = GUIApplication::instance().getSceneRailNodeClipboard().getData();
            for (auto &node : nodes) {
                rail->insertNode(selected_index + 1, node.m_selected);
                selected_index += 1;
            }
            m_update_render_objs = true;
            return Result<void>();
        });

    m_rail_node_list_single_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE}, [this](SelectionNodeInfo<Rail::RailNode> info) {
            RefPtr<Rail::Rail> rail =
                m_current_scene->getRailData().getRail(info.m_selected->getRailUUID());
            rail->removeNode(info.m_selected);
            m_update_render_objs = true;
            return Result<void>();
        });
}

void SceneWindow::buildContextMenuMultiRailNode() {
    m_rail_node_list_multi_node_menu =
        ContextMenu<std::vector<SelectionNodeInfo<Rail::RailNode>>>();

    m_rail_node_list_multi_node_menu.addOption(
        "Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
        [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
            for (auto &select : info) {
                select.m_selected = make_deep_clone<Rail::RailNode>(select.m_selected);
            }
            GUIApplication::instance().getSceneRailNodeClipboard().setData(info);
            return Result<void>();
        });

    m_rail_node_list_multi_node_menu.addOption(
        "Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
        [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
            RefPtr<Rail::Rail> rail =
                m_current_scene->getRailData().getRail(info[0].m_selected->getRailUUID());
            size_t selected_index = rail->getNodeCount();
            auto result           = rail->getNodeIndex(info[0].m_selected);
            if (result) {
                selected_index = result.value();
            }
            auto nodes = GUIApplication::instance().getSceneRailNodeClipboard().getData();
            for (auto &node : nodes) {
                rail->insertNode(selected_index + 1, node.m_selected);
                selected_index += 1;
            }
            m_update_render_objs = true;
            return Result<void>();
        });

    m_rail_node_list_multi_node_menu.addOption(
        "Delete", {KeyCode::KEY_DELETE},
        [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
            for (auto &select : info) {
                RefPtr<Rail::Rail> rail =
                    m_current_scene->getRailData().getRail(select.m_selected->getRailUUID());
                rail->removeNode(select.m_selected);
            }
            m_update_render_objs = true;
            return Result<void>();
        });
}

void SceneWindow::buildCreateObjDialog() {
    m_create_obj_dialog.setup();
    m_create_obj_dialog.setActionOnAccept(
        [this](size_t sibling_index, std::string_view name, const Object::Template &template_,
               std::string_view wizard_name, CreateObjDialog::InsertPolicy policy,
               SelectionNodeInfo<Object::ISceneObject> info) {
            auto new_object_result = Object::ObjectFactory::create(template_, wizard_name);
            if (!name.empty()) {
                new_object_result->setNameRef(name);
            }

            size_t insert_index;

            GroupSceneObject *this_parent;
            if (info.m_selected->isGroupObject() &&
                policy == CreateObjDialog::InsertPolicy::INSERT_CHILD) {
                this_parent  = reinterpret_cast<GroupSceneObject *>(info.m_selected.get());
                insert_index = this_parent->getChildren().size();
            } else {
                this_parent  = reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                insert_index = policy == CreateObjDialog::InsertPolicy::INSERT_BEFORE
                                   ? sibling_index
                                   : sibling_index + 1;
            }

            if (!this_parent) {
                auto result = make_error<void>("Scene Hierarchy",
                                               "Failed to get parent node for obj creation");
                LogError(result.error());
                return;
            }

            auto result = this_parent->insertChild(insert_index, std::move(new_object_result));
            if (!result) {
                LogError(result.error());
                return;
            }

            RefPtr<ISceneObject> object = this_parent->getChild(std::string(name));

            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            task_communicator.taskAddSceneObject(
                object, get_shared_ptr(*this_parent),
                [object](u32 actor_ptr) { object->setGamePtr(actor_ptr); });

            m_update_render_objs = true;
            return;
        });
    m_create_obj_dialog.setActionOnReject([](SelectionNodeInfo<Object::ISceneObject>) {});
}

void SceneWindow::buildRenameObjDialog() {
    m_rename_obj_dialog.setup();
    m_rename_obj_dialog.setActionOnAccept(
        [this](std::string_view new_name, SelectionNodeInfo<Object::ISceneObject> info) {
            if (new_name.empty()) {
                auto result = make_error<void>("Rail Data", "Can not rename rail to empty string");
                LogError(result.error());
                return;
            }
            info.m_selected->setNameRef(new_name);
        });
    m_rename_obj_dialog.setActionOnReject([](SelectionNodeInfo<Object::ISceneObject>) {});
}

void SceneWindow::buildCreateRailDialog() {
    m_create_rail_dialog.setup();
    m_create_rail_dialog.setActionOnAccept(
        [this](std::string_view name, u16 node_count, s16 node_distance, bool loop) {
            if (name.empty()) {
                auto result = make_error<void>("Rail Data", "Can not name rail as empty string");
                LogError(result.error());
                return;
            }

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

                auto node = make_referable<Rail::RailNode>(x, y, z, 0);
                new_nodes.push_back(node);

                angle += angle_step;
            }

            Rail::Rail new_rail(name, new_nodes);

            for (u16 i = 0; i < node_count; ++i) {
                auto result = new_rail.connectNodeToNeighbors(i, true);
                if (!result) {
                    LogError(result.error());
                }
            }

            if (!loop) {
                // First
                {
                    auto result = new_rail.removeConnection(0, 1);
                    if (!result) {
                        LogError(result.error());
                    }
                }

                // Last
                {
                    auto result = new_rail.removeConnection(node_count - 1, 1);
                    if (!result) {
                        LogError(result.error());
                    }
                }
            }

            m_current_scene->getRailData().addRail(new_rail);

            m_renderer.updatePaths(m_current_scene->getRailData(), m_rail_visible_map);
            m_update_render_objs = true;
        });
}

void SceneWindow::buildRenameRailDialog() {
    m_rename_rail_dialog.setup();
    m_rename_rail_dialog.setActionOnAccept(
        [this](std::string_view new_name, SelectionNodeInfo<Rail::Rail> info) {
            if (new_name.empty()) {
                auto result =
                    make_error<void>("Scene Hierarchy", "Can not rename rail to empty string");
                LogError(result.error());
                return;
            }
            info.m_selected->setName(new_name);
        });
    m_rename_rail_dialog.setActionOnReject([](SelectionNodeInfo<Rail::Rail>) {});
}

void SceneWindow::saveMimeObject(Buffer &buffer, size_t index, RefPtr<ISceneObject> parent) {
    if (index >= parent->getChildren().size()) {
        LogError(make_error<void>("Scene Hierarchy", "Failed to get child object").error());
        return;
    }

    buffer.alloc(32);
    buffer.set<bool>(0, true);  // Internal
    buffer.set<u16>(1, index);  // Index
    buffer.set<u64>(3, parent->getUUID());

    /*TRY(Serializer::ObjectToBytes(*parent->getChildren()[index], buffer, 32))
        .error([](const SerialError &err) { LogError(err); });*/
}

void SceneWindow::saveMimeRail(Buffer &buffer, size_t index) {
    RefPtr<Rail::Rail> rail = m_current_scene->getRailData().getRail(index);
    if (!rail) {
        LogError(make_error<void>("Scene Hierarchy", "Failed to get rail").error());
        return;
    }

    TRY(Serializer::ObjectToBytes(*rail, buffer, 4))
        .error([](const SerialError &err) { LogError(err); })
        .then([&]() {
            buffer.set<bool>(0, true);  // Internal
            buffer.set<u16>(1, index);  // Index
        });
}

void SceneWindow::saveMimeRailNode(Buffer &buffer, size_t index, RefPtr<Rail::Rail> parent) {
    if (index >= parent->nodes().size()) {
        LogError(make_error<void>("Scene Hierarchy", "Failed to get rail node").error());
        return;
    }

    TRY(Serializer::ObjectToBytes(*parent->nodes()[index], buffer, 32))
        .error([](const SerialError &err) { LogError(err); })
        .then([&]() {
            buffer.set<bool>(0, true);  // Internal
            buffer.set<u16>(1, index);  // Index
            buffer.set<u64>(3, parent->getUUID());
        });
}

void SceneWindow::loadMimeObject(Buffer &buffer, size_t index, UUID64 parent_id) {
    bool is_internal = buffer.get<bool>(0);  // Internal

    u16 orig_index          = buffer.get<u16>(1);  // Index
    UUID64 orig_parent_uuid = buffer.get<UUID64>(3);

    RefPtr<ISceneObject> parent = m_current_scene->getObjHierarchy().findObject(parent_id);
    if (!parent) {
        LogError(make_error<void>("Scene Hierarchy", "Failed to get parent object").error());
        return;
    }

    if (is_internal) {
        RefPtr<ISceneObject> orig_parent =
            m_current_scene->getObjHierarchy().findObject(orig_parent_uuid);
        if (!orig_parent) {
            LogError(make_error<void>("Scene Hierarchy", "Failed to get original parent object")
                         .error());
            return;
        }
        RefPtr<ISceneObject> obj = orig_parent->getChildren()[orig_index];
        orig_parent->removeChild(orig_index)
            .and_then([&]() {
                parent->insertChild(index, obj);
                return Result<void, ObjectGroupError>();
            })
            .or_else([](const ObjectGroupError &err) {
                LogError(err);
                return Result<void, ObjectGroupError>();
            });
        return;
    }

    LogError(make_error<void>("Scene Hierarchy", "External object unsupported.").error());

    m_update_render_objs = true;
    return;

    //// Get to the object data
    // in.seek(32, std::ios::beg);

    // auto result = ObjectFactory::create(in);
    // if (!result) {
    //     LogError(result.error());
    //     return;
    // }

    // RefPtr<ISceneObject> obj = std::move(result.value());
    // if (!obj) {
    //     LogError(make_error<void>("Scene Hierarchy", "Failed to create object").error());
    //     return;
    // }

    // if (is_internal) {
    //     RefPtr<ISceneObject> orig_parent =
    //         m_current_scene->getObjHierarchy().findObject(orig_parent_uuid);
    //     if (orig_parent) {
    //         TRY(orig_parent->removeChild(orig_index)).error([](const ObjectGroupError &err) {
    //             LogError(err);
    //         });
    //     }
    // }

    // RefPtr<ISceneObject> orig_parent =
    //     m_current_scene->getObjHierarchy().findObject(orig_parent_uuid);
    // if (orig_parent) {
    //     TRY(orig_parent->insertChild(index, obj)).error([](const ObjectGroupError &err) {
    //         LogError(err);
    //     });
    // }
}

void SceneWindow::loadMimeRail(Buffer &buffer, size_t index) {
    Rail::Rail rail("((null))");

    bool is_internal = buffer.get<bool>(0);  // Internal
    u16 orig_index   = buffer.get<u16>(1);   // Index

    auto result = Deserializer::BytesToObject(buffer, rail, 4);
    if (!result) {
        LogError(result.error());
        return;
    }

    m_current_scene->getRailData().insertRail(index, rail);
    m_update_render_objs = true;
}

void SceneWindow::loadMimeRailNode(Buffer &buffer, size_t index, UUID64 rail_id) {
    Rail::RailNode node;

    bool is_internal = buffer.get<bool>(0);  // Internal

    u16 orig_index          = buffer.get<u16>(1);  // Index
    UUID64 orig_parent_uuid = buffer.get<UUID64>(3);

    Deserializer::BytesToObject(buffer, node, 32)
        .and_then([&]() {
            _moveNode(node, index, rail_id, orig_index, orig_parent_uuid, is_internal);
            m_update_render_objs = true;
            return Result<void, SerialError>();
        })
        .or_else([](const SerialError &error) {
            LogError(error);
            return Result<void, SerialError>();
        });
}

void Toolbox::UI::SceneWindow::processObjectSelection(RefPtr<Object::ISceneObject> node,
                                                      bool is_multi) {
    if (!node) {
        TOOLBOX_DEBUG_LOG("Hit object is null");
        return;
    }

    std::string node_uid_str = getNodeUID(node);
    ImGuiID tree_node_id     = static_cast<ImGuiID>(node->getUUID());

    bool is_object_selected =
        std::any_of(m_hierarchy_selected_nodes.begin(), m_hierarchy_selected_nodes.end(),
                    [&](auto &info) { return info.m_node_id == tree_node_id; });

    SelectionNodeInfo<Object::ISceneObject> node_info = {
        .m_selected      = node,
        .m_node_id       = tree_node_id,
        .m_parent_synced = true,
        .m_scene_synced  = true};  // Only spacial objects get scene selection

    if (is_multi) {
        m_selected_properties.clear();
        if (!is_object_selected)
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

    Transform obj_transform = node->getTransform().value();
    // BoundingBox obj_bb      = node->getBoundingBox().value();

    glm::mat4x4 gizmo_transform =
        glm::translate(glm::identity<glm::mat4x4>(), obj_transform.m_translation);

    /*glm::mat4x4 obb_rot_mtx =
        glm::eulerAngleXYZ(glm::radians(obj_transform.m_rotation.x),
       glm::radians(obj_transform.m_rotation.y),
                           glm::radians(obj_transform.m_rotation.z));*/

    // TODO: Figure out why this rotation composes wrong.
    glm::mat4x4 obb_rot_mtx = glm::toMat4(glm::quat(obj_transform.m_rotation));
    gizmo_transform         = gizmo_transform * obb_rot_mtx;

    gizmo_transform = glm::scale(gizmo_transform, obj_transform.m_scale);
    m_renderer.setGizmoTransform(gizmo_transform);

    m_properties_render_handler = renderObjectProperties;

    TOOLBOX_DEBUG_LOG_V("Hit object {} ({})", node->type(), node->getNameRef().name());
}

void Toolbox::UI::SceneWindow::processRailSelection(RefPtr<Rail::Rail> node, bool is_multi) {
    ImGuiID rail_id = static_cast<ImGuiID>(node->getUUID());

    bool is_rail_selected =
        std::any_of(m_rail_list_selected_nodes.begin(), m_rail_list_selected_nodes.end(),
                    [&](auto &info) { return info.m_node_id == rail_id; });

    SelectionNodeInfo<Rail::Rail> rail_info = {
        .m_selected = node, .m_node_id = rail_id, .m_parent_synced = true, .m_scene_synced = false};

    if (is_multi) {
        m_selected_properties.clear();
        if (!is_rail_selected)
            m_rail_list_selected_nodes.push_back(rail_info);
    } else {
        m_rail_list_selected_nodes.clear();
        m_rail_list_selected_nodes.push_back(rail_info);
    }

    // Since a rail is selected, we should clear the rail nodes
    m_rail_node_list_selected_nodes.clear();

    glm::mat4x4 gizmo_transform =
        glm::translate(glm::identity<glm::mat4x4>(), node->getCenteroid());
    m_renderer.setGizmoTransform(gizmo_transform);

    m_properties_render_handler = renderRailProperties;

    TOOLBOX_DEBUG_LOG_V("Hit rail \"{}\"", node->name());
}

void Toolbox::UI::SceneWindow::processRailNodeSelection(RefPtr<Rail::RailNode> node,
                                                        bool is_multi) {
    ImGuiID node_id = static_cast<ImGuiID>(node->getUUID());

    bool is_rail_node_selected =
        std::any_of(m_rail_node_list_selected_nodes.begin(), m_rail_node_list_selected_nodes.end(),
                    [&](auto &info) { return info.m_node_id == node_id; });

    SelectionNodeInfo<Rail::RailNode> node_info = {
        .m_selected = node, .m_node_id = node_id, .m_parent_synced = true, .m_scene_synced = false};

    if (is_multi) {
        m_selected_properties.clear();
        if (!is_rail_node_selected)
            m_rail_node_list_selected_nodes.push_back(node_info);
    } else {
        m_rail_node_list_selected_nodes.clear();
        m_rail_node_list_selected_nodes.push_back(node_info);
    }

    // Since a rail node is selected, we should clear the rails
    m_rail_list_selected_nodes.clear();

    glm::mat4x4 gizmo_transform = glm::translate(glm::identity<glm::mat4x4>(), node->getPosition());
    m_renderer.setGizmoTransform(gizmo_transform);

    m_properties_render_handler = renderRailNodeProperties;

    // Debug log
    {
        RailData &rail_data = m_current_scene->getRailData();

        RefPtr<Rail::Rail> rail = rail_data.getRail(node->getRailUUID());
        if (!rail) {
            TOOLBOX_ERROR("Failed to find rail for node.");
            return;
        }

        TOOLBOX_DEBUG_LOG_V("Hit node {} of rail \"{}\"", rail->getNodeIndex(node).value(),
                            rail->name());
    }
}

void Toolbox::UI::SceneWindow::_moveNode(const Rail::RailNode &node, size_t index, UUID64 rail_id,
                                         size_t orig_index, UUID64 orig_id, bool is_internal) {
    RailData &data                        = m_current_scene->getRailData();
    std::vector<RefPtr<Rail::Rail>> rails = data.rails();

    auto new_rail_it = std::find_if(rails.begin(), rails.end(), [&](RefPtr<Rail::Rail> rail) {
        return rail->getUUID() == rail_id;
    });

    if (new_rail_it == rails.end()) {
        LogError(
            make_error<void>("Scene Hierarchy", "Failed to find rail to move node to").error());
        return;
    }

    if (is_internal) {
        auto orig_rail_it = std::find_if(rails.begin(), rails.end(), [&](RefPtr<Rail::Rail> rail) {
            return rail->getUUID() == orig_id;
        });

        if (orig_rail_it != rails.end()) {
            // The node is being moved forward in the same rail
            if (orig_index < index && orig_rail_it == new_rail_it) {
                index -= 1;
            }

            (*orig_rail_it)->removeNode(orig_index).or_else([](const MetaError &err) {
                LogError(err);
                return Result<void, MetaError>();
            });
        }
    }

    (*new_rail_it)
        ->insertNode(index, make_referable<Rail::RailNode>(node))
        .or_else([](const MetaError &err) {
            LogError(err);
            return Result<void, MetaError>();
        });
}

ImGuiID SceneWindow::onBuildDockspace() {
    ImGuiID dockspace_id = ImGui::GetID(std::to_string(getUUID()).c_str());
    ImGui::DockBuilderAddNode(dockspace_id);
    {
        ImGuiID other_node_id;
        m_dock_node_left_id      = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                                               nullptr, &other_node_id);
        m_dock_node_up_left_id   = ImGui::DockBuilderSplitNode(m_dock_node_left_id, ImGuiDir_Down,
                                                               0.5f, nullptr, &m_dock_node_left_id);
        m_dock_node_down_left_id = ImGui::DockBuilderSplitNode(
            m_dock_node_up_left_id, ImGuiDir_Down, 0.5f, nullptr, &m_dock_node_up_left_id);

        ImGui::DockBuilderDockWindow(ImWindowComponentTitle(*this, "Scene View").c_str(),
                                     other_node_id);
        ImGui::DockBuilderDockWindow(ImWindowComponentTitle(*this, "Dolphin View").c_str(),
                                     other_node_id);
        ImGui::DockBuilderDockWindow(ImWindowComponentTitle(*this, "Hierarchy Editor").c_str(),
                                     m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(ImWindowComponentTitle(*this, "Rail Editor").c_str(),
                                     m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(ImWindowComponentTitle(*this, "Properties Editor").c_str(),
                                     m_dock_node_down_left_id);
    }
    ImGui::DockBuilderFinish(dockspace_id);
    return dockspace_id;
}

void SceneWindow::onRenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File", true)) {
            if (ImGui::MenuItem(ICON_FK_FLOPPY_O " Save")) {
                m_is_save_default_ready = true;
            }
            if (ImGui::MenuItem(ICON_FK_FLOPPY_O " Save As...")) {
                m_is_save_as_dialog_open = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Verify")) {
            // TODO: Flag scene verification (ideally on new process or thread)
        }
        ImGui::EndMenuBar();
    }

    if (m_is_save_default_ready) {
        m_is_save_default_ready = false;
        if (m_current_scene->rootPath())
            (void)onSaveData(m_current_scene->rootPath());
        else
            m_is_save_as_dialog_open = true;
    }

    if (m_is_save_as_dialog_open) {
        ImGuiFileDialog::Instance()->OpenDialog("SaveSceneDialog", "Choose Directory", nullptr, "",
                                                "");
    }

    if (ImGuiFileDialog::Instance()->Display("SaveSceneDialog")) {
        ImGuiFileDialog::Instance()->Close();
        m_is_save_as_dialog_open = false;

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

            auto dir_result = Toolbox::Filesystem::is_directory(path);
            if (!dir_result) {
                return;
            }

            if (!dir_result.value()) {
                return;
            }

            auto result = m_current_scene->saveToPath(path);
            if (!result) {
                LogError(result.error());
            }
        }
    }
}  // namespace Toolbox::UI