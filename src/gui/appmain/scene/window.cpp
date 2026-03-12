#if defined(_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cmath>
#include <execution>
#include <vector>

#include "core/input/input.hpp"
#include "core/log.hpp"
#include "core/threaded.hpp"
#include "core/timing.hpp"

#include "model/objmodel.hpp"

#include "IconsForkAwesome.h"
#include "gui/appmain/application.hpp"
#include "gui/appmain/project/events.hpp"
#include "gui/appmain/scene/events.hpp"
#include "gui/appmain/scene/window.hpp"
#include "gui/appmain/settings/settings.hpp"
#include "gui/font.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/logging/errors.hpp"
#include "gui/modelcache.hpp"
#include "gui/util.hpp"

#include "gui/imgui_ext.hpp"

#include "platform/capture.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if WIN32
#include <windows.h>
#endif
#include "gui/context_menu.hpp"

#include <J3D/Material/J3DMaterialTableLoader.hpp>
#include <J3D/Material/J3DUniformBufferObject.hpp>

#include <glm/gtx/euler_angles.hpp>

using namespace Toolbox;
using namespace Toolbox::Scene;

namespace Toolbox::UI {

    static std::unordered_set<std::string> s_game_blacklist = {"Map", "Sky"};

    static std::string getNodeUID(RefPtr<Toolbox::Object::ISceneObject> node) {
        std::string node_name =
            std::format("{} ({}) [{:08X}]###{}", node->type(), node->getNameRef().name(),
                        node->getGamePtr(), node->getUUID());
        return node_name;
    }

    static std::string getNodeUID(RailData::rail_ptr_t rail) {
        return std::format("{}###{}", rail->name(), rail->getUUID());
    }

    static std::string getNodeUID(RailData::rail_ptr_t rail, size_t node_index) {
        if (node_index >= rail->nodes().size()) {
            return std::format("(orphan)###{}", node_index);
        }
        return std::format("Node {}###{}", node_index, rail->nodes()[node_index]->getUUID());
    }

    static bool isSceneObjectFiltered(RefPtr<SceneObjModel> model, const std::string &filter,
                                      std::string type, std::string name) {
        std::transform(type.begin(), type.end(), type.begin(),
                       [](char c) { return ::tolower(static_cast<int>(c)); });
        std::transform(name.begin(), name.end(), name.begin(),
                       [](char c) { return ::tolower(static_cast<int>(c)); });
        return !type.starts_with(filter) && !name.starts_with(filter);
    }

    SceneCreateRailEvent::SceneCreateRailEvent(const UUID64 &target_id, const Rail::Rail &rail)
        : BaseEvent(target_id, SCENE_CREATE_RAIL_EVENT), m_rail(rail) {}

    ScopePtr<ISmartResource> SceneCreateRailEvent::clone(bool deep) const {
        return make_scoped<SceneCreateRailEvent>(*this);
    }

    SceneWindow::SceneWindow(const std::string &name) : ImWindow(name) {}

