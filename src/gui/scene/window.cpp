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

#include <glm/gtx/euler_angles.hpp>

using namespace Toolbox::Scene;

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

    static std::string getNodeUID(std::shared_ptr<Toolbox::Object::ISceneObject> node) {
        std::string node_name = std::format("{} ({})##{}", node->type(), node->getNameRef().name(),
                                            node->getID());
        return node_name;
    }

    static std::string getNodeUID(std::shared_ptr<Rail::Rail> node) {
        std::string node_name = std::format("{}##{}", node->name(), node->getSiblingID());
        return node_name;
    }

    SceneWindow::~SceneWindow() {
        // J3DRendering::Cleanup();
        m_renderables.clear();
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

    bool SceneWindow::update(f32 deltaTime) {
        bool inputState = m_renderer.inputUpdate();

        std::vector<std::shared_ptr<Rail::RailNode>> rendered_nodes;
        for (auto &rail : m_current_scene->getRailData().rails()) {
            if (!m_rail_visible_map[rail->name()])
                continue;
            rendered_nodes.insert(rendered_nodes.end(), rail->nodes().begin(), rail->nodes().end());
        }

        bool should_reset                       = false;
        bool should_select                      = false;
        Renderer::selection_variant_t selection = std::nullopt;
        if (!ImGuizmo::IsOver()) {
            should_select = true;
            selection     = m_renderer.findSelection(m_renderables, rendered_nodes, should_reset);
        } else {
            return inputState;
        }

        bool multi_select = Input::GetKey(GLFW_KEY_LEFT_CONTROL);

        if (should_reset && should_select && !multi_select) {
            m_hierarchy_selected_nodes.clear();
            m_rail_node_list_selected_nodes.clear();
            m_selected_properties.clear();
            m_properties_render_handler = renderEmptyProperties;
        }

        if (std::holds_alternative<std::shared_ptr<ISceneObject>>(selection)) {
            auto render_obj = std::get<std::shared_ptr<ISceneObject>>(selection);
            if (render_obj) {
                std::string node_uid_str = getNodeUID(render_obj);
                ImGuiID tree_node_id =
                    ImHashStr(node_uid_str.c_str(), node_uid_str.size(), m_window_seed);

                auto node_it = std::find_if(
                    m_hierarchy_selected_nodes.begin(), m_hierarchy_selected_nodes.end(),
                    [&](const SelectionNodeInfo<Object::ISceneObject> &other) {
                        return other.m_node_id == tree_node_id;
                    });

                SelectionNodeInfo<Object::ISceneObject> node_info = {
                    .m_selected = render_obj,
                    .m_node_id =
                        ImHashStr(node_uid_str.c_str(), node_uid_str.size(), m_window_seed),
                    .m_parent_synced = true,
                    .m_scene_synced  = true};  // Only spacial objects get scene selection

                Transform obj_transform = render_obj->getTransform().value();
                // BoundingBox obj_bb      = render_obj->getBoundingBox().value();

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
                /*m_renderer.setGizmoOperation(ImGuizmo::OPERATION::TRANSLATE |
                                             ImGuizmo::OPERATION::ROTATE |
                                             ImGuizmo::OPERATION::SCALE |
                                             ImGuizmo::OPERATION::SCALEU);*/

                if (multi_select) {
                    m_selected_properties.clear();
                    if (node_it == m_hierarchy_selected_nodes.end())
                        m_hierarchy_selected_nodes.push_back(node_info);
                } else {
                    m_hierarchy_selected_nodes.clear();
                    m_hierarchy_selected_nodes.push_back(node_info);
                    for (auto &member : render_obj->getMembers()) {
                        member->syncArray();
                        auto prop = createProperty(member);
                        if (prop) {
                            m_selected_properties.push_back(std::move(prop));
                        }
                    }
                }

                m_properties_render_handler = renderObjectProperties;

                Log::AppLogger::instance().debugLog(std::format(
                    "Hit object {} ({})", render_obj->type(), render_obj->getNameRef().name()));
            }
        } else if (std::holds_alternative<std::shared_ptr<Rail::RailNode>>(selection)) {
            auto node = std::get<std::shared_ptr<Rail::RailNode>>(selection);
            if (node) {
                Rail::Rail *rail = node->rail();

                // In this circumstance, select the whole rail
                if (Input::GetKey(GLFW_KEY_LEFT_ALT)) {
                    SelectionNodeInfo<Rail::Rail> rail_info = {
                        .m_selected = get_shared_ptr<Rail::Rail>(*rail),
                        .m_node_id =
                            ImHashStr(rail->name().data(), rail->name().size(), m_window_seed),
                        .m_parent_synced = true,
                        .m_scene_synced  = false};

                    glm::mat4x4 gizmo_transform =
                        glm::translate(glm::identity<glm::mat4x4>(), rail->getCenteroid());
                    m_renderer.setGizmoTransform(gizmo_transform);
                    m_renderer.setGizmoOperation(ImGuizmo::OPERATION::TRANSLATE |
                                                 ImGuizmo::OPERATION::ROTATE |
                                                 ImGuizmo::OPERATION::SCALE);

                    // Since a rail is selected, we should clear the nodes
                    m_rail_node_list_selected_nodes.clear();

                    if (!multi_select) {
                        m_rail_list_selected_nodes.clear();
                    }

                    m_rail_list_selected_nodes.push_back(rail_info);

                    m_properties_render_handler = renderRailProperties;

                    Log::AppLogger::instance().debugLog(
                        std::format("Hit rail \"{}\"", rail->name()));
                } else {
                    std::string node_name =
                        std::format("Node {}", rail->getNodeIndex(node).value());
                    std::string qual_name = std::string(rail->name()) + "##" + node_name;

                    SelectionNodeInfo<Rail::RailNode> node_info = {
                        .m_selected = node,
                        .m_node_id  = ImHashStr(qual_name.data(), qual_name.size(), m_window_seed),
                        .m_parent_synced = true,
                        .m_scene_synced  = false};

                    glm::mat4x4 gizmo_transform =
                        glm::translate(glm::identity<glm::mat4x4>(), node->getPosition());
                    m_renderer.setGizmoTransform(gizmo_transform);
                    m_renderer.setGizmoOperation(ImGuizmo::OPERATION::TRANSLATE);

                    // Since a node is selected, we should clear the rail selections
                    m_rail_list_selected_nodes.clear();

                    if (!multi_select) {
                        m_rail_node_list_selected_nodes.clear();
                    }

                    m_rail_node_list_selected_nodes.push_back(node_info);

                    m_properties_render_handler = renderRailNodeProperties;

                    Log::AppLogger::instance().debugLog(
                        std::format("Hit node {} of rail \"{}\"", rail->getNodeIndex(node).value(),
                                    rail->name()));
                }
            }
        }

        if (m_hierarchy_selected_nodes.empty() && m_rail_list_selected_nodes.empty() &&
            m_rail_node_list_selected_nodes.empty()) {
            m_renderer.setGizmoVisible(false);
        } else {
            m_renderer.setGizmoVisible(true);
        }

        return inputState;
    }

    bool SceneWindow::postUpdate(f32 delta_time) {
        if (!m_renderer.isGizmoManipulated()) {
            return true;
        }

        // TODO: update all selected objects based on Gizmo
        if (m_hierarchy_selected_nodes.size() == 0) {
            return true;
        }

        glm::mat4x4 gizmo_transform = m_renderer.getGizmoTransform();
        Transform obj_transform;

        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(gizmo_transform), glm::value_ptr(obj_transform.m_translation),
            glm::value_ptr(obj_transform.m_rotation), glm::value_ptr(obj_transform.m_scale));

        // std::shared_ptr<ISceneObject> obj = m_hierarchy_selected_nodes[0].m_selected;
        // BoundingBox obj_old_bb            = obj->getBoundingBox().value();
        // Transform obj_old_transform       = obj->getTransform().value();

        //// Since the translation is for the gizmo, offset it back to the actual transform
        // obj_transform.m_translation = obj_old_transform.m_translation + (translation -
        // obj_old_bb.m_center);

        m_hierarchy_selected_nodes[0].m_selected->setTransform(obj_transform);

        m_update_render_objs = true;
        return true;
    }

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
        ImGuiID tree_node_id = ImHashStr(node_uid_str.c_str(), node_uid_str.size(), m_window_seed);

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
            window.m_renderer.updatePaths(window.m_current_scene->getRailData(),
                                          window.m_rail_visible_map);
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
                std::string uid_str                     = getNodeUID(rail);

                SelectionNodeInfo<Rail::Rail> rail_info = {
                    .m_selected = rail,
                    .m_node_id       = ImHashStr(uid_str.data(), uid_str.size(), m_window_seed),
                    .m_parent_synced = true,
                    .m_scene_synced  = false};

                bool is_rail_selected = std::any_of(
                    m_rail_list_selected_nodes.begin(), m_rail_list_selected_nodes.end(),
                    [&](auto &info) { return info.m_node_id == rail_info.m_node_id; });

                if (!m_rail_visible_map.contains(uid_str)) {
                    m_rail_visible_map[uid_str] = true;
                }
                bool &rail_visibility = m_rail_visible_map[uid_str];
                bool is_rail_visible  = rail_visibility;

                bool is_rail_open = ImGui::TreeNodeEx(uid_str.data(), rail_flags,
                                                      is_rail_selected, &is_rail_visible);

                if (rail_visibility != is_rail_visible) {
                    rail_visibility = is_rail_visible;
                    m_renderer.updatePaths(m_current_scene->getRailData(), m_rail_visible_map);
                    m_update_render_objs = true;
                }

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
                            .m_selected = node,
                            .m_node_id =
                                ImHashStr(qual_name.c_str(), qual_name.size(), m_window_seed),
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

        std::vector<J3DLight> lights;

        // perhaps find a way to limit this so it only happens when we need to re-render?
        if (m_current_scene != nullptr) {
            if (m_update_render_objs) {
                m_renderables.clear();
                auto perform_result = m_current_scene->getObjHierarchy().getRoot()->performScene(
                    delta_time, m_renderables, m_resource_cache, lights);
                if (!perform_result) {
                    const ObjectError &error = perform_result.error();
                    logObjectError(error);
                }
                m_update_render_objs = false;
            } else {
                for (auto &renderable : m_renderables) {
                    renderable.m_model->UpdateAnimations(delta_time);
                }
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
                return;
            });

        m_hierarchy_virtual_node_menu.addDivider();

        m_hierarchy_virtual_node_menu.addOption(
            "Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_rename_obj_dialog.open();
                m_rename_obj_dialog.setOriginalName(info.m_selected->getNameRef().name());
                return;
            });

        m_hierarchy_virtual_node_menu.addDivider();

        m_hierarchy_virtual_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                MainApplication::instance().getSceneObjectClipboard().setData(info);
                return;
            });

        m_hierarchy_virtual_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto nodes = MainApplication::instance().getSceneObjectClipboard().getData();
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    logError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                std::vector<std::string> sibling_names;
                auto children = std::move(this_parent->getChildren().value());
                for (auto &child : children) {
                    sibling_names.push_back(std::string(child->getNameRef().name()));
                }
                for (auto &node : nodes) {
                    std::shared_ptr<ISceneObject> new_object  = make_deep_clone<ISceneObject>(node.m_selected);
                    NameRef node_ref = new_object->getNameRef();
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
            "Delete", {GLFW_KEY_DELETE}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    logError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());
                auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                         m_hierarchy_selected_nodes.end(), info);
                m_hierarchy_selected_nodes.erase(node_it);
                m_update_render_objs = true;
                return;
            });
    }

    void SceneWindow::buildContextMenuGroupObj() {
        m_hierarchy_group_node_menu = ContextMenu<SelectionNodeInfo<Object::ISceneObject>>();

        m_hierarchy_group_node_menu.addOption("Add Child Object...",
                                              {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
                                              [this](SelectionNodeInfo<Object::ISceneObject> info) {
                                                  m_create_obj_dialog.open();
                                                  return;
                                              });

        m_hierarchy_group_node_menu.addDivider();

        m_hierarchy_group_node_menu.addOption("Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R},
                                              [this](SelectionNodeInfo<Object::ISceneObject> info) {
                                                  m_rename_obj_dialog.open();
                                                  m_rename_obj_dialog.setOriginalName(
                                                      info.m_selected->getNameRef().name());
                                                  return;
                                              });

        m_hierarchy_group_node_menu.addDivider();

        m_hierarchy_group_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                MainApplication::instance().getSceneObjectClipboard().setData(info);
                return;
            });

        m_hierarchy_group_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto nodes       = MainApplication::instance().getSceneObjectClipboard().getData();
                auto this_parent = std::reinterpret_pointer_cast<GroupSceneObject>(info.m_selected);
                if (!this_parent) {
                    logError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                std::vector<std::string> sibling_names;
                auto children = std::move(this_parent->getChildren().value());
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
            "Delete", {GLFW_KEY_DELETE}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    logError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());
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
            "Insert Object Here...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_create_obj_dialog.open();
                return;
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                m_rename_obj_dialog.open();
                m_rename_obj_dialog.setOriginalName(info.m_selected->getNameRef().name());
                return;
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "View in Scene", {GLFW_KEY_LEFT_ALT, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto member_result = info.m_selected->getMember("Transform");
                if (!member_result) {
                    logError(make_error<void>("Scene Hierarchy",
                                              "Failed to find transform member of physical object")
                                 .error());
                    return;
                }
                auto member_ptr = member_result.value();
                if (!member_ptr) {
                    logError(make_error<void>("Scene Hierarchy",
                                              "Found the transform member but it was null")
                                 .error());
                    return;
                }
                Transform transform = getMetaValue<Transform>(member_ptr).value();
                f32 max_scale       = std::max(transform.m_scale.x, transform.m_scale.y);
                max_scale           = std::max(max_scale, transform.m_scale.z);

                m_renderer.setCameraOrientation({0, 1, 0}, transform.m_translation,
                                                {transform.m_translation.x,
                                                 transform.m_translation.y,
                                                 transform.m_translation.z + 1000 * max_scale});
                m_update_render_objs = true;
                return;
            });

        m_hierarchy_physical_node_menu.addOption(
            "Move to Camera", {GLFW_KEY_LEFT_ALT, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto member_result = info.m_selected->getMember("Transform");
                if (!member_result) {
                    logError(make_error<void>("Scene Hierarchy",
                                              "Failed to find transform member of physical object")
                                 .error());
                    return;
                }
                auto member_ptr = member_result.value();
                if (!member_ptr) {
                    logError(make_error<void>("Scene Hierarchy",
                                              "Found the transform member but it was null")
                                 .error());
                    return;
                }

                Transform transform = getMetaValue<Transform>(member_ptr).value();
                m_renderer.getCameraTranslation(transform.m_translation);
                auto result = setMetaValue<Transform>(member_ptr, 0, transform);
                if (!result) {
                    logError(
                        make_error<void>("Scene Hierarchy",
                                         "Failed to set the transform member of physical object")
                            .error());
                    return;
                }

                m_update_render_objs = true;
                return;
            });

        m_hierarchy_physical_node_menu.addDivider();

        m_hierarchy_physical_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                info.m_selected = make_deep_clone<ISceneObject>(info.m_selected);
                MainApplication::instance().getSceneObjectClipboard().setData(info);
                return;
            });

        m_hierarchy_physical_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto nodes = MainApplication::instance().getSceneObjectClipboard().getData();
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    logError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                std::vector<std::string> sibling_names;
                auto children = std::move(this_parent->getChildren().value());
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
            "Delete", {GLFW_KEY_DELETE}, [this](SelectionNodeInfo<Object::ISceneObject> info) {
                auto this_parent =
                    reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                if (!this_parent) {
                    logError(
                        make_error<void>("Scene Hierarchy", "Failed to get parent node for pasting")
                            .error());
                    return;
                }
                this_parent->removeChild(info.m_selected->getNameRef().name());
                auto node_it = std::find(m_hierarchy_selected_nodes.begin(),
                                         m_hierarchy_selected_nodes.end(), info);
                m_hierarchy_selected_nodes.erase(node_it);
                m_update_render_objs = true;
                return;
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
                return;
            });

        m_hierarchy_multi_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
                auto nodes = MainApplication::instance().getSceneObjectClipboard().getData();
                for (auto &info : infos) {
                    auto this_parent =
                        reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                    if (!this_parent) {
                        logError(make_error<void>("Scene Hierarchy",
                                                  "Failed to get parent node for pasting")
                                     .error());
                        return;
                    }
                    std::vector<std::string> sibling_names;
                    auto children = std::move(this_parent->getChildren().value());
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
            "Delete", {GLFW_KEY_DELETE},
            [this](std::vector<SelectionNodeInfo<Object::ISceneObject>> infos) {
                for (auto &info : infos) {
                    auto this_parent =
                        reinterpret_cast<GroupSceneObject *>(info.m_selected->getParent());
                    if (!this_parent) {
                        logError(make_error<void>("Scene Hierarchy",
                                                  "Failed to get parent node for pasting")
                                     .error());
                        return;
                    }
                    this_parent->removeChild(info.m_selected->getNameRef().name());
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
                                               {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_N},
                                               [this](SelectionNodeInfo<Rail::Rail> info) {
                                                   m_create_rail_dialog.open();
                                                   return;
                                               });

        m_rail_list_single_node_menu.addDivider();

        m_rail_list_single_node_menu.addOption("Rename...", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_R},
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
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Rail::Rail> info) {
                info.m_selected = make_deep_clone<Rail::Rail>(info.m_selected);
                MainApplication::instance().getSceneRailClipboard().setData(info);
                return;
            });

        m_rail_list_single_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Rail::Rail> info) {
                auto nodes = MainApplication::instance().getSceneRailClipboard().getData();
                if (nodes.size() > 0) {
                    RailData data         = m_current_scene->getRailData();
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
                    m_current_scene->setRailData(data);
                }
                m_update_render_objs = true;
                return;
            });

        m_rail_list_single_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE}, [this](SelectionNodeInfo<Rail::Rail> info) {
                std::string uid_str = getNodeUID(info.m_selected);
                m_rail_visible_map.erase(uid_str);
                RailData data = m_current_scene->getRailData();
                data.removeRail(*info.m_selected);
                m_current_scene->setRailData(data);
                m_update_render_objs = true;
                return;
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
                return;
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
                    m_current_scene->setRailData(data);
                }
                m_update_render_objs = true;
                return;
            });

        m_rail_list_multi_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE}, [this](std::vector<SelectionNodeInfo<Rail::Rail>> info) {
                RailData data = m_current_scene->getRailData();
                for (auto &select : info) {
                    m_rail_visible_map.erase(select.m_selected->name());
                    data.removeRail(select.m_selected->name());
                }
                m_current_scene->setRailData(data);
                m_update_render_objs = true;
                return;
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
            "View in Scene", {GLFW_KEY_LEFT_ALT, GLFW_KEY_V},
            [this](SelectionNodeInfo<Rail::RailNode> info) {
                Rail::Rail *rail      = info.m_selected->rail();
                glm::vec3 translation = info.m_selected->getPosition();

                m_renderer.setCameraOrientation(
                    {0, 1, 0}, translation, {translation.x, translation.y, translation.z + 1000});
                m_update_render_objs = true;
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_single_node_menu.addOption(
            "Move to Camera", {GLFW_KEY_LEFT_ALT, GLFW_KEY_C},
            [this](SelectionNodeInfo<Rail::RailNode> info) {
                Rail::Rail *rail = info.m_selected->rail();

                glm::vec3 translation;
                m_renderer.getCameraTranslation(translation);
                auto result = rail->setNodePosition(info.m_selected, translation);
                if (!result) {
                    logMetaError(result.error());
                    return;
                }

                m_renderer.updatePaths(m_current_scene->getRailData(), m_rail_visible_map);
                m_update_render_objs = true;
                return;
            });

        m_rail_node_list_single_node_menu.addDivider();

        m_rail_node_list_single_node_menu.addOption(
            "Copy", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_C},
            [this](SelectionNodeInfo<Rail::RailNode> info) {
                info.m_selected = make_deep_clone<Rail::RailNode>(info.m_selected);
                MainApplication::instance().getSceneRailNodeClipboard().setData(info);
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_single_node_menu.addOption(
            "Paste", {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_V},
            [this](SelectionNodeInfo<Rail::RailNode> info) {
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
                m_update_render_objs = true;
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_single_node_menu.addOption("Delete", {GLFW_KEY_DELETE},
                                                    [this](SelectionNodeInfo<Rail::RailNode> info) {
                                                        Rail::Rail *rail = info.m_selected->rail();
                                                        rail->removeNode(info.m_selected);
                                                        m_update_render_objs = true;
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
                m_update_render_objs = true;
                return std::expected<void, BaseError>();
            });

        m_rail_node_list_multi_node_menu.addOption(
            "Delete", {GLFW_KEY_DELETE},
            [this](std::vector<SelectionNodeInfo<Rail::RailNode>> info) {
                for (auto &select : info) {
                    Rail::Rail *rail = select.m_selected->rail();
                    rail->removeNode(select.m_selected);
                }
                m_update_render_objs = true;
                return std::expected<void, BaseError>();
            });
    }

    void SceneWindow::buildCreateObjDialog() {
        m_create_obj_dialog.setup();
        m_create_obj_dialog.setActionOnAccept(
            [this](size_t sibling_index, std::string_view name, const Object::Template &template_,
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

                auto result = this_parent->insertChild(sibling_index, std::move(new_object_result));
                if (!result) {
                    logObjectGroupError(result.error());
                    return;
                }

                m_update_render_objs = true;
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

            Rail::Rail new_rail(name, new_nodes);

            for (u16 i = 0; i < node_count; ++i) {
                auto result = new_rail.connectNodeToNeighbors(i, true);
                if (!result) {
                    logMetaError(result.error());
                }
            }

            if (!loop) {
                // First
                {
                    auto result = new_rail.removeConnection(0, 1);
                    if (!result) {
                        logMetaError(result.error());
                    }
                }

                // Last
                {
                    auto result = new_rail.removeConnection(node_count - 1, 1);
                    if (!result) {
                        logMetaError(result.error());
                    }
                }
            }

            rail_data.addRail(new_rail);

            m_current_scene->setRailData(rail_data);
            m_renderer.updatePaths(rail_data, m_rail_visible_map);
            m_update_render_objs = true;
        });
    }

    void SceneWindow::buildRenameRailDialog() {
        m_rename_rail_dialog.setup();
        m_rename_rail_dialog.setActionOnAccept([this](std::string_view new_name,
                                                      SelectionNodeInfo<Rail::Rail> info) {
            if (new_name.empty()) {
                auto result =
                    make_error<void>("Scene Hierarchy", "Can not rename rail to empty string");
                logError(result.error());
                return;
            }
            m_rail_visible_map[std::string(new_name)] = m_rail_visible_map[info.m_selected->name()];
            m_rail_visible_map.erase(info.m_selected->name());
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

    void SceneWindow::renderMenuBar() {
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
            ImGui::EndMenuBar();
        }

        if (m_is_save_as_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog("SaveSceneDialog", "Choose Directory", nullptr,
                                                    "", "");
        }

        if (ImGuiFileDialog::Instance()->Display("SaveSceneDialog")) {
            ImGuiFileDialog::Instance()->Close();
            m_is_save_as_dialog_open = false;

            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                auto dir_result = Toolbox::is_directory(path);
                if (!dir_result) {
                    return;
                }

                if (!dir_result.value()) {
                    return;
                }

                auto result = m_current_scene->saveToPath(path);
                if (!result) {
                    logSerialError(result.error());
                }
            }
        }
    }

};  // namespace Toolbox::UI