    bool SceneWindow::onLoadData(const fs_path &path) {
        if (!Toolbox::Filesystem::exists(path)) {
            return false;
        }

        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();

        const bool include_custom_objs = MainApplication::instance()
                                             .getSettingsManager()
                                             .getCurrentProfile()
                                             .m_is_custom_obj_allowed;

        if (Toolbox::Filesystem::is_directory(path)) {
            if (path.filename() != "scene") {
                return false;
            }

            SceneInstance::FromPath(path, include_custom_objs)
                .and_then([&](ScopePtr<SceneInstance> &&scene) {
                    m_current_scene = std::move(scene);

                    m_resource_cache.m_model.clear();
                    m_resource_cache.m_material.clear();

                    // Initialize the rail visibility map
                    for (RailData::rail_ptr_t rail : m_current_scene->getRailData()->rails()) {
                        m_rail_visible_map[rail->getUUID()] = true;
                    }

                    if (task_communicator.isSceneLoaded(m_stage, m_scenario)) {
                        reassignAllActorPtrs(0);
                    } else if (task_communicator.isSceneLoaded()) {
                        u8 area, episode;
                        task_communicator.getLoadedScene(area, episode);
                        TOOLBOX_WARN_V("[SCENE] Editor scene is <area: {}, episode {}> but game "
                                       "scene is <area: {}, episode {}>",
                                       m_stage, m_scenario, area, episode);
                    }

                    return Result<void, SerialError>();
                })
                .or_else([](const SerialError &error) {
                    LogError(error);
                    return Result<void, SerialError>();
                });

            if (m_current_scene) {
                m_io_context_path = path;

                m_scene_object_model = make_referable<SceneObjModel>();
                m_table_object_model = make_referable<SceneObjModel>();
                m_rail_model         = make_referable<RailObjModel>();

                m_scene_object_model->initialize(*m_current_scene->getObjHierarchy());
                m_table_object_model->initialize(*m_current_scene->getTableHierarchy());
                m_rail_model->initialize(*m_current_scene->getRailData());

                m_scene_selection_mgr = ModelSelectionManager(m_scene_object_model);
                m_table_selection_mgr = ModelSelectionManager(m_table_object_model);
                m_rail_selection_mgr  = ModelSelectionManager(m_rail_model);

                m_scene_selection_mgr.setDeepSpans(false);
                m_table_selection_mgr.setDeepSpans(false);
                m_rail_selection_mgr.setDeepSpans(false);

                m_renderer.initializeData(m_rail_model);
                return true;
            }

            return false;
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool SceneWindow::onSaveData(std::optional<fs_path> path) {
        fs_path root_path;

        if (!m_current_scene) {
            TOOLBOX_ERROR("(SCENE) Failed to save the scene due to lack of a scene instance.");
            return false;
        }

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

        const AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        if (settings.m_is_file_backup_allowed) {
            const fs_path src_path = root_path.parent_path();
            const fs_path dst_path = fs_path(src_path).replace_extension(".bak");
            auto result            = Filesystem::copy(src_path, dst_path,
                                                      Filesystem::copy_options::recursive |
                                                          Filesystem::copy_options::overwrite_existing);
            if (!result) {
                LogError(result.error());
            }
        }

        auto result = m_current_scene->saveToPath(root_path);
        if (!result) {
            LogError(result.error());
            return false;
        }

        const AppSettings &cur_settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();
        if (cur_settings.m_repack_scenes_on_save && m_current_scene->rootPath().has_value()) {
            m_repack_io_busy = true;

            MainApplication::instance().dispatchEvent<ProjectPackEvent, true>(
                0, m_current_scene->rootPath().value().parent_path(), true,
                [&]() { m_repack_io_busy = false; });
        }

        return true;
    }

    void SceneWindow::onAttach() {
        m_properties_render_handler = renderEmptyProperties;

        buildContextMenuSceneObj();
        buildContextMenuRail();

        buildCreateObjDialog();
        buildRenameObjDialog();

        buildCreateRailDialog();
        buildRenameRailDialog();
    }

    void SceneWindow::onDetach() {
        if (unsaved()) {
            TOOLBOX_WARN_V("[SCENE_WINDOW] Scene closed with unsaved changes ({}).", context());
        }

        m_hierarchy_filter.Clear();

        m_scene_selection_mgr.getState().clearSelection();
        m_table_selection_mgr.getState().clearSelection();
        m_rail_selection_mgr.getState().clearSelection();

        m_selected_properties.clear();
        m_properties_render_handler = renderEmptyProperties;

        m_renderables.clear();
        m_resource_cache = {};

        m_rail_visible_map.clear();

        m_drop_target_buffer.free();
        m_current_scene.reset();
    }

    void SceneWindow::onImGuiUpdate(TimeStep delta_time) {
        m_renderer.inputUpdate(delta_time);

        calcDolphinVPMatrix();

        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();

        if (task_communicator.isSceneLoaded()) {
            if (Input::GetKeyDown(KeyCode::KEY_E)) {
                m_is_game_edit_mode ^= true;
            }
        } else {
            m_is_game_edit_mode = false;
        }

        return;
    }

    void SceneWindow::onImGuiPostUpdate(TimeStep delta_time) {
        const AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        if (m_current_scene) {
            std::vector<RefPtr<Rail::RailNode>> rendered_nodes;
            for (auto &rail : m_current_scene->getRailData()->rails()) {
                if (!m_rail_visible_map[rail->getUUID()])
                    continue;
                rendered_nodes.insert(rendered_nodes.end(), rail->nodes().begin(),
                                      rail->nodes().end());
            }

            if (m_is_render_window_open && (!m_renderer.getGizmoVisible() || !ImGuizmo::IsOver())) {
                bool should_reset = false;
                Renderer::selection_variant_t selection =
                    m_renderer.findSelection(m_renderables, rendered_nodes, should_reset);

                bool multi_select = Input::GetKey(KeyCode::KEY_LEFTCONTROL);

                if (should_reset && !multi_select) {
                    m_scene_selection_mgr.getState().clearSelection();
                    m_table_selection_mgr.getState().clearSelection();
                    m_rail_selection_mgr.getState().clearSelection();

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
                        RefPtr<RailData> rail_data = m_current_scene->getRailData();

                        RailData::rail_ptr_t rail = rail_data->getRail(node->getRailUUID());
                        if (!rail) {
                            TOOLBOX_ERROR("Failed to find rail for node.");
                            return;
                        }

                        processRailSelection(rail, multi_select);
                    } else {
                        processRailNodeSelection(node, multi_select);
                    }
                }

                // Check if all selected objects have transforms for the gizmo to manipulate
                const bool any_valid_object_selected = [this]() {
                    if (m_scene_selection_mgr.getState().count() > 0) {
                        const IDataModel::index_container &indexes =
                            m_scene_selection_mgr.getState().getSelection();
                        std::all_of(indexes.begin(), indexes.end(), [this](const ModelIndex &node) {
                            RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(node);
                            return obj && obj->getTransform().has_value();
                        });
                        return true;
                    }
                    if (m_table_selection_mgr.getState().count() > 0) {
                        const IDataModel::index_container &indexes =
                            m_table_selection_mgr.getState().getSelection();
                        std::all_of(indexes.begin(), indexes.end(), [this](const ModelIndex &node) {
                            RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(node);
                            return obj && obj->getTransform().has_value();
                        });
                        return true;
                    }
                    return false;
                }();

                const bool any_valid_rail_or_node_selected = [this]() {
                    return m_rail_selection_mgr.getState().count() > 0;
                }();

                m_renderer.setGizmoVisible(any_valid_object_selected ||
                                           any_valid_rail_or_node_selected);
            }
        }

        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();

        if (m_selection_transforms_needs_update) {
            calcNewGizmoMatrixFromSelection();
            m_selection_transforms_needs_update = false;
        }

        bool should_update_paths =
            m_renderer.isUniqueRailColors() != settings.m_is_unique_rail_color;

        if (m_renderer.isGizmoManipulated()) {
            glm::mat4x4 gizmo_total_delta = m_renderer.getGizmoTotalDelta();

            // Scene object transform manipulation
            {
                const IDataModel::index_container &obj_indexes =
                    m_scene_selection_mgr.getState().getSelection();

                size_t i = 0;
                for (const ModelIndex &index : obj_indexes) {
                    RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(index);
                    if (!obj || !obj->getTransform()) {
                        continue;
                    }
                    const Transform &obj_transform = m_selection_transforms[i];
                    Transform new_transform        = gizmo_total_delta * obj_transform;
                    obj->setTransform(new_transform);
                    i++;
                }
            }

            // Table object transform manipulation
            {
                const IDataModel::index_container &obj_indexes =
                    m_table_selection_mgr.getState().getSelection();

                size_t i = 0;
                for (const ModelIndex &index : obj_indexes) {
                    RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(index);
                    if (!obj || !obj->getTransform()) {
                        continue;
                    }
                    const Transform &obj_transform = m_selection_transforms[i];
                    Transform new_transform        = gizmo_total_delta * obj_transform;
                    obj->setTransform(new_transform);
                    i++;
                }
            }

            {
                const IDataModel::index_container &rail_indexes =
                    m_rail_selection_mgr.getState().getSelection();

                glm::mat4x4 gizmo_delta   = m_renderer.getGizmoFrameDelta();
                Transform delta_transform = Transform::FromMat4x4(gizmo_delta);

                size_t i = 0;
                for (const ModelIndex &index : rail_indexes) {
                    RailData::rail_ptr_t rail = m_rail_model->getRailRef(index);

                    const bool is_node = m_rail_model->isIndexRailNode(index);
                    if (is_node) {
                        Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                        const Transform &node_transform = m_selection_transforms[i];
                        Transform new_transform         = gizmo_total_delta * node_transform;

                        rail->setNodePosition(node, new_transform.m_translation);
                    } else {
                        const Transform &rail_transform = m_selection_transforms[i];
                        rail->transform(gizmo_delta);
                    }

                    i++;
                }

                should_update_paths = true;
            }

            m_gizmo_maniped = true;
        }

        if (should_update_paths) {
            m_renderer.setUniqueRailColors(settings.m_is_unique_rail_color);
            m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
        }

        // Refresh the selection transforms so new gizmo manips don't reset
        if (!m_renderer.isGizmoActive() && m_gizmo_maniped) {
            m_selection_transforms_needs_update = true;
            m_gizmo_maniped                     = false;
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
                if (m_current_scene) {
                    const Rail::Rail &rail = event->getRail();
                    m_current_scene->getRailData()->addRail(rail);
                    m_rail_visible_map[rail.getUUID()] = true;
                    ev->accept();
                } else {
                    TOOLBOX_ERROR("Failed to create rail due to lack of a scene instance.");
                    ev->ignore();
                }
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

    void SceneWindow::onRenderBody(TimeStep deltaTime) {
        renderHierarchy();
        renderProperties();
        renderRailEditor();
        renderScene(deltaTime);
        renderDolphin(deltaTime);
        renderSanitizationSteps();

        m_scene_hierarchy_context_menu.applyDeferredCmds();
        m_table_hierarchy_context_menu.applyDeferredCmds();
        m_rail_list_context_menu.applyDeferredCmds();
    }

    void SceneWindow::renderSanitizationSteps() {
        const ImGuiStyle &style = ImGui::GetStyle();

        if (m_scene_verifier) {
            if (m_scene_verifier->tIsAlive()) {
                ImGui::OpenPopup("Scene Validator");

                ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(),
                                        ImGuiCond_Appearing, {0.5f, 0.5f});

                if (ImGui::BeginPopupModal("Scene Validator", nullptr, ImGuiWindowFlags_NoResize)) {
                    ImGui::Text("Validating scene, please wait...");
                    ImGui::Separator();

                    float progress            = m_scene_verifier->getProgress();
                    std::string progress_text = m_scene_verifier->getProgressText();

                    ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f), progress_text.c_str());
                    ImGui::EndPopup();
                }
            } else if (m_scene_verifier->tIsKilled()) {
                if (!m_scene_validator_result_opened) {
                    if (m_scene_verifier->isValid()) {
                        MainApplication::instance().showSuccessModal(
                            this, "Scene Validator Result",
                            "Scene validation completed successfully!");
                    } else {
                        std::vector<std::string> errors = m_scene_verifier->getErrors();
                        MainApplication::instance().showErrorModal(
                            this, "Scene Validator Result", "Scene validation failed with errors!",
                            errors);
                    }
                    m_scene_validator_result_opened = true;
                }
            }
        }

        if (m_scene_mender) {
            if (m_scene_mender->tIsAlive()) {
                ImGui::OpenPopup("Scene Repair");

                ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(),
                                        ImGuiCond_Appearing, {0.5f, 0.5f});

                if (ImGui::BeginPopupModal("Scene Repair", nullptr, ImGuiWindowFlags_NoResize)) {
                    ImGui::Text("Validating scene, please wait...");
                    ImGui::Separator();

                    std::string progress_text = m_scene_mender->getProgressText();

                    ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-FLT_MIN, 0.0f),
                                       progress_text.c_str());
                    ImGui::EndPopup();
                }
            } else if (m_scene_mender->tIsKilled()) {
                if (!m_scene_mender_result_opened) {
                    if (m_scene_mender->isValid()) {
                        std::vector<std::string> changes = m_scene_mender->getChanges();
                        MainApplication::instance().showSuccessModal(
                            this, "Scene Repair Result",
                            changes.empty() ? "The scene was already valid!"
                                            : "Scene repair completed successfully!",
                            changes);
                    } else {
                        std::vector<std::string> errors = m_scene_mender->getErrors();
                        MainApplication::instance().showErrorModal(
                            this, "Scene Repair Result", "Scene repair failed with errors!",
                            errors);
                    }
                    m_scene_mender_result_opened = true;
                }
            }
        }
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

            if (m_current_scene) {
                ModelIndex root_index = m_scene_object_model->getIndex(0, 0, ModelIndex());
                renderSceneObjectTree(root_index);
            }

            ImGui::Spacing();
            ImGui::Text("Scene Info");
            ImGui::Separator();

            if (m_current_scene) {
                ModelIndex root_index = m_table_object_model->getIndex(0, 0, ModelIndex());
                renderTableObjectTree(root_index);
            }
        }
        ImGui::End();

        // Scene dialogs
        {
            const ModelIndex &last_selected = m_scene_selection_mgr.getState().getLastSelected();
            if (m_scene_object_model->validateIndex(last_selected)) {
                m_create_scene_obj_dialog.render(m_scene_object_model, last_selected);
                m_rename_scene_obj_dialog.render(last_selected);
            }
        }

        // Table dialogs
        {
            const ModelIndex &last_selected = m_table_selection_mgr.getState().getLastSelected();
            if (m_table_object_model->validateIndex(last_selected)) {
                m_create_table_obj_dialog.render(m_table_object_model, last_selected);
                m_rename_table_obj_dialog.render(last_selected);
            }
        }
    }

    void SceneWindow::renderSceneObjectTree(const ModelIndex &index) {
        constexpr auto dir_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanFullWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;

        constexpr auto file_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth |
                                    ImGuiTreeNodeFlags_FramePadding |
                                    ImGuiTreeNodeFlags_DefaultOpen;

        RefPtr<ISceneObject> node = m_scene_object_model->getObjectRef(index);

        bool multi_select     = Input::GetKey(KeyCode::KEY_LEFTCONTROL);
        bool needs_scene_sync = node->getTransform() ? false : true;

        std::string display_name = std::format("{} ({})", node->type(), node->getNameRef().name());
        bool is_filtered_out     = !m_hierarchy_filter.PassFilter(display_name.c_str());

        std::string node_uid_str = getNodeUID(node);
        ImGuiID tree_node_id     = static_cast<ImGuiID>(node->getUUID());

        const bool node_selected = m_scene_selection_mgr.getState().isSelected(index);

        bool node_visible    = node->getIsPerforming();
        bool node_visibility = node->getCanPerform();

        bool node_open = false;

        ImGuiTreeNodeFlags the_flags = node->isGroupObject() ? dir_flags : file_flags;
        // the_flags |= node_selected ? ImGuiTreeNodeFlags_Framed : 0;

        // const bool is_cut = std::find(m_cut_indices.begin(), m_cut_indices.end(), child_index) !=
        //                     m_cut_indices.end();
        const bool is_cut = false;

        ImGui::PushID(tree_node_id);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 4.0f});

        ImRect node_rect;

        if (node->isGroupObject()) {
            if (is_filtered_out) {
                for (size_t i = 0; i < m_scene_object_model->getRowCount(index); ++i) {
                    ModelIndex child_index = m_scene_object_model->getIndex(i, 0, index);
                    renderSceneObjectTree(child_index);
                }
            } else {

                if (node_visibility) {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected);
                }

                node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

                // Drag and drop for OBJECT
                {
                    ImVec2 mouse_pos = ImGui::GetMousePos();
                    ImVec2 item_size = ImGui::GetItemRectSize();
                    ImVec2 item_pos  = ImGui::GetItemRectMin();

                    if (ImGui::BeginDragDropSource()) {
                        ScopePtr<MimeData> mime_data = m_scene_selection_mgr.actionCopySelection();
                        if (mime_data) {
                            std::optional<Buffer> buffer =
                                mime_data->get_data("toolbox/scene/object");
                            if (buffer) {
                                ImGui::SetDragDropPayload("toolbox/scene/object", buffer->buf(),
                                                          buffer->size(), ImGuiCond_Once);
                                ImGui::Text("Object: %s", node->getNameRef().name().data());
                                ImGui::EndDragDropSource();
                            }
                        }
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

                                int64_t node_index = m_scene_object_model->getRow(index);

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
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (ImGui::IsItemHovered()) {
                    m_scene_selection_mgr.handleActionsByMouseInput(index, true);

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                        ImGui::FocusWindow(ImGui::GetCurrentWindow());

                        m_selected_properties.clear();
                        m_rail_selection_mgr.getState().clearSelection();

                        if (!Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) &&
                            !Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
                            m_table_selection_mgr.getState().clearSelection();
                        }

                        if (m_scene_selection_mgr.getState().getSelection().size() == 1) {
                            for (auto &member : node->getMembers()) {
                                member->syncArray();
                                auto prop = createProperty(member);
                                if (prop) {
                                    m_selected_properties.push_back(std::move(prop));
                                }
                            }
                        }

                        m_selection_transforms_needs_update = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderSceneHierarchyContextMenu(node_uid_str, index);

                if (node_open) {
                    for (size_t i = 0; i < m_scene_object_model->getRowCount(index); ++i) {
                        ModelIndex child_index = m_scene_object_model->getIndex(i, 0, index);
                        renderSceneObjectTree(child_index);
                    }
                    ImGui::TreePop();
                }
            }
        } else {
            if (!is_filtered_out) {
                if (node_visibility) {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected);
                }

                node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

                // Drag and drop for OBJECT
                {
                    ImVec2 mouse_pos = ImGui::GetMousePos();
                    ImVec2 item_size = ImGui::GetItemRectSize();
                    ImVec2 item_pos  = ImGui::GetItemRectMin();

                    if (ImGui::BeginDragDropSource()) {
                        int64_t node_index = m_scene_object_model->getRow(index);

                        Toolbox::Buffer buffer;
                        saveMimeObject(buffer, node_index, get_shared_ptr(*node->getParent()));
                        ImGui::SetDragDropPayload("toolbox/scene/object", buffer.buf(),
                                                  buffer.size(), ImGuiCond_Once);
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

                                int64_t node_index = m_scene_object_model->getRow(index);

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

                if (ImGui::IsItemHovered()) {
                    m_scene_selection_mgr.handleActionsByMouseInput(index, true);

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                        ImGui::FocusWindow(ImGui::GetCurrentWindow());

                        m_selected_properties.clear();
                        m_rail_selection_mgr.getState().clearSelection();

                        if (!Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) &&
                            !Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
                            m_table_selection_mgr.getState().clearSelection();
                        }

                        if (m_scene_selection_mgr.getState().getSelection().size() == 1) {
                            for (auto &member : node->getMembers()) {
                                member->syncArray();
                                auto prop = createProperty(member);
                                if (prop) {
                                    m_selected_properties.push_back(std::move(prop));
                                }
                            }
                        }

                        m_selection_transforms_needs_update = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderSceneHierarchyContextMenu(node_uid_str, index);

                if (node_open) {
                    ImGui::TreePop();
                }
            }
        }

        const ImGuiStyle &style = ImGui::GetStyle();

        const float render_alpha = is_cut ? 0.5f : 1.0f;
        ImVec4 col               = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        col.w *= render_alpha;

        if (index == m_scene_selection_mgr.getState().getLastSelected()) {
            ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            col.w *= render_alpha;
            ImGui::GetWindowDrawList()->AddRect(node_rect.Min - (style.FramePadding / 2.0f),
                                                node_rect.Max + (style.FramePadding / 2.0f),
                                                ImGui::ColorConvertFloat4ToU32(col), 0.0f,
                                                ImDrawFlags_RoundCornersAll, 2.0f);
        }

        ImGui::PopStyleVar();

        ImGui::PopID();
    }

    void SceneWindow::renderTableObjectTree(const ModelIndex &index) {
        constexpr auto dir_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanFullWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;

        constexpr auto file_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth |
                                    ImGuiTreeNodeFlags_FramePadding |
                                    ImGuiTreeNodeFlags_DefaultOpen;

        RefPtr<ISceneObject> node = m_table_object_model->getObjectRef(index);

        bool multi_select     = Input::GetKey(KeyCode::KEY_LEFTCONTROL);
        bool needs_scene_sync = node->getTransform() ? false : true;

        std::string display_name = std::format("{} ({})", node->type(), node->getNameRef().name());
        bool is_filtered_out     = !m_hierarchy_filter.PassFilter(display_name.c_str());

        std::string node_uid_str = getNodeUID(node);
        ImGuiID tree_node_id     = static_cast<ImGuiID>(node->getUUID());

        const bool node_selected = m_table_selection_mgr.getState().isSelected(index);

        bool node_visible    = node->getIsPerforming();
        bool node_visibility = node->getCanPerform();

        bool node_open = false;

        ImGuiTreeNodeFlags the_flags = node->isGroupObject() ? dir_flags : file_flags;
        // the_flags |= node_selected ? ImGuiTreeNodeFlags_Framed : 0;

        // const bool is_cut = std::find(m_cut_indices.begin(), m_cut_indices.end(), child_index) !=
        //                     m_cut_indices.end();
        const bool is_cut = false;

        ImGui::PushID(tree_node_id);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 4.0f});

        ImRect node_rect;

        if (node->isGroupObject()) {
            if (is_filtered_out) {
                for (size_t i = 0; i < m_table_object_model->getRowCount(index); ++i) {
                    ModelIndex child_index = m_table_object_model->getIndex(i, 0, index);
                    renderTableObjectTree(child_index);
                }
            } else {
                if (node_visibility) {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected);
                }

                node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

                // Drag and drop for OBJECT
                {
                    ImVec2 mouse_pos = ImGui::GetMousePos();
                    ImVec2 item_size = ImGui::GetItemRectSize();
                    ImVec2 item_pos  = ImGui::GetItemRectMin();

                    if (ImGui::BeginDragDropSource()) {
                        ScopePtr<MimeData> mime_data = m_table_selection_mgr.actionCopySelection();
                        if (mime_data) {
                            std::optional<Buffer> buffer =
                                mime_data->get_data("toolbox/scene/object");
                            if (buffer) {
                                ImGui::SetDragDropPayload("toolbox/scene/object", buffer->buf(),
                                                          buffer->size(), ImGuiCond_Once);
                                ImGui::Text("Object: %s", node->getNameRef().name().data());
                                ImGui::EndDragDropSource();
                            }
                        }
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

                                int64_t node_index = m_table_object_model->getRow(index);

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
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (ImGui::IsItemHovered()) {
                    m_table_selection_mgr.handleActionsByMouseInput(index, true);

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                        ImGui::FocusWindow(ImGui::GetCurrentWindow());

                        m_selected_properties.clear();
                        m_rail_selection_mgr.getState().clearSelection();

                        if (!Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) &&
                            !Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
                            m_scene_selection_mgr.getState().clearSelection();
                        }

                        if (m_table_selection_mgr.getState().getSelection().size() == 1) {
                            for (auto &member : node->getMembers()) {
                                member->syncArray();
                                auto prop = createProperty(member);
                                if (prop) {
                                    m_selected_properties.push_back(std::move(prop));
                                }
                            }
                        }

                        m_selection_transforms_needs_update = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderTableHierarchyContextMenu(node_uid_str, index);

                if (node_open) {
                    for (size_t i = 0; i < m_table_object_model->getRowCount(index); ++i) {
                        ModelIndex child_index = m_table_object_model->getIndex(i, 0, index);
                        renderTableObjectTree(child_index);
                    }
                    ImGui::TreePop();
                }
            }
        } else {
            if (!is_filtered_out) {
                if (node_visibility) {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), the_flags, node_selected);
                }

                node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

                // Drag and drop for OBJECT
                {
                    ImVec2 mouse_pos = ImGui::GetMousePos();
                    ImVec2 item_size = ImGui::GetItemRectSize();
                    ImVec2 item_pos  = ImGui::GetItemRectMin();

                    if (ImGui::BeginDragDropSource()) {
                        int64_t node_index = m_table_object_model->getRow(index);

                        Toolbox::Buffer buffer;
                        saveMimeObject(buffer, node_index, get_shared_ptr(*node->getParent()));
                        ImGui::SetDragDropPayload("toolbox/scene/object", buffer.buf(),
                                                  buffer.size(), ImGuiCond_Once);
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

                                int64_t node_index = m_table_object_model->getRow(index);

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

                if (ImGui::IsItemHovered()) {
                    m_table_selection_mgr.handleActionsByMouseInput(index, true);

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                        ImGui::FocusWindow(ImGui::GetCurrentWindow());

                        m_selected_properties.clear();
                        m_rail_selection_mgr.getState().clearSelection();

                        if (!Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) &&
                            !Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
                            m_scene_selection_mgr.getState().clearSelection();
                        }

                        if (m_table_selection_mgr.getState().getSelection().size() == 1) {
                            for (auto &member : node->getMembers()) {
                                member->syncArray();
                                auto prop = createProperty(member);
                                if (prop) {
                                    m_selected_properties.push_back(std::move(prop));
                                }
                            }
                        }

                        m_selection_transforms_needs_update = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderTableHierarchyContextMenu(node_uid_str, index);

                if (node_open) {
                    ImGui::TreePop();
                }
            }
        }

        const ImGuiStyle &style = ImGui::GetStyle();

        const float render_alpha = is_cut ? 0.5f : 1.0f;
        ImVec4 col               = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        col.w *= render_alpha;

        if (index == m_table_selection_mgr.getState().getLastSelected()) {
            ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            col.w *= render_alpha;
            ImGui::GetWindowDrawList()->AddRect(node_rect.Min - (style.FramePadding / 2.0f),
                                                node_rect.Max + (style.FramePadding / 2.0f),
                                                ImGui::ColorConvertFloat4ToU32(col), 0.0f,
                                                ImDrawFlags_RoundCornersAll, 2.0f);
        }

        ImGui::PopStyleVar();

        ImGui::PopID();
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

        ModelIndex last_selected = window.m_rail_selection_mgr.getState().getLastSelected();
        if (!window.m_rail_model->isIndexRailNode(last_selected)) {
            ImGui::Text("Select a rail node to edit its properties.");
            return false;
        }

        RailData::rail_ptr_t rail   = window.m_rail_model->getRailRef(last_selected);
        Rail::Rail::node_ptr_t node = window.m_rail_model->getRailNodeRef(last_selected);

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
                    ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                rail->setNodeFlag(node, flags);
            }
        }

        /* Values */
        {
            ImGui::Text("Values");

            if (!collapse_lines) {
                ImGui::SameLine();
                ImGui::Dummy({label_width - ImGui::CalcTextSize("Values").x, 0});
                ImGui::SameLine();
            }

            std::string label = "##Values";

            u32 step = 1, step_fast = 10;

            int int_flags[4] = {node->getValue(0).value(), node->getValue(1).value(),
                                node->getValue(2).value(), node->getValue(3).value()};

            if (ImGui::InputScalarCompactN(
                    label.c_str(), ImGuiDataType_S32, &int_flags, 4, &step, &step_fast, nullptr,
                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                int_flags[0] = std::clamp<int>(int_flags[0], std::numeric_limits<s16>::min(),
                                               std::numeric_limits<s16>::max());
                int_flags[1] = std::clamp<int>(int_flags[1], std::numeric_limits<s16>::min(),
                                               std::numeric_limits<s16>::max());
                int_flags[2] = std::clamp<int>(int_flags[2], std::numeric_limits<s16>::min(),
                                               std::numeric_limits<s16>::max());
                int_flags[3] = std::clamp<int>(int_flags[3], std::numeric_limits<s16>::min(),
                                               std::numeric_limits<s16>::max());
                rail->setNodeValue(node, 0, (s16)int_flags[0]);
                rail->setNodeValue(node, 1, (s16)int_flags[1]);
                rail->setNodeValue(node, 2, (s16)int_flags[2]);
                rail->setNodeValue(node, 3, (s16)int_flags[3]);
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
                            LogError(result.error());
                        }
                        is_updated = true;
                    }
                }
            }
        }
        ImGui::EndGroupPanel();

        if (is_updated) {
            window.m_renderer.updatePaths(window.m_rail_model, window.m_rail_visible_map);
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
        std::for_each(std::execution::seq, objects.begin(), objects.end(),
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

        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();
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
        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();
        RefPtr<ISceneObject> root = m_current_scene->getObjHierarchy()->getRoot();
        std::vector<RefPtr<ISceneObject>> objects;
        recursiveFlattenActorTree(root, objects);
        double timing = Timing::measure(recursiveAssignActorPtrs, task_communicator, objects);
        TOOLBOX_INFO_V("[SCENE] Acquired all actor ptrs in {} seconds", timing);
    }

    void SceneWindow::renderRailEditor() {
        if (!m_current_scene) {
            return;
        }

        const std::string rail_editor_str = ImWindowComponentTitle(*this, "Rail Editor");

        constexpr auto rail_flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding |
            ImGuiTreeNodeFlags_DefaultOpen;

        constexpr auto node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth |
                                    ImGuiTreeNodeFlags_FramePadding |
                                    ImGuiTreeNodeFlags_DefaultOpen;

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

            const bool is_cut = false;

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 4.0f});

            ImRect rail_rect;

            const size_t rail_count = m_rail_model->getRowCount(ModelIndex());
            for (size_t i = 0; i < rail_count; ++i) {
                ModelIndex rail_index = m_rail_model->getIndex(i, 0, ModelIndex());
                if (!m_rail_model->isIndexRail(rail_index)) {
                    TOOLBOX_ERROR_V(
                        "[SceneWindow] Expected rail index, got a node index. Skipping index {}",
                        i);
                    continue;
                }

                RailData::rail_ptr_t rail = m_rail_model->getRailRef(rail_index);

                const std::string uid_str = getNodeUID(rail);
                const ImGuiID rail_id     = static_cast<ImGuiID>(rail->getUUID());
                const UUID64 rail_uuid    = rail->getUUID();

                ImGui::PushID(rail_id);

                const bool is_rail_selected =
                    m_rail_selection_mgr.getState().isSelected(rail_index);

                if (!m_rail_visible_map.contains(rail_uuid)) {
                    m_rail_visible_map[rail_uuid] = true;
                }
                bool is_rail_visible = m_rail_visible_map[rail_uuid];

                bool is_rail_open = ImGui::TreeNodeEx(uid_str.data(), rail_flags, is_rail_selected,
                                                      &is_rail_visible);

                rail_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

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
                                "toolbox/scene/rail",
                                ImGuiDragDropFlags_AcceptBeforeDelivery |
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

                if (m_rail_visible_map[rail_uuid] != is_rail_visible) {
                    m_rail_visible_map[rail_uuid] = is_rail_visible;
                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                    m_update_render_objs = true;
                }

                if (ImGui::IsItemHovered()) {
                    m_rail_selection_mgr.handleActionsByMouseInput(rail_index, true);
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                        ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                        ImGui::FocusWindow(ImGui::GetCurrentWindow());

                        m_scene_selection_mgr.getState().clearSelection();
                        m_table_selection_mgr.getState().clearSelection();

                        m_selection_transforms_needs_update = true;

                        m_properties_render_handler = renderRailProperties;
                    }
                }

                renderRailContextMenu(rail->name(), rail_index);

                if (is_rail_open) {
                    ImRect node_rect;

                    const size_t node_count = m_rail_model->getRowCount(rail_index);
                    for (size_t j = 0; j < node_count; ++j) {
                        ModelIndex node_index = m_rail_model->getIndex(j, 0, rail_index);
                        if (!m_rail_model->isIndexRailNode(node_index)) {
                            TOOLBOX_ERROR_V("[SceneWindow] Expected rail node index, got a rail "
                                            "index. Skipping index {} of rail {}",
                                            j, i);
                            continue;
                        }

                        RefPtr<Rail::RailNode> node = m_rail_model->getRailNodeRef(node_index);
                        std::string node_uid_str    = getNodeUID(rail, j);
                        ImGuiID node_id             = static_cast<ImGuiID>(node->getUUID());

                        ImGui::PushID(node_id);

                        bool is_rail_node_selected =
                            m_rail_selection_mgr.getState().isSelected(node_index);

                        bool is_node_open = ImGui::TreeNodeEx(node_uid_str.c_str(), node_flags,
                                                              is_rail_node_selected);

                        node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

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

                        if (ImGui::IsItemHovered()) {
                            m_rail_selection_mgr.handleActionsByMouseInput(node_index, true);
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                                ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                                ImGui::FocusWindow(ImGui::GetCurrentWindow());

                                m_scene_selection_mgr.getState().clearSelection();
                                m_table_selection_mgr.getState().clearSelection();

                                m_selection_transforms_needs_update = true;

                                m_properties_render_handler = renderRailNodeProperties;
                            }
                        }

                        renderRailContextMenu(node_uid_str, node_index);

                        if (is_node_open) {
                            ImGui::TreePop();
                        }

                        const ImGuiStyle &style = ImGui::GetStyle();

                        const float render_alpha = is_cut ? 0.5f : 1.0f;
                        ImVec4 col               = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                        col.w *= render_alpha;

                        if (node_index == m_rail_selection_mgr.getState().getLastSelected()) {
                            ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                            col.w *= render_alpha;
                            ImGui::GetWindowDrawList()->AddRect(
                                node_rect.Min - (style.FramePadding / 2.0f),
                                node_rect.Max + (style.FramePadding / 2.0f),
                                ImGui::ColorConvertFloat4ToU32(col), 0.0f,
                                ImDrawFlags_RoundCornersAll, 2.0f);
                        }

                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                const ImGuiStyle &style = ImGui::GetStyle();

                const float render_alpha = is_cut ? 0.5f : 1.0f;
                ImVec4 col               = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                col.w *= render_alpha;

                if (rail_index == m_rail_selection_mgr.getState().getLastSelected()) {
                    ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    col.w *= render_alpha;
                    ImGui::GetWindowDrawList()->AddRect(rail_rect.Min - (style.FramePadding / 2.0f),
                                                        rail_rect.Max + (style.FramePadding / 2.0f),
                                                        ImGui::ColorConvertFloat4ToU32(col), 0.0f,
                                                        ImDrawFlags_RoundCornersAll, 2.0f);
                }

                ImGui::PopID();
            }

            ImGui::PopStyleVar();
        }
        ImGui::End();

        ModelIndex last_selected = m_rail_selection_mgr.getState().getLastSelected();
        if (m_rail_model->isIndexRail(last_selected)) {
            m_create_rail_dialog.render(last_selected);
            m_rename_rail_dialog.render(last_selected);
        }
    }

    void SceneWindow::renderScene(TimeStep delta_time) {
        const AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        std::vector<J3DLight> lights;

        // perhaps find a way to limit this so it only happens when we need to re-render?
        if (m_current_scene) {
            if (m_update_render_objs || !settings.m_is_rendering_simple) {
                m_renderables.clear();
                auto perform_result = m_current_scene->getObjHierarchy()->getRoot()->performScene(
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
        const AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();

        float window_bar_height =
            ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
        ImGui::SetCursorPos({0, window_bar_height + 10});

        const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        const ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 cmd_button_size   = ImGui::CalcTextSize(ICON_FA_BACKWARD) + frame_padding;
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
        if (ImGui::AlignedButton(ICON_FA_PLAY, cmd_button_size)) {
            if (DolphinHookManager::instance().startProcess(settings.m_hide_dolphin_on_play)) {
                task_communicator.taskLoadScene(m_stage, m_scenario,
                                                TOOLBOX_BIND_EVENT_FN(reassignAllActorPtrs));
            }
        }

        ImGui::PopStyleColor(3);

        if (is_dolphin_running) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.35f, 0.1f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.7f, 0.2f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.7f, 0.2f, 0.2f, 1.0f});

        bool context_controls_disabled = !is_dolphin_running || m_control_disable_requested ||
                                         m_repack_io_busy;

        if (context_controls_disabled) {
            ImGui::BeginDisabled();
        }

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 + cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FA_STOP, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                                 ImDrawFlags_RoundCornersBottomRight)) {
            DolphinHookManager::instance().stopProcess();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.2f, 0.4f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.4f, 0.8f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.4f, 0.8f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 - cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FA_BACKWARD, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                                 ImDrawFlags_RoundCornersBottomLeft)) {
            fs_path scene_arc_path = m_current_scene->rootPath().value_or("");
            if (!scene_arc_path.empty()) {
                scene_arc_path = scene_arc_path.parent_path();
                scene_arc_path.replace_extension(".szs");
                fs_path root_path = scene_arc_path.parent_path().parent_path().parent_path();
                scene_arc_path    = Filesystem::relative(scene_arc_path, root_path).value_or("");
                if (!scene_arc_path.empty()) {
                    task_communicator.flushFileInGameFST(root_path, scene_arc_path);
                }
            }

            task_communicator.taskLoadScene(m_stage, m_scenario,
                                            TOOLBOX_BIND_EVENT_FN(reassignAllActorPtrs));
        }

        ImGui::PopStyleColor(3);

        if (context_controls_disabled) {
            ImGui::EndDisabled();
        }
    }

    void SceneWindow::renderScenePeripherals(TimeStep delta_time) {
        float window_bar_height =
            ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
        ImGui::SetCursorPos({0, window_bar_height + 10});

        const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        ImGui::SetCursorPosX(window_padding.x);
        if (ImGui::Button(m_is_game_edit_mode ? "Game: Edit Mode" : "Game: View Mode")) {
            m_is_game_edit_mode ^= true;
        }
    }

    void SceneWindow::renderDolphin(TimeStep delta_time) {
        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();

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
                DolphinCommunicator &communicator =
                    MainApplication::instance().getDolphinCommunicator();
                Game::TaskCommunicator &task_communicator =
                    MainApplication::instance().getTaskCommunicator();

                m_dolphin_image = std::move(task_communicator.captureXFBAsTexture(
                    static_cast<int>(ImGui::GetWindowWidth()),
                    static_cast<int>(ImGui::GetWindowHeight())));
                if (!m_dolphin_image) {
                    ImGui::Text(
                        "Start a Dolphin process running\nSuper Mario Sunshine to get started");
                } else {
                    m_dolphin_painter.render(m_dolphin_image, render_size);

                    ImGui::SetCursorPos(cursor_pos);
                    for (const auto &[layer_name, render_layer] : m_render_layers) {
                        render_layer(delta_time, std::string_view(layer_name),
                                     ImGui::GetWindowSize().x, ImGui::GetWindowSize().y,
                                     m_dolphin_vp_mtx, getUUID());
                        ImGui::SetCursorPos(cursor_pos);
                    }
                }
            }

            // ImGui::PopClipRect();

            renderPlaybackButtons(delta_time);

            ImGui::Dummy({0, 0});  // Needed to grow parent boundaries after cursor manipulations
                                   // (recent ImGui spec)
        }
        ImGui::End();
    }  // namespace Toolbox::UI

    void SceneWindow::renderSceneHierarchyContextMenu(std::string str_id, const ModelIndex &index) {
        const ModelIndex &selected_index = m_scene_selection_mgr.getState().getLastSelected();
        if (!m_scene_object_model->validateIndex(selected_index)) {
            return;
        }

        m_scene_hierarchy_context_menu.renderForItem(str_id, selected_index);
    }

    void SceneWindow::renderTableHierarchyContextMenu(std::string str_id, const ModelIndex &index) {
        const ModelIndex &selected_index = m_table_selection_mgr.getState().getLastSelected();
        if (!m_table_object_model->validateIndex(selected_index)) {
            return;
        }

        m_table_hierarchy_context_menu.renderForItem(str_id, selected_index);
    }

    void SceneWindow::renderRailContextMenu(std::string str_id, const ModelIndex &index) {
        const ModelIndex &selected_index = m_rail_selection_mgr.getState().getLastSelected();
        if (!m_rail_model->validateIndex(selected_index)) {
            return;
        }

        m_rail_list_context_menu.renderForItem(str_id, selected_index);
    }

    void SceneWindow::buildContextMenuSceneObj() {
        m_scene_hierarchy_context_menu = ContextMenu<ModelIndex>();

        ContextMenuBuilder(&m_scene_hierarchy_context_menu)
            .addOption(
                "Add Child Object...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) {
                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    RefPtr<ISceneObject> parent_obj = m_scene_object_model->getObjectRef(index);
                    if (!parent_obj) {
                        return false;
                    }
                    return m_scene_selection_mgr.getState().count() == 1 &&
                           parent_obj->isGroupObject();
                },
                [this](ModelIndex index) {
                    m_create_scene_obj_dialog.setInsertPolicy(
                        CreateObjDialog::InsertPolicy::INSERT_CHILD);
                    m_create_scene_obj_dialog.open();
                    return;
                })
            .addOption(
                "Insert Object Before...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) {
                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    return true;
                },
                [this](ModelIndex index) {
                    m_create_scene_obj_dialog.setInsertPolicy(
                        CreateObjDialog::InsertPolicy::INSERT_BEFORE);
                    m_create_scene_obj_dialog.open();
                })
            .addOption(
                "Insert Object After...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) {
                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    return true;
                },
                [this](ModelIndex index) {
                    m_create_scene_obj_dialog.setInsertPolicy(
                        CreateObjDialog::InsertPolicy::INSERT_AFTER);
                    m_create_scene_obj_dialog.open();
                })
            .addDivider()
            .addOption("Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                       [this](ModelIndex index) {
                           std::string name = m_scene_object_model->getObjectKey(index);
                           m_rename_scene_obj_dialog.open();
                           m_rename_scene_obj_dialog.setOriginalName(name);
                       })
            .addDivider()
            .addOption(
                "View in Scene", {KeyCode::KEY_LEFTALT, KeyCode::KEY_V},
                [this](ModelIndex index) {
                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }
                    return this_obj->getTransform().has_value();
                },
                [this](ModelIndex index) {
                    RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(index);
                    Transform transform      = obj->getTransform().value();

                    f32 max_scale = std::max(transform.m_scale.x, transform.m_scale.y);
                    max_scale     = std::max(max_scale, transform.m_scale.z);

                    m_renderer.setCameraOrientation({0, 1, 0}, transform.m_translation,
                                                    {transform.m_translation.x,
                                                     transform.m_translation.y,
                                                     transform.m_translation.z + 1000 * max_scale});
                    m_update_render_objs = true;
                    return;
                })
            .addOption(
                "Move to Camera", {KeyCode::KEY_LEFTALT, KeyCode::KEY_C},
                [this](ModelIndex index) {
                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }
                    return this_obj->getTransform().has_value();
                },
                [this](ModelIndex index) {
                    RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(index);

                    Transform transform = obj->getTransform().value();
                    m_renderer.getCameraTranslation(transform.m_translation);
                    auto result = obj->setTransform(transform);
                    if (!result) {
                        LogError(
                            make_error<void>("Scene Hierarchy",
                                             "Failed to set transform of object when moving to "
                                             "camera")
                                .error());
                        return;
                    }

                    m_update_render_objs = true;
                    return;
                })
            .addDivider()
            .addOption("Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
                       [this](ModelIndex index) {
                           ScopePtr<MimeData> copy_data =
                               m_scene_selection_mgr.actionCopySelection();
                           if (!copy_data) {
                               return;
                           }
                           SystemClipboard::instance().setContent(*copy_data);
                       })
            .addOption("Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
                       [this](ModelIndex index) {
                           auto result = SystemClipboard::instance().getContent();
                           if (!result) {
                               LogError(result.error());
                               return;
                           }
                           m_scene_selection_mgr.actionPasteIntoSelection(result.value());
                           m_update_render_objs = true;
                       })
            .addDivider()
            .addOption("Delete", {KeyCode::KEY_DELETE},
                       [this](ModelIndex index) { m_scene_selection_mgr.actionDeleteSelection(); })
            .addDivider()
            .addOption(
                "Copy Player Transform",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_P},
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }

                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }

                    return this_obj->getTransform().has_value() &&
                           task_communicator.isSceneLoaded();
                },
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);

                    task_communicator.setObjectTransformToMario(
                        ref_cast<PhysicalSceneObject>(this_obj));
                    m_update_render_objs = true;
                    return;
                })
            .addOption(
                "Copy Player Position",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_P},
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    if (m_scene_selection_mgr.getState().count() != 1) {
                        return false;
                    }

                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }

                    return this_obj->getTransform().has_value() &&
                           task_communicator.isSceneLoaded();
                },
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);
                    if (!this_obj) {
                        TOOLBOX_ERROR("[Scene Hierarchy] Failed to get object reference when "
                                      "copying player position");
                        return;
                    }

                    task_communicator.setObjectTranslationToMario(
                        ref_cast<PhysicalSceneObject>(this_obj));
                    m_update_render_objs = true;
                });
    }

    void SceneWindow::buildContextMenuTableObj() {
        m_table_hierarchy_context_menu = ContextMenu<ModelIndex>();

        ContextMenuBuilder(&m_table_hierarchy_context_menu)
            .addOption(
                "Add Child Object...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) {
                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    RefPtr<ISceneObject> parent_obj = m_table_object_model->getObjectRef(index);
                    if (!parent_obj) {
                        return false;
                    }
                    return m_table_selection_mgr.getState().count() == 1 &&
                           parent_obj->isGroupObject();
                },
                [this](ModelIndex index) {
                    m_create_table_obj_dialog.setInsertPolicy(
                        CreateObjDialog::InsertPolicy::INSERT_CHILD);
                    m_create_table_obj_dialog.open();
                    return;
                })
            .addOption(
                "Insert Object Before...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) {
                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    return true;
                },
                [this](ModelIndex index) {
                    m_create_table_obj_dialog.setInsertPolicy(
                        CreateObjDialog::InsertPolicy::INSERT_BEFORE);
                    m_create_table_obj_dialog.open();
                })
            .addOption(
                "Insert Object After...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) {
                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    return true;
                },
                [this](ModelIndex index) {
                    m_create_table_obj_dialog.setInsertPolicy(
                        CreateObjDialog::InsertPolicy::INSERT_AFTER);
                    m_create_table_obj_dialog.open();
                })
            .addDivider()
            .addOption("Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                       [this](ModelIndex index) {
                           std::string name = m_table_object_model->getObjectKey(index);
                           m_rename_table_obj_dialog.open();
                           m_rename_table_obj_dialog.setOriginalName(name);
                       })
            .addDivider()
            .addOption(
                "View in Scene", {KeyCode::KEY_LEFTALT, KeyCode::KEY_V},
                [this](ModelIndex index) {
                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    RefPtr<ISceneObject> this_obj = m_table_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }
                    return this_obj->getTransform().has_value();
                },
                [this](ModelIndex index) {
                    RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(index);
                    Transform transform      = obj->getTransform().value();

                    f32 max_scale = std::max(transform.m_scale.x, transform.m_scale.y);
                    max_scale     = std::max(max_scale, transform.m_scale.z);

                    m_renderer.setCameraOrientation({0, 1, 0}, transform.m_translation,
                                                    {transform.m_translation.x,
                                                     transform.m_translation.y,
                                                     transform.m_translation.z + 1000 * max_scale});
                    m_update_render_objs = true;
                    return;
                })
            .addOption(
                "Move to Camera", {KeyCode::KEY_LEFTALT, KeyCode::KEY_C},
                [this](ModelIndex index) {
                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }
                    RefPtr<ISceneObject> this_obj = m_table_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }
                    return this_obj->getTransform().has_value();
                },
                [this](ModelIndex index) {
                    RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(index);

                    Transform transform = obj->getTransform().value();
                    m_renderer.getCameraTranslation(transform.m_translation);
                    auto result = obj->setTransform(transform);
                    if (!result) {
                        LogError(
                            make_error<void>("Scene Hierarchy",
                                             "Failed to set transform of object when moving to "
                                             "camera")
                                .error());
                        return;
                    }

                    m_update_render_objs = true;
                    return;
                })
            .addDivider()
            .addOption("Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
                       [this](ModelIndex index) {
                           ScopePtr<MimeData> copy_data =
                               m_table_selection_mgr.actionCopySelection();
                           if (!copy_data) {
                               return;
                           }
                           SystemClipboard::instance().setContent(*copy_data);
                       })
            .addOption("Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
                       [this](ModelIndex index) {
                           auto result = SystemClipboard::instance().getContent();
                           if (!result) {
                               LogError(result.error());
                               return;
                           }
                           m_table_selection_mgr.actionPasteIntoSelection(result.value());
                           m_update_render_objs = true;
                       })
            .addDivider()
            .addOption("Delete", {KeyCode::KEY_DELETE},
                       [this](ModelIndex index) { m_table_selection_mgr.actionDeleteSelection(); })
            .addDivider()
            .addOption(
                "Copy Player Transform",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_P},
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }

                    RefPtr<ISceneObject> this_obj = m_table_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }

                    return this_obj->getTransform().has_value() &&
                           task_communicator.isSceneLoaded();
                },
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    RefPtr<ISceneObject> this_obj = m_table_object_model->getObjectRef(index);

                    task_communicator.setObjectTransformToMario(
                        ref_cast<PhysicalSceneObject>(this_obj));
                    m_update_render_objs = true;
                    return;
                })
            .addOption(
                "Copy Player Position",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_P},
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    if (m_table_selection_mgr.getState().count() != 1) {
                        return false;
                    }

                    RefPtr<ISceneObject> this_obj = m_table_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }

                    return this_obj->getTransform().has_value() &&
                           task_communicator.isSceneLoaded();
                },
                [this](ModelIndex index) {
                    Game::TaskCommunicator &task_communicator =
                        MainApplication::instance().getTaskCommunicator();

                    RefPtr<ISceneObject> this_obj = m_scene_object_model->getObjectRef(index);
                    if (!this_obj) {
                        return false;
                    }

                    task_communicator.setObjectTranslationToMario(
                        ref_cast<PhysicalSceneObject>(this_obj));
                    m_update_render_objs = true;
                });
    }

    void SceneWindow::buildContextMenuRail() {
        m_rail_list_context_menu = ContextMenu<ModelIndex>();

        ContextMenuBuilder(&m_rail_list_context_menu)
            .addOption(
                "Insert Rail...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1 &&
                           m_rail_model->isIndexRail(index);
                },
                [this](ModelIndex index) {
                    m_create_rail_dialog.open();
                    return;
                })
            .addOption(
                "Insert Node", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1;
                },
                [this](ModelIndex index) {
                    ModelIndex parent_index = m_rail_model->getParent(index);
                    int64_t sibling_index   = m_rail_model->getRow(index);
                    int64_t sibling_count   = m_rail_model->getRowCount(parent_index);

                    Rail::Rail::node_ptr_t new_node = std::make_shared<Rail::RailNode>();

                    ModelIndex result;
                    if (m_rail_model->validateIndex(parent_index)) {
                        // This means a node is selected
                        result =
                            m_rail_model->insertRailNode(new_node, sibling_index, parent_index);
                    } else {
                        // This means a rail is selected, so we insert at the end of the rail
                        result = m_rail_model->insertRailNode(new_node, sibling_count, index);
                    }

                    if (!m_rail_model->isIndexRailNode(result)) {
                        LogError(make_error<void>("Rail List Context Menu",
                                                  "Failed to insert new rail node into model")
                                     .error());
                        return;
                    }
                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addOption(
                "Insert Node At Camera", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1;
                },
                [this](ModelIndex index) {
                    ModelIndex parent_index = m_rail_model->getParent(index);
                    int64_t sibling_index   = m_rail_model->getRow(index);
                    int64_t sibling_count   = m_rail_model->getRowCount(parent_index);

                    glm::vec3 translation;
                    m_renderer.getCameraTranslation(translation);
                    Rail::Rail::node_ptr_t new_node = std::make_shared<Rail::RailNode>(translation);

                    ModelIndex result;
                    if (m_rail_model->validateIndex(parent_index)) {
                        // This means a node is selected
                        result =
                            m_rail_model->insertRailNode(new_node, sibling_index, parent_index);
                    } else {
                        // This means a rail is selected, so we insert at the end of the rail
                        result = m_rail_model->insertRailNode(new_node, sibling_count, index);
                    }

                    if (!m_rail_model->isIndexRailNode(result)) {
                        LogError(make_error<void>("Rail List Context Menu",
                                                  "Failed to insert new rail node into model")
                                     .error());
                        return;
                    }
                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addDivider()
            .beginGroup("Node Connections")
            .addOption(
                "Connect This to Selection",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_S},
                [this](ModelIndex index) -> bool {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return false;
                    }
                    const UUID64 rail_uuid = m_rail_model->getRailNodeRef(index)->getRailUUID();
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();
                    return selection.size() > 1 && std::all_of(
                        selection.begin(), selection.end(),
                        [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                            const bool is_valid_node = m_rail_model->isIndexRailNode(sel_index);
                            const bool is_same_rail =
                                m_rail_model->getRailNodeRef(sel_index)->getRailUUID() == rail_uuid;
                            return is_valid_node && is_same_rail;
                        });
                },
                [this](ModelIndex index) {
                    std::vector<ModelIndex> selection_cpy =
                        m_rail_selection_mgr.getState().getSelection();
                    std::erase(selection_cpy, index);

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    rail->clearConnections(node);

                    for (const ModelIndex &sel_index : selection_cpy) {
                        Rail::Rail::node_ptr_t sel_node = m_rail_model->getRailNodeRef(sel_index);
                        auto result                     = rail->addConnection(node, sel_node);
                        if (!result) {
                            LogError(result.error());
                        }
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addDivider()
            .addOption(
                "Connect to Nearest",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return false;
                    }
                    const UUID64 rail_uuid = m_rail_model->getRailNodeRef(index)->getRailUUID();
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selection.begin(), selection.end(),
                        [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                            const bool is_valid_node = m_rail_model->isIndexRailNode(sel_index);
                            const bool is_same_rail =
                                m_rail_model->getRailNodeRef(sel_index)->getRailUUID() == rail_uuid;
                            return is_valid_node && is_same_rail;
                        });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    for (const ModelIndex &sel_index : selection) {
                        Rail::Rail::node_ptr_t sel_node = m_rail_model->getRailNodeRef(sel_index);
                        auto result                     = rail->connectNodeToNearest(sel_node, 1);
                        if (!result) {
                            LogError(result.error());
                        }
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addOption(
                "Connect to Neighbors",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_B},
                [this](ModelIndex index) -> bool {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return false;
                    }
                    const UUID64 rail_uuid = m_rail_model->getRailNodeRef(index)->getRailUUID();
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selection.begin(), selection.end(),
                        [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                            const bool is_valid_node = m_rail_model->isIndexRailNode(sel_index);
                            const bool is_same_rail =
                                m_rail_model->getRailNodeRef(sel_index)->getRailUUID() == rail_uuid;
                            return is_valid_node && is_same_rail;
                        });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    for (const ModelIndex &sel_index : selection) {
                        Rail::Rail::node_ptr_t sel_node = m_rail_model->getRailNodeRef(sel_index);
                        auto result = rail->connectNodeToNeighbors(sel_node, false);
                        if (!result) {
                            LogError(result.error());
                        }
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addOption(
                "Connect to Next",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return false;
                    }
                    const UUID64 rail_uuid = m_rail_model->getRailNodeRef(index)->getRailUUID();
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selection.begin(), selection.end(),
                        [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                            const bool is_valid_node = m_rail_model->isIndexRailNode(sel_index);
                            const bool is_same_rail =
                                m_rail_model->getRailNodeRef(sel_index)->getRailUUID() == rail_uuid;
                            return is_valid_node && is_same_rail;
                        });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    for (const ModelIndex &sel_index : selection) {
                        Rail::Rail::node_ptr_t sel_node = m_rail_model->getRailNodeRef(sel_index);
                        auto result = rail->connectNodeToNext(sel_node);
                        if (!result) {
                            LogError(result.error());
                        }
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addOption(
                "Connect to Prev",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_P},
                [this](ModelIndex index) -> bool {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return false;
                    }
                    const UUID64 rail_uuid = m_rail_model->getRailNodeRef(index)->getRailUUID();
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selection.begin(), selection.end(),
                        [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                            const bool is_valid_node = m_rail_model->isIndexRailNode(sel_index);
                            const bool is_same_rail =
                                m_rail_model->getRailNodeRef(sel_index)->getRailUUID() == rail_uuid;
                            return is_valid_node && is_same_rail;
                        });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    for (const ModelIndex &sel_index : selection) {
                        Rail::Rail::node_ptr_t sel_node = m_rail_model->getRailNodeRef(sel_index);
                        auto result                     = rail->connectNodeToPrev(sel_node);
                        if (!result) {
                            LogError(result.error());
                        }
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addOption(
                "Connect to References",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_P},
                [this](ModelIndex index) -> bool {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return false;
                    }
                    const UUID64 rail_uuid = m_rail_model->getRailNodeRef(index)->getRailUUID();
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selection.begin(), selection.end(),
                        [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                            const bool is_valid_node = m_rail_model->isIndexRailNode(sel_index);
                            const bool is_same_rail =
                                m_rail_model->getRailNodeRef(sel_index)->getRailUUID() == rail_uuid;
                            return is_valid_node && is_same_rail;
                        });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selection =
                        m_rail_selection_mgr.getState().getSelection();

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    for (const ModelIndex &sel_index : selection) {
                        Rail::Rail::node_ptr_t sel_node = m_rail_model->getRailNodeRef(sel_index);
                        auto result                     = rail->connectNodeToReferrers(sel_node);
                        if (!result) {
                            LogError(result.error());
                        }
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .endGroup()
            .addOption(
                "Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1 &&
                           m_rail_model->isIndexRail(index);
                },
                [this](ModelIndex index) {
                    m_rename_rail_dialog.open();
                    m_rename_rail_dialog.setOriginalName(m_rail_model->getRailKey(index));
                    return;
                })
            .addDivider()
            .addOption(
                "View in Scene", {KeyCode::KEY_LEFTALT, KeyCode::KEY_V},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1;
                },
                [this](ModelIndex index) {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return;
                    }

                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);
                    glm::vec3 translation       = node->getPosition();

                    m_renderer.setCameraOrientation(
                        {0, 1, 0}, translation,
                        {translation.x, translation.y, translation.z + 1000});
                    m_update_render_objs = true;
                })
            .addOption(
                "Move to Camera", {KeyCode::KEY_LEFTALT, KeyCode::KEY_C},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1;
                },
                [this](ModelIndex index) {
                    if (!m_rail_model->isIndexRailNode(index)) {
                        return;
                    }

                    ModelIndex rail_index = m_rail_model->getParent(index);
                    if (!m_rail_model->isIndexRail(rail_index)) {
                        return;
                    }

                    RailData::rail_ptr_t rail   = m_rail_model->getRailRef(rail_index);
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                    glm::vec3 translation;
                    m_renderer.getCameraTranslation(translation);
                    auto result = rail->setNodePosition(node, translation);
                    if (!result) {
                        LogError(result.error());
                        return;
                    }

                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                    m_update_render_objs = true;
                    return;
                })
            .addDivider()
            .addOption("Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
                       [this](ModelIndex index) {
                           ScopePtr<MimeData> copy_data =
                               m_rail_selection_mgr.actionCopySelection();
                           if (!copy_data) {
                               return;
                           }
                           SystemClipboard::instance().setContent(*copy_data);
                       })
            .addOption("Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
                       [this](ModelIndex index) {
                           auto result = SystemClipboard::instance().getContent();
                           if (!result) {
                               LogError(result.error());
                               return;
                           }
                           m_rail_selection_mgr.actionPasteIntoSelection(result.value());
                           m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                           m_update_render_objs = true;
                       })
            .addOption("Delete", {KeyCode::KEY_DELETE},
                       [this](ModelIndex index) {
                           m_rail_selection_mgr.actionDeleteSelection();
                           m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                           m_update_render_objs = true;
                       })
            .addDivider()
            .addOption(
                "Decimate", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_D},
                [this](ModelIndex index) -> bool {
                    const std::vector<ModelIndex> &selected_indices =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selected_indices.begin(), selected_indices.end(),
                        [this](ModelIndex index) { return m_rail_model->isIndexRail(index); });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selected_indices =
                        m_rail_selection_mgr.getState().getSelection();
                    for (const ModelIndex &index : selected_indices) {
                        if (!m_rail_model->isIndexRail(index)) {
                            return;
                        }
                        RailData::rail_ptr_t rail = m_rail_model->getRailRef(index);
                        rail->decimate(1);
                    }
                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                })
            .addOption(
                "Subdivide", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_S},
                [this](ModelIndex index) -> bool {
                    const std::vector<ModelIndex> &selected_indices =
                        m_rail_selection_mgr.getState().getSelection();
                    return std::all_of(
                        selected_indices.begin(), selected_indices.end(),
                        [this](ModelIndex index) { return m_rail_model->isIndexRail(index); });
                },
                [this](ModelIndex index) {
                    const std::vector<ModelIndex> &selected_indices =
                        m_rail_selection_mgr.getState().getSelection();
                    for (const ModelIndex &index : selected_indices) {
                        if (!m_rail_model->isIndexRail(index)) {
                            return;
                        }
                        RailData::rail_ptr_t rail = m_rail_model->getRailRef(index);
                        rail->subdivide(1);
                    }
                    m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
                });
    }

    void SceneWindow::buildCreateObjDialog() {
        AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        m_create_scene_obj_dialog.setExtendedMode(settings.m_is_custom_obj_allowed);
        m_create_scene_obj_dialog.setup();
        m_create_scene_obj_dialog.setActionOnAccept([this](std::string_view name,
                                                           const Object::Template &template_,
                                                           std::string_view wizard_name,
                                                           CreateObjDialog::InsertPolicy policy,
                                                           const ModelIndex &index) {
            auto new_object_result = Object::ObjectFactory::create(
                template_, wizard_name, m_current_scene->rootPath().value_or(""));
            if (!name.empty()) {
                new_object_result->setNameRef(name);
            }

            if (!m_scene_object_model->validateIndex(index)) {
                auto result = make_error<void>("Scene Hierarchy",
                                               "Failed to get selected node for obj creation");
                LogError(result.error());
                return;
            }

            RefPtr<ISceneObject> insert_obj = m_scene_object_model->getObjectRef(index);

            ModelIndex parent_index;
            size_t insert_index;

            if (insert_obj->isGroupObject() &&
                policy == CreateObjDialog::InsertPolicy::INSERT_CHILD) {
                parent_index = index;
                insert_index = m_scene_object_model->getRowCount(index);
            } else {
                int64_t sibling_index = m_scene_object_model->getRow(index);
                parent_index          = m_scene_object_model->getParent(index);
                insert_index          = policy == CreateObjDialog::InsertPolicy::INSERT_BEFORE
                                            ? sibling_index
                                            : sibling_index + 1;
            }

            if (!m_scene_object_model->validateIndex(parent_index)) {
                auto result = make_error<void>("Scene Hierarchy",
                                               "Failed to get parent node for obj creation");
                LogError(result.error());
                return;
            }

            ModelIndex new_object = m_scene_object_model->insertObject(std::move(new_object_result),
                                                                       insert_index, parent_index);
            if (!m_scene_object_model->validateIndex(new_object)) {
                auto result = make_error<void>("Scene Hierarchy", "Failed to create new object");
                LogError(result.error());
                return;
            }

            RefPtr<ISceneObject> object = m_scene_object_model->getObjectRef(new_object);
            RefPtr<GroupSceneObject> parent_obj =
                ref_cast<GroupSceneObject>(m_scene_object_model->getObjectRef(parent_index));

            Game::TaskCommunicator &task_communicator =
                MainApplication::instance().getTaskCommunicator();
            task_communicator.taskAddSceneObject(
                object, parent_obj, [object](u32 actor_ptr) { object->setGamePtr(actor_ptr); });

            m_update_render_objs = true;
            return;
        });
        m_create_scene_obj_dialog.setActionOnReject([](const ModelIndex &) {});

        // ================

        m_create_table_obj_dialog.setExtendedMode(settings.m_is_custom_obj_allowed);
        m_create_table_obj_dialog.setup();
        m_create_table_obj_dialog.setActionOnAccept([this](std::string_view name,
                                                           const Object::Template &template_,
                                                           std::string_view wizard_name,
                                                           CreateObjDialog::InsertPolicy policy,
                                                           const ModelIndex &index) {
            auto new_object_result = Object::ObjectFactory::create(
                template_, wizard_name, m_current_scene->rootPath().value_or(""));
            if (!name.empty()) {
                new_object_result->setNameRef(name);
            }

            if (!m_table_object_model->validateIndex(index)) {
                auto result = make_error<void>("Scene Hierarchy",
                                               "Failed to get selected node for obj creation");
                LogError(result.error());
                return;
            }

            RefPtr<ISceneObject> insert_obj = m_table_object_model->getObjectRef(index);

            ModelIndex parent_index;
            size_t insert_index;

            if (insert_obj->isGroupObject() &&
                policy == CreateObjDialog::InsertPolicy::INSERT_CHILD) {
                parent_index = index;
                insert_index = m_table_object_model->getRowCount(index);
            } else {
                int64_t sibling_index = m_table_object_model->getRow(index);
                parent_index          = m_table_object_model->getParent(index);
                insert_index          = policy == CreateObjDialog::InsertPolicy::INSERT_BEFORE
                                            ? sibling_index
                                            : sibling_index + 1;
            }

            if (!m_table_object_model->validateIndex(parent_index)) {
                auto result = make_error<void>("Scene Hierarchy",
                                               "Failed to get parent node for obj creation");
                LogError(result.error());
                return;
            }

            ModelIndex new_object = m_table_object_model->insertObject(std::move(new_object_result),
                                                                       insert_index, parent_index);
            if (!m_table_object_model->validateIndex(new_object)) {
                auto result = make_error<void>("Scene Hierarchy", "Failed to create new object");
                LogError(result.error());
                return;
            }

            RefPtr<ISceneObject> object = m_table_object_model->getObjectRef(new_object);
            RefPtr<GroupSceneObject> parent_obj =
                ref_cast<GroupSceneObject>(m_table_object_model->getObjectRef(parent_index));

            Game::TaskCommunicator &task_communicator =
                MainApplication::instance().getTaskCommunicator();
            task_communicator.taskAddSceneObject(
                object, parent_obj, [object](u32 actor_ptr) { object->setGamePtr(actor_ptr); });

            m_update_render_objs = true;
            return;
        });
        m_create_table_obj_dialog.setActionOnReject([](const ModelIndex &) {});
    }

    void SceneWindow::buildRenameObjDialog() {
        m_rename_scene_obj_dialog.setup();
        m_rename_scene_obj_dialog.setActionOnAccept([this](std::string_view new_name,
                                                           const ModelIndex &index) {
            if (new_name.empty()) {
                auto result = make_error<void>("SCENE", "Can not rename object to empty string");
                LogError(result.error());
                return;
            }
            for (const ModelIndex &selected_index :
                 m_scene_selection_mgr.getState().getSelection()) {
                if (!m_scene_object_model->validateIndex(selected_index)) {
                    auto result =
                        make_error<void>("Scene Hierarchy", "Failed to get object for renaming");
                    LogError(result.error());
                    continue;
                }

                ModelIndex parent_index = m_scene_object_model->getParent(selected_index);

                std::string old_name = m_scene_object_model->getObjectKey(selected_index);
                if (old_name == new_name) {
                    continue;
                }

                std::string new_unique_name =
                    m_scene_object_model->findUniqueName(parent_index, std::string(new_name));
                m_scene_object_model->setRailKey(selected_index, new_unique_name);

                RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(selected_index);

                Game::TaskCommunicator &task_communicator =
                    MainApplication::instance().getTaskCommunicator();
                task_communicator.taskRenameSceneObject(obj, old_name, new_unique_name);
            }
        });
        m_rename_scene_obj_dialog.setActionOnReject([](const ModelIndex &) {});

        // ================

        m_rename_table_obj_dialog.setup();
        m_rename_table_obj_dialog.setActionOnAccept([this](std::string_view new_name,
                                                           const ModelIndex &index) {
            if (new_name.empty()) {
                auto result = make_error<void>("SCENE", "Can not rename object to empty string");
                LogError(result.error());
                return;
            }
            for (const ModelIndex &selected_index :
                 m_table_selection_mgr.getState().getSelection()) {
                if (!m_table_object_model->validateIndex(selected_index)) {
                    auto result =
                        make_error<void>("Scene Hierarchy", "Failed to get object for renaming");
                    LogError(result.error());
                    continue;
                }

                ModelIndex parent_index = m_table_object_model->getParent(selected_index);

                std::string old_name = m_table_object_model->getObjectKey(selected_index);
                if (old_name == new_name) {
                    continue;
                }

                std::string new_unique_name =
                    m_table_object_model->findUniqueName(parent_index, std::string(new_name));
                m_table_object_model->setRailKey(selected_index, new_unique_name);

                RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(selected_index);

                Game::TaskCommunicator &task_communicator =
                    MainApplication::instance().getTaskCommunicator();
                task_communicator.taskRenameSceneObject(obj, old_name, new_unique_name);
            }
        });
        m_rename_table_obj_dialog.setActionOnReject([](const ModelIndex &) {});
    }

    void SceneWindow::buildCreateRailDialog() {
        m_create_rail_dialog.setup();
        m_create_rail_dialog.setActionOnAccept([this](std::string_view name, u16 node_count,
                                                      s16 node_distance, bool loop) {
            if (name.empty()) {
                auto result = make_error<void>("SCENE", "Can not name rail as empty string");
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

            RailData::rail_ptr_t new_rail = make_referable<Rail::Rail>(name, new_nodes);

            for (u16 i = 0; i < node_count; ++i) {
                auto result = new_rail->connectNodeToNeighbors(i, true);
                if (!result) {
                    LogError(result.error());
                }
            }

            if (!loop) {
                // First
                {
                    auto result = new_rail->removeConnection(0, 1);
                    if (!result) {
                        LogError(result.error());
                    }
                }

                // Last
                {
                    auto result = new_rail->removeConnection(node_count - 1, 1);
                    if (!result) {
                        LogError(result.error());
                    }
                }
            }

            m_rail_model->insertRail(std::move(new_rail), m_rail_model->getRowCount(ModelIndex()));

            m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
            m_update_render_objs = true;
        });
    }

    void SceneWindow::buildRenameRailDialog() {
        m_rename_rail_dialog.setup();
        m_rename_rail_dialog.setActionOnAccept(
            [this](std::string_view new_name, const ModelIndex &index) {
                if (new_name.empty()) {
                    auto result =
                        make_error<void>("Scene Hierarchy", "Can not rename rail to empty string");
                    LogError(result.error());
                    return;
                }

                if (!m_rail_model->isIndexRail(index)) {
                    auto result =
                        make_error<void>("Scene Hierarchy", "Failed to get rail for renaming");
                    LogError(result.error());
                    return;
                }

                m_rail_model->setRailKey(index, std::string(new_name));
            });
        m_rename_rail_dialog.setActionOnReject([](const ModelIndex &index) {});
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
        RailData::rail_ptr_t rail = m_current_scene->getRailData()->getRail(index);
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

    void SceneWindow::saveMimeRailNode(Buffer &buffer, size_t index, RailData::rail_ptr_t parent) {
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

        RefPtr<ISceneObject> parent = m_current_scene->getObjHierarchy()->findObject(parent_id);
        if (!parent) {
            LogError(make_error<void>("Scene Hierarchy", "Failed to get parent object").error());
            return;
        }

        if (is_internal) {
            RefPtr<ISceneObject> orig_parent =
                m_current_scene->getObjHierarchy()->findObject(orig_parent_uuid);
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
        //         m_current_scene->getObjHierarchy()->findObject(orig_parent_uuid);
        //     if (orig_parent) {
        //         TRY(orig_parent->removeChild(orig_index)).error([](const ObjectGroupError
        //         &err) {
        //             LogError(err);
        //         });
        //     }
        // }

        // RefPtr<ISceneObject> orig_parent =
        //     m_current_scene->getObjHierarchy()->findObject(orig_parent_uuid);
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

        m_current_scene->getRailData()->insertRail(index, rail);
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

    void SceneWindow::processObjectSelection(RefPtr<Object::ISceneObject> node, bool is_multi) {
        if (!node) {
            TOOLBOX_DEBUG_LOG("Hit object is null");
            return;
        }

        const IDataModel::index_container &selection =
            m_scene_selection_mgr.getState().getSelection();

        const ModelIndex new_obj_selection = m_scene_object_model->getIndex(node);

        // const bool is_object_selected =
        //     std::any_of(selection.begin(), selection.end(), new_obj_selection);

        m_rail_selection_mgr.getState().clearSelection();

        m_scene_selection_mgr.actionSelectIndex(new_obj_selection, !is_multi, false, true);
        if (!Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) &&
            !Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
            m_table_selection_mgr.getState().clearSelection();
        }

        if (m_scene_selection_mgr.getState().getSelection().size() == 1) {
            for (auto &member : node->getMembers()) {
                member->syncArray();
                auto prop = createProperty(member);
                if (prop) {
                    m_selected_properties.push_back(std::move(prop));
                }
            }
        }

        m_selection_transforms_needs_update = true;

        m_properties_render_handler = renderObjectProperties;

        TOOLBOX_DEBUG_LOG_V("Hit object {} ({})", node->type(), node->getNameRef().name());
    }

    void Toolbox::UI::SceneWindow::processRailSelection(RailData::rail_ptr_t node, bool is_multi) {
        ImGuiID rail_id = static_cast<ImGuiID>(node->getUUID());

        ModelIndex new_rail_selection = m_rail_model->getIndex(node);

        m_scene_selection_mgr.getState().clearSelection();
        m_table_selection_mgr.getState().clearSelection();

        m_rail_selection_mgr.actionSelectIndex(new_rail_selection, !is_multi, false, true);

        m_selection_transforms_needs_update = true;

        m_properties_render_handler = renderRailProperties;

        TOOLBOX_DEBUG_LOG_V("Hit rail \"{}\"", node->name());
    }

    void Toolbox::UI::SceneWindow::processRailNodeSelection(RefPtr<Rail::RailNode> node,
                                                            bool is_multi) {
        ImGuiID rail_id = static_cast<ImGuiID>(node->getUUID());

        ModelIndex new_rail_selection = m_rail_model->getIndex(node);

        m_scene_selection_mgr.getState().clearSelection();
        m_table_selection_mgr.getState().clearSelection();

        m_rail_selection_mgr.actionSelectIndex(new_rail_selection, !is_multi, false, true);

        m_selection_transforms_needs_update = true;

        m_properties_render_handler = renderRailNodeProperties;

        // Debug log
        {
            RefPtr<RailData> rail_data = m_current_scene->getRailData();

            RailData::rail_ptr_t rail = rail_data->getRail(node->getRailUUID());
            if (!rail) {
                TOOLBOX_ERROR("Failed to find rail for node.");
                return;
            }

            TOOLBOX_DEBUG_LOG_V("Hit node {} of rail \"{}\"", rail->getNodeIndex(node).value(),
                                rail->name());
        }
    }

    void Toolbox::UI::SceneWindow::calcNewGizmoMatrixFromSelection() {
        Transform combined_transform     = {};
        combined_transform.m_translation = {0.0f, 0.0f, 0.0f};
        combined_transform.m_rotation    = {0.0f, 0.0f, 0.0f};
        combined_transform.m_scale       = {1.0f, 1.0f, 1.0f};

        const size_t total_selected_objects =
            m_scene_selection_mgr.getState().count() + m_table_selection_mgr.getState().count();

        const size_t total_selected_rails_and_nodes = m_rail_selection_mgr.getState().count();

        m_selection_transforms.clear();

        if (total_selected_objects > 0) {
            m_selection_transforms.reserve(total_selected_objects);

            for (const ModelIndex &index : m_scene_selection_mgr.getState().getSelection()) {
                RefPtr<ISceneObject> obj               = m_scene_object_model->getObjectRef(index);
                std::optional<Transform> obj_transform = obj->getTransform();
                if (!obj_transform) {
                    continue;
                }

                m_selection_transforms.emplace_back(obj_transform.value());
                combined_transform.m_translation += m_selection_transforms.back().m_translation;
            }

            for (const ModelIndex &index : m_table_selection_mgr.getState().getSelection()) {
                RefPtr<ISceneObject> obj               = m_table_object_model->getObjectRef(index);
                std::optional<Transform> obj_transform = obj->getTransform();
                if (!obj_transform) {
                    continue;
                }

                m_selection_transforms.emplace_back(obj_transform.value());
                combined_transform.m_translation += m_selection_transforms.back().m_translation;
            }

            combined_transform.m_translation /= total_selected_objects;
            m_renderer.setGizmoTransform(combined_transform);
        } else if (total_selected_rails_and_nodes > 0) {
            m_selection_transforms.reserve(total_selected_rails_and_nodes);

            for (const ModelIndex &index : m_rail_selection_mgr.getState().getSelection()) {
                glm::vec3 center;
                if (m_rail_model->isIndexRail(index)) {
                    RailData::rail_ptr_t rail = m_rail_model->getRailRef(index);
                    center                    = rail->getCenteroid();
                } else {
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);
                    center                      = node->getPosition();
                }
                m_selection_transforms.push_back(
                    Transform(center, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}));
                combined_transform.m_translation += center;
            }

            combined_transform.m_translation /= total_selected_rails_and_nodes;
            m_renderer.setGizmoTransform(combined_transform);
        }
    }

    void Toolbox::UI::SceneWindow::_moveNode(const Rail::RailNode &node, size_t index,
                                             UUID64 rail_id, size_t orig_index, UUID64 orig_id,
                                             bool is_internal) {
        RefPtr<RailData> data                   = m_current_scene->getRailData();
        std::vector<RailData::rail_ptr_t> rails = data->rails();

        auto new_rail_it = std::find_if(rails.begin(), rails.end(), [&](RailData::rail_ptr_t rail) {
            return rail->getUUID() == rail_id;
        });

        if (new_rail_it == rails.end()) {
            LogError(
                make_error<void>("Scene Hierarchy", "Failed to find rail to move node to").error());
            return;
        }

        if (is_internal) {
            auto orig_rail_it =
                std::find_if(rails.begin(), rails.end(),
                             [&](RailData::rail_ptr_t rail) { return rail->getUUID() == orig_id; });

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

    void SceneWindow::setStageScenario(u8 stage, u8 scenario) {
        m_stage = stage, m_scenario = scenario;
    }

    ImGuiID SceneWindow::onBuildDockspace() {
        ImGuiID dockspace_id = ImGui::GetID(std::to_string(getUUID()).c_str());
        ImGui::DockBuilderAddNode(dockspace_id);
        {
            ImGuiID other_node_id;
            m_dock_node_left_id    = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                                                 nullptr, &other_node_id);
            m_dock_node_up_left_id = ImGui::DockBuilderSplitNode(
                m_dock_node_left_id, ImGuiDir_Down, 0.5f, nullptr, &m_dock_node_left_id);
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
        ImGuiWindow *window = ImGui::GetCurrentWindow();

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File", true)) {
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save")) {
                    m_is_save_default_ready = true;
                }
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...")) {
                    m_is_save_as_dialog_open = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Validation")) {
                const bool is_verifying = m_scene_verifier && m_scene_verifier->tIsAlive();
                const bool is_repairing = m_scene_mender && m_scene_mender->tIsAlive();

                if (is_verifying || is_repairing) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::MenuItem("Verify Scene")) {
                    m_scene_verifier = make_scoped<ToolboxSceneVerifier>(
                        ref_cast<const SceneInstance>(m_current_scene), false);
                    m_scene_verifier->tStart(true, nullptr);
                    m_scene_validator_result_opened = false;
                }

                if (ImGui::MenuItem("Verify Scene & Dependencies")) {
                    m_scene_verifier = make_scoped<ToolboxSceneVerifier>(
                        ref_cast<const SceneInstance>(m_current_scene), true);
                    m_scene_verifier->tStart(true, nullptr);
                    m_scene_validator_result_opened = false;
                }

                if (ImGui::MenuItem("Repair Dependencies")) {
                    m_scene_mender = make_scoped<ToolboxSceneDependencyMender>(
                        ref_cast<const SceneInstance>(m_current_scene));
                    m_scene_mender->tStart(true, nullptr);
                    m_scene_mender_result_opened = false;
                }

                if (is_verifying || is_repairing) {
                    ImGui::EndDisabled();
                }

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (m_is_save_default_ready) {
            m_is_save_default_ready = false;
            if (!m_io_context_path.empty()) {
                if (onSaveData(m_io_context_path)) {
                    MainApplication::instance().showSuccessModal(this, name(),
                                                                 "Scene saved successfully!");
                } else {
                    MainApplication::instance().showErrorModal(this, name(),
                                                               "Scene failed to save!");
                }
            } else {
                m_is_save_as_dialog_open = true;
            }
        }

        if (m_is_save_as_dialog_open) {
            if (!FileDialog::instance()->isAlreadyOpen()) {
                FileDialog::instance()->saveDialog(*this, m_io_context_path, "scene", true);
            }
            m_is_save_as_dialog_open = false;
        }

        if (FileDialog::instance()->isDone(*this)) {
            FileDialog::instance()->close();
            if (FileDialog::instance()->isOk()) {
                switch (FileDialog::instance()->getFilenameMode()) {
                case FileDialog::FileNameMode::MODE_SAVE: {
                    fs_path selected_path = FileDialog::instance()->getFilenameResult();

                    if (selected_path.empty()) {
                        selected_path = m_io_context_path;
                    }

                    auto dir_result =
                        Toolbox::Filesystem::is_directory(selected_path).value_or(false);
                    if (!dir_result) {
                        return;
                    }

                    m_io_context_path = selected_path;
                    if (onSaveData(m_io_context_path)) {
                        MainApplication::instance().showSuccessModal(this, name(),
                                                                     "Scene saved successfully!");
                    } else {
                        MainApplication::instance().showErrorModal(this, name(),
                                                                   "Scene failed to save!");
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

}  // namespace Toolbox::UI

void ToolboxSceneVerifier::tRun(void *param) {
    m_successful = m_scene->validate(
        m_check_dependencies,
        [this](double progress, const std::string &progress_text) {
            setProgress(progress);
            m_progress_text = progress_text;
        },
        [this](const std::string &error_msg) { m_errors.push_back(error_msg); });
}

void ToolboxSceneDependencyMender::tRun(void *param) {
    m_successful = m_scene->repair(
        true, [this](const std::string &progress_text) { m_progress_text = progress_text; },
        [this](const std::string &change_msg) { m_changes.push_back(change_msg); },
        [this](const std::string &error_msg) { m_errors.push_back(error_msg); });
}
