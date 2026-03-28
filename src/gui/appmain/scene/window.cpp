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

constexpr float TreeNodeFramePaddingY = 4.0f;

class SceneValidator {
public:
    using size_fn        = std::function<bool(size_t)>;
    using foreach_obj_fn = std::function<void(RefPtr<SceneObjModel>, ModelIndex)>;

public:
    SceneValidator(RefPtr<SceneObjModel> object_model, RefPtr<SceneObjModel> table_model,
                   RefPtr<RailObjModel> rail_model, bool check_dependencies = true)
        : m_object_model(object_model), m_table_model(table_model), m_rail_model(rail_model),
          m_check_dependencies(check_dependencies), m_valid(true) {
        m_total_objects =
            static_cast<double>(object_model->getObjectCount() + table_model->getObjectCount());
    }

public:
    SceneValidator()                           = delete;
    SceneValidator(const SceneValidator &)     = delete;
    SceneValidator(SceneValidator &&) noexcept = delete;

public:
    void setProgressCallback(ToolboxSceneVerifier::validate_progress_cb cb) {
        m_progress_callback = cb;
    }
    void setErrorCallback(ToolboxSceneVerifier::validate_error_cb cb) { m_error_callback = cb; }

    // Test that an object exists
    SceneValidator &scopeAssertObject(const std::string &obj_type, const std::string &obj_name);

    // Test that the group scope is a given size
    SceneValidator &scopeAssertSize(size_fn predicate);
    SceneValidator &scopeEnter();
    SceneValidator &scopeEscape();

    SceneValidator &scopeValidateDependencies(RefPtr<SceneObjModel> model, const ModelIndex &index);
    SceneValidator &scopeForAll(RefPtr<SceneObjModel> model, foreach_obj_fn);

    explicit operator bool() const { return m_valid; }

private:
    ToolboxSceneVerifier::validate_progress_cb m_progress_callback;
    ToolboxSceneVerifier::validate_error_cb m_error_callback;

    std::stack<ModelIndex> m_parent_stack;
    ModelIndex m_last_index;

    RefPtr<SceneObjModel> m_object_model;
    RefPtr<SceneObjModel> m_table_model;
    RefPtr<RailObjModel> m_rail_model;

    bool m_check_dependencies;

    bool m_valid;

    double m_processed_objects = 0;
    double m_total_objects     = 0;
};

#define VALIDATOR_LT(x) [](size_t v) -> bool { return v < (x); }
#define VALIDATOR_LE(x) [](size_t v) -> bool { return v <= (x); }
#define VALIDATOR_GT(x) [](size_t v) -> bool { return v > (x); }
#define VALIDATOR_GE(x) [](size_t v) -> bool { return v >= (x); }
#define VALIDATOR_NE(x) [](size_t v) -> bool { return v != (x); }
#define VALIDATOR_EQ(x) [](size_t v) -> bool { return v == (x); }

class SceneMender {
public:
    using size_fn        = std::function<bool(size_t)>;
    using foreach_obj_fn = std::function<void(RefPtr<SceneObjModel>, ModelIndex)>;

public:
    SceneMender(RefPtr<SceneObjModel> object_model, RefPtr<SceneObjModel> table_model,
                RefPtr<RailObjModel> rail_model, bool check_dependencies = true)
        : m_object_model(object_model), m_table_model(table_model), m_rail_model(rail_model),
          m_check_dependencies(check_dependencies), m_valid(true) {}

public:
    SceneMender()                        = delete;
    SceneMender(const SceneMender &)     = delete;
    SceneMender(SceneMender &&) noexcept = delete;

public:
    void setProgressCallback(ToolboxSceneDependencyMender::repair_progress_cb cb) {
        m_progress_callback = cb;
    }
    void setChangeCallback(ToolboxSceneDependencyMender::repair_change_cb cb) {
        m_change_callback = cb;
    }
    void setErrorCallback(ToolboxSceneDependencyMender::repair_error_cb cb) {
        m_error_callback = cb;
    }

    // Test that an object exists
    SceneMender &scopeTryCreateObjectDefault(const std::string &obj_type,
                                             const std::string &obj_name);

    SceneMender &scopeEnter();
    SceneMender &scopeEscape();

    SceneMender &scopeFulfillManagerDependencies(RefPtr<SceneObjModel> model,
                                                 const ModelIndex &index);
    SceneMender &scopeFulfillTableBinDependencies(RefPtr<SceneObjModel> model,
                                                  const ModelIndex &index);
    SceneMender &scopeFulfillRailDependencies(RefPtr<SceneObjModel> model, const ModelIndex &index);
    SceneMender &scopeFulfillAssetDependencies(RefPtr<SceneObjModel> model,
                                               const ModelIndex &index);
    SceneMender &scopeForAll(RefPtr<SceneObjModel> model, foreach_obj_fn);

    explicit operator bool() const { return m_valid; }

private:
    ToolboxSceneDependencyMender::repair_progress_cb m_progress_callback;
    ToolboxSceneDependencyMender::repair_change_cb m_change_callback;
    ToolboxSceneDependencyMender::repair_error_cb m_error_callback;

    std::stack<ModelIndex> m_parent_stack;
    ModelIndex m_last_index;

    RefPtr<SceneObjModel> m_object_model;
    RefPtr<SceneObjModel> m_table_model;
    RefPtr<RailObjModel> m_rail_model;

    bool m_check_dependencies;

    bool m_valid;

    int m_processed_objects = 0;
};

class ScenePruner {
public:
    using size_fn         = std::function<bool(size_t)>;
    using foreach_obj_fn  = std::function<void(RefPtr<SceneObjModel>, ModelIndex)>;
    using foreach_rail_fn = std::function<void(RefPtr<RailObjModel>, ModelIndex)>;

public:
    ScenePruner(RefPtr<SceneObjModel> object_model, RefPtr<SceneObjModel> table_model,
                RefPtr<RailObjModel> rail_model, bool check_dependencies = true)
        : m_object_model(object_model), m_table_model(table_model), m_rail_model(rail_model),
          m_check_dependencies(check_dependencies), m_valid(true) {}

public:
    ScenePruner()                        = delete;
    ScenePruner(const ScenePruner &)     = delete;
    ScenePruner(ScenePruner &&) noexcept = delete;

public:
    void setProgressCallback(ToolboxScenePruner::prune_progress_cb cb) { m_progress_callback = cb; }
    void setChangeCallback(ToolboxScenePruner::prune_change_cb cb) { m_change_callback = cb; }
    void setErrorCallback(ToolboxScenePruner::prune_error_cb cb) { m_error_callback = cb; }

    ScenePruner &finalize();

    ScenePruner &addManagerDependency(const TemplateDependencies::ObjectInfo &manager_info);
    ScenePruner &addRailDependency(const std::string &rail_name);

    ScenePruner &scopeAssertObject(const std::string &obj_type, const std::string &obj_name);

    ScenePruner &scopeEnter();
    ScenePruner &scopeEscape();

    ScenePruner &scopeCollectDependencies(RefPtr<SceneObjModel> model, const ModelIndex &index);
    ScenePruner &scopePruneManagerIfUnused(RefPtr<SceneObjModel> model, const ModelIndex &index);
    ScenePruner &scopePruneRailIfUnused(RefPtr<RailObjModel> model, const ModelIndex &index);

    ScenePruner &scopeForAll(RefPtr<SceneObjModel> model, foreach_obj_fn);
    ScenePruner &scopeForAll(RefPtr<RailObjModel> model, foreach_rail_fn);

    explicit operator bool() const { return m_valid; }

protected:
    struct ToolboxScopeDepenedencies {
        std::vector<TemplateDependencies::ObjectInfo> m_managers;
        std::unordered_set<std::string> m_rails;
        std::unordered_set<fs_path> m_asset_paths;
    };

private:
    ToolboxScenePruner::prune_progress_cb m_progress_callback;
    ToolboxScenePruner::prune_change_cb m_change_callback;
    ToolboxScenePruner::prune_error_cb m_error_callback;

    std::stack<ModelIndex> m_parent_stack;
    ModelIndex m_last_index;

    RefPtr<SceneObjModel> m_object_model;
    RefPtr<SceneObjModel> m_table_model;
    RefPtr<RailObjModel> m_rail_model;

    std::vector<ModelIndex> m_indexes_to_prune;

    ToolboxScopeDepenedencies m_dependencies;

    bool m_check_dependencies;

    bool m_valid;

    int m_processed_objects = 0;
};

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

                m_scene_object_model->setScenePath(*m_current_scene->rootPath());
                m_table_object_model->setScenePath(*m_current_scene->rootPath());

                m_scene_selection_mgr = ModelSelectionManager(m_scene_object_model);
                m_table_selection_mgr = ModelSelectionManager(m_table_object_model);
                m_rail_selection_mgr  = ModelSelectionManager(m_rail_model);

                m_scene_selection_mgr.setDeepSpans(false);
                m_table_selection_mgr.setDeepSpans(false);
                m_rail_selection_mgr.setDeepSpans(false);

                // Initialize the rail visibility map
                const int64_t rail_count = m_rail_model->getRowCount(ModelIndex());
                for (int64_t i = 0; i < rail_count; ++i) {
                    ModelIndex rail_index                    = m_rail_model->getIndex(i, 0);
                    m_rail_visible_map[rail_index.getUUID()] = true;
                }

                m_scene_object_model->addEventListener(
                    getUUID(),
                    [&](ModelIndex index, int flags) {
                        if ((flags & ModelEventFlags::EVENT_INDEX_ADDED) ==
                            ModelEventFlags::EVENT_INDEX_ADDED) {
                            m_selection_transforms_update_requested = true;
                            return;
                        }

                        if ((flags & ModelEventFlags::EVENT_INDEX_REMOVED) ==
                            ModelEventFlags::EVENT_INDEX_REMOVED) {
                            m_selection_transforms_update_requested = true;
                            return;
                        }
                    },
                    ModelEventFlags::EVENT_INDEX_ANY);

                m_table_object_model->addEventListener(
                    getUUID(),
                    [&](ModelIndex index, int flags) {
                        if ((flags & ModelEventFlags::EVENT_INDEX_ADDED) ==
                            ModelEventFlags::EVENT_INDEX_ADDED) {
                            m_selection_transforms_update_requested = true;
                            return;
                        }

                        if ((flags & ModelEventFlags::EVENT_INDEX_REMOVED) ==
                            ModelEventFlags::EVENT_INDEX_REMOVED) {
                            m_selection_transforms_update_requested = true;
                            return;
                        }
                    },
                    ModelEventFlags::EVENT_INDEX_ANY);

                m_rail_model->addEventListener(
                    getUUID(),
                    [&](ModelIndex index, int flags) {
                        m_path_renderer_update_reqeusted = true;

                        if ((flags & ModelEventFlags::EVENT_INDEX_ADDED) ==
                            ModelEventFlags::EVENT_INDEX_ADDED) {
                            if (m_rail_model->isIndexRail(index)) {
                                m_rail_visible_map_update_request_index = index;
                            }
                            m_selection_transforms_update_requested = true;
                            return;
                        }

                        if ((flags & ModelEventFlags::EVENT_INDEX_REMOVED) ==
                            ModelEventFlags::EVENT_INDEX_REMOVED) {
                            m_selection_transforms_update_requested = true;
                            return;
                        }
                    },
                    ModelEventFlags::EVENT_INDEX_ANY);

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

        // --
        // Here we bake the changes to our scene data
        // --
        RefPtr<Scene::ObjectHierarchy> objects_to_save =
            m_scene_object_model->bakeToHierarchy("Map");
        RefPtr<Scene::ObjectHierarchy> tables_to_save =
            m_table_object_model->bakeToHierarchy("Table");
        RefPtr<RailData> rails_to_save = m_rail_model->bakeToRailData();

        m_current_scene->setObjHierarchy(objects_to_save);
        m_current_scene->setTableHierarchy(tables_to_save);
        m_current_scene->setRailData(rails_to_save);
        // --

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

        std::unique_lock<std::mutex> lock(m_scene_pruner.getOperationMutex());

        ModelIndex rail_visibility_index = m_rail_visible_map_update_request_index;
        if (m_rail_model->isIndexRail(rail_visibility_index)) {
            m_rail_visible_map[rail_visibility_index.getUUID()] = true;
            m_rail_visible_map_update_request_index             = ModelIndex();
        }

        if (m_path_renderer_update_reqeusted.load()) {
            m_renderer.updatePaths(m_rail_model, m_rail_visible_map);
            m_path_renderer_update_reqeusted.store(false);
        }

        return;
    }

    void SceneWindow::onImGuiPostUpdate(TimeStep delta_time) {
        std::unique_lock<std::mutex> lock(m_scene_pruner.getOperationMutex());

        const AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        if (m_current_scene) {
            std::vector<RefPtr<Rail::RailNode>> rendered_nodes;
            const int64_t rail_count = m_rail_model->getRowCount(ModelIndex());
            for (int64_t row = 0; row < rail_count; ++row) {
                ModelIndex rail_index = m_rail_model->getIndex(row, 0);
                if (!m_rail_visible_map[rail_index.getUUID()])
                    continue;
                const int64_t node_count = m_rail_model->getRowCount(rail_index);
                for (int64_t node_row = 0; node_row < node_count; ++node_row) {
                    ModelIndex node_index = m_rail_model->getIndex(node_row, 0, rail_index);
                    rendered_nodes.push_back(m_rail_model->getRailNodeRef(node_index));
                }
            }

            if (m_is_render_window_open && (!m_renderer.getGizmoVisible() || !ImGuizmo::IsOver())) {
                bool should_reset = false;
                Renderer::selection_variant_t selection =
                    m_renderer.findSelection(m_renderables, rendered_nodes, should_reset);

                const bool multi_select = Input::GetKey(KeyCode::KEY_LEFTCONTROL);
                const bool is_mouse_up = Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT) ||
                                         Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT);

                if (std::holds_alternative<std::monostate>(selection)) {
                    if (should_reset) {
                        m_scene_selection_mgr.getState().clearSelection();
                        m_table_selection_mgr.getState().clearSelection();
                        m_rail_selection_mgr.getState().clearSelection();

                        m_selected_properties.clear();
                        m_properties_render_handler = renderEmptyProperties;
                    }
                } else if (std::holds_alternative<RefPtr<ISceneObject>>(selection)) {
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
                        return std::all_of(
                            indexes.begin(), indexes.end(), [this](const ModelIndex &node) {
                                RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(node);
                                return obj && obj->getTransform().has_value();
                            });
                    }
                    if (m_table_selection_mgr.getState().count() > 0) {
                        const IDataModel::index_container &indexes =
                            m_table_selection_mgr.getState().getSelection();
                        return std::all_of(
                            indexes.begin(), indexes.end(), [this](const ModelIndex &node) {
                                RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(node);
                                return obj && obj->getTransform().has_value();
                            });
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

        if (m_selection_transforms_update_requested.load()) {
            calcNewGizmoMatrixFromSelection();
            m_selection_transforms_update_requested.store(false);
        }

        bool should_update_paths =
            m_renderer.isUniqueRailColors() != settings.m_is_unique_rail_color;

        if (m_renderer.isGizmoManipulated() &&
            Input::GetMouseButton(Input::MouseButton::BUTTON_LEFT)) {
            glm::mat4x4 gizmo_total_delta = m_renderer.getGizmoTotalDelta();

            // Scene object transform manipulation
            {
                const IDataModel::index_container &obj_indexes =
                    m_scene_selection_mgr.getState().getSelection();

                for (const ModelIndex &index : obj_indexes) {
                    RefPtr<ISceneObject> obj = m_scene_object_model->getObjectRef(index);
                    if (!obj || !obj->getTransform()) {
                        continue;
                    }
                    const Transform &obj_transform = m_selection_transforms[index.getUUID()];
                    Transform new_transform        = gizmo_total_delta * obj_transform;
                    obj->setTransform(new_transform);
                }
            }

            // Table object transform manipulation
            {
                const IDataModel::index_container &obj_indexes =
                    m_table_selection_mgr.getState().getSelection();

                for (const ModelIndex &index : obj_indexes) {
                    RefPtr<ISceneObject> obj = m_table_object_model->getObjectRef(index);
                    if (!obj || !obj->getTransform()) {
                        continue;
                    }
                    const Transform &obj_transform = m_selection_transforms[index.getUUID()];
                    Transform new_transform        = gizmo_total_delta * obj_transform;
                    obj->setTransform(new_transform);
                }
            }

            {
                const IDataModel::index_container &rail_indexes =
                    m_rail_selection_mgr.getState().getSelection();

                glm::mat4x4 gizmo_delta   = m_renderer.getGizmoFrameDelta();
                Transform delta_transform = Transform::FromMat4x4(gizmo_delta);

                for (const ModelIndex &index : rail_indexes) {
                    RailData::rail_ptr_t rail = m_rail_model->getRailRef(index);

                    const bool is_node = m_rail_model->isIndexRailNode(index);
                    if (is_node) {
                        Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);

                        const Transform &node_transform = m_selection_transforms[index.getUUID()];
                        Transform new_transform         = gizmo_total_delta * node_transform;

                        rail->setNodePosition(node, new_transform.m_translation);
                    } else {
                        const Transform &rail_transform = m_selection_transforms[index.getUUID()];
                        rail->transform(gizmo_delta);
                    }
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
            m_selection_transforms_update_requested = true;
            m_gizmo_maniped                         = false;
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
                    RailData::rail_ptr_t new_rail =
                        ref_cast<Rail::Rail>(event->getRail().clone(true));
                    int64_t insert_row        = m_rail_model->getRowCount(ModelIndex());
                    ModelIndex new_rail_index = m_rail_model->insertRail(new_rail, insert_row);
                    if (m_rail_model->validateIndex(new_rail_index)) {
                        m_rail_visible_map[new_rail_index.getUUID()] = true;
                    }
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
        std::unique_lock<std::mutex> lock(m_scene_pruner.getOperationMutex());

        renderHierarchy();
        renderProperties();
        renderRailEditor();
        renderScene(deltaTime);
        renderDolphin(deltaTime);
        renderSanitizationSteps();

        m_scene_hierarchy_context_menu.applyDeferredCmds();
        m_table_hierarchy_context_menu.applyDeferredCmds();
        m_rail_list_context_menu.applyDeferredCmds();

        m_scene_selection_ancestry_for_view.clear();
        m_rail_selection_ancestry_for_view.clear();
    }

    void SceneWindow::renderSanitizationSteps() {
        const ImGuiStyle &style = ImGui::GetStyle();

        if (m_scene_verifier.tIsAlive()) {
            ImGui::OpenPopup("Scene Validator");

            ImGui::SetNextWindowSize({400.0f, 200.0f});
            ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(),
                                    ImGuiCond_Appearing, {0.5f, 0.5f});

            if (ImGui::BeginPopupModal("Scene Validator", nullptr, ImGuiWindowFlags_NoResize)) {
                ImGui::Text("Validating scene, please wait...");
                ImGui::Separator();

                float progress            = m_scene_verifier.getProgress();
                std::string progress_text = m_scene_verifier.getProgressText();

                ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f), progress_text.c_str());
                ImGui::EndPopup();
            }
        } else if (m_scene_verifier.tIsKilled()) {
            if (!m_scene_validator_result_opened) {
                if (m_scene_verifier.isValid()) {
                    MainApplication::instance().showSuccessModal(
                        this, "Scene Validator Result",
                        "Scene validation completed successfully!");
                } else {
                    std::vector<std::string> errors = m_scene_verifier.getErrors();
                    MainApplication::instance().showErrorModal(
                        this, "Scene Validator Result", "Scene validation failed with errors!",
                        errors);
                }
                m_scene_validator_result_opened = true;
            }
        }

        if (m_scene_mender.tIsAlive()) {
            ImGui::OpenPopup("Scene Repair");

            ImGui::SetNextWindowSize({400.0f, 200.0f});
            ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(),
                                    ImGuiCond_Appearing, {0.5f, 0.5f});

            if (ImGui::BeginPopupModal("Scene Repair", nullptr, ImGuiWindowFlags_NoResize)) {
                ImGui::Text("Validating scene, please wait...");
                ImGui::Separator();

                std::string progress_text = m_scene_mender.getProgressText();

                ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-FLT_MIN, 0.0f),
                                    progress_text.c_str());
                ImGui::EndPopup();
            }
        } else if (m_scene_mender.tIsKilled()) {
            if (!m_scene_mender_result_opened) {
                if (m_scene_mender.isValid()) {
                    std::vector<std::string> changes = m_scene_mender.getChanges();
                    MainApplication::instance().showSuccessModal(
                        this, "Scene Repair Result",
                        changes.empty() ? "The scene was already valid!"
                                        : "Scene repair completed successfully!",
                        changes);
                } else {
                    std::vector<std::string> errors = m_scene_mender.getErrors();
                    MainApplication::instance().showErrorModal(
                        this, "Scene Repair Result", "Scene repair failed with errors!",
                        errors);
                }
                m_scene_mender_result_opened = true;
            }
        }

        if (m_scene_pruner.tIsAlive()) {
            ImGui::OpenPopup("Scene Pruner");

            ImGui::SetNextWindowSize({400.0f, 200.0f});
            ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(),
                                    ImGuiCond_Appearing, {0.5f, 0.5f});

            if (ImGui::BeginPopupModal("Scene Pruner", nullptr, ImGuiWindowFlags_NoResize)) {
                ImGui::Text("Pruning scene, please wait...");
                ImGui::Separator();

                std::string progress_text = m_scene_pruner.getProgressText();

                ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-FLT_MIN, 0.0f),
                                    progress_text.c_str());
                ImGui::EndPopup();
            }
        } else if (m_scene_pruner.tIsKilled()) {
            if (!m_scene_pruner_result_opened) {
                if (m_scene_pruner.isValid()) {
                    std::vector<std::string> changes = m_scene_pruner.getChanges();
                    MainApplication::instance().showSuccessModal(
                        this, "Scene Pruner Result",
                        changes.empty() ? "The scene was already pruned / efficient!"
                                        : "Scene prune completed successfully!",
                        changes);
                } else {
                    std::vector<std::string> errors = m_scene_pruner.getErrors();
                    MainApplication::instance().showErrorModal(
                        this, "Scene Pruner Result", "Scene prune failed with errors!", errors);
                }
                m_scene_pruner_result_opened = true;
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
                if (m_requested_object_scroll_y.has_value()) {
                    const float window_height   = ImGui::GetWindowHeight();
                    const float window_scroll_y = ImGui::GetScrollY();

                    if (fabsf(window_scroll_y - m_requested_object_scroll_y.value()) >
                        window_height / 2.0f) {
                        ImGui::SetScrollY(m_requested_object_scroll_y.value());
                    }
                    m_requested_object_scroll_y = std::nullopt;
                }

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

        if (m_current_scene) {
            // Scene dialogs
            {
                const ModelIndex &last_selected =
                    m_scene_selection_mgr.getState().getLastSelected();
                if (m_scene_object_model->validateIndex(last_selected)) {
                    m_create_scene_obj_dialog.render(m_scene_object_model, last_selected);
                    m_rename_scene_obj_dialog.render(last_selected);
                }
            }

            // Table dialogs
            {
                const ModelIndex &last_selected =
                    m_table_selection_mgr.getState().getLastSelected();
                if (m_table_object_model->validateIndex(last_selected)) {
                    m_create_table_obj_dialog.render(m_table_object_model, last_selected);
                    m_rename_table_obj_dialog.render(last_selected);
                }
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

        std::string display_name = m_scene_object_model->getDisplayText(index);
        bool is_filtered_out     = !m_hierarchy_filter.PassFilter(display_name.c_str());

        ImGuiID tree_node_id = static_cast<ImGuiID>(node->getUUID());

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

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, TreeNodeFramePaddingY});

        const bool this_node_is_view_ancestry =
            !m_scene_selection_ancestry_for_view.empty() &&
            std::any_of(m_scene_selection_ancestry_for_view.begin(),
                        m_scene_selection_ancestry_for_view.end(),
                        [index](const ModelIndex &other) { return other == index; });

        if (this_node_is_view_ancestry) {
            if (is_filtered_out) {
                m_scene_selection_ancestry_for_view.pop_back();
                TOOLBOX_DEBUG_LOG("Ancestor of scene view selection is filtered!");
            } else if (node->isGroupObject()) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                TOOLBOX_DEBUG_LOG("Forcing ancestor of scene view selection to be open!");
            }
        }

        ImRect node_rect;

        if (node->isGroupObject()) {
            if (is_filtered_out) {
                for (size_t i = 0; i < m_scene_object_model->getRowCount(index); ++i) {
                    ModelIndex child_index = m_scene_object_model->getIndex(i, 0, index);
                    renderSceneObjectTree(child_index);
                }
            } else {

                if (node_visibility) {
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected);
                }

                m_tree_node_open_map[index] = node_open;

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
                            regeneratePropertiesForObject(node);
                        }

                        m_selection_transforms_update_requested = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderSceneHierarchyContextMenu(display_name, index);

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
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected);
                }

                m_tree_node_open_map[index] = node_open;

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
                            regeneratePropertiesForObject(node);
                        }

                        m_selection_transforms_update_requested = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderSceneHierarchyContextMenu(display_name, index);

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

        std::string display_name = m_table_object_model->getDisplayText(index);
        bool is_filtered_out     = !m_hierarchy_filter.PassFilter(display_name.c_str());

        ImGuiID tree_node_id = static_cast<ImGuiID>(node->getUUID());

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

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, TreeNodeFramePaddingY});

        ImRect node_rect;

        if (node->isGroupObject()) {
            if (is_filtered_out) {
                for (size_t i = 0; i < m_table_object_model->getRowCount(index); ++i) {
                    ModelIndex child_index = m_table_object_model->getIndex(i, 0, index);
                    renderTableObjectTree(child_index);
                }
            } else {
                if (node_visibility) {
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected);
                }

                m_tree_node_open_map[index] = node_open;

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
                            regeneratePropertiesForObject(node);
                        }

                        m_selection_transforms_update_requested = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderTableHierarchyContextMenu(display_name, index);

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
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected,
                                                  &node_visible);
                    if (node->getIsPerforming() != node_visible) {
                        node->setIsPerforming(node_visible);
                        m_update_render_objs = true;
                    }
                } else {
                    node_open = ImGui::TreeNodeEx(display_name.c_str(), the_flags, node_selected);
                }

                m_tree_node_open_map[index] = node_open;

                node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

                // Drag and drop for OBJECT
                {
                    ImVec2 mouse_pos = ImGui::GetMousePos();
                    ImVec2 item_size = ImGui::GetItemRectSize();
                    ImVec2 item_pos  = ImGui::GetItemRectMin();

                    // if (ImGui::BeginDragDropSource()) {
                    //     int64_t node_index = m_table_object_model->getRow(index);

                    //    Toolbox::Buffer buffer;
                    //    saveMimeObject(buffer, node_index, get_shared_ptr(*node->getParent()));
                    //    ImGui::SetDragDropPayload("toolbox/scene/object", buffer.buf(),
                    //                              buffer.size(), ImGuiCond_Once);
                    //    ImGui::Text("Object: %s", node->getNameRef().name().data());
                    //    ImGui::EndDragDropSource();
                    //}

                    // ImGuiDropFlags drop_flags = ImGuiDropFlags_None;
                    // if (mouse_pos.y < item_pos.y + (item_size.y / 2)) {
                    //     drop_flags = ImGuiDropFlags_InsertBefore;
                    // } else {
                    //     drop_flags = ImGuiDropFlags_InsertAfter;
                    // }

                    // if (ImGui::BeginDragDropTarget()) {
                    //     if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                    //             "toolbox/scene/object",
                    //             ImGuiDragDropFlags_AcceptBeforeDelivery |
                    //                 ImGuiDragDropFlags_AcceptNoDrawDefaultRect |
                    //                 ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {

                    //        ImGui::RenderDragDropTargetRect(
                    //            ImGui::GetCurrentContext()->DragDropTargetRect,
                    //            ImGui::GetCurrentContext()->DragDropTargetClipRect, drop_flags);

                    //        if (payload->IsDelivery()) {
                    //            Toolbox::Buffer buffer;
                    //            buffer.setBuf(payload->Data, payload->DataSize);
                    //            buffer.copyTo(m_drop_target_buffer);

                    //            int64_t node_index = m_table_object_model->getRow(index);

                    //            // Calculate index based on position relative to center
                    //            m_object_drop_target = node_index;
                    //            if (drop_flags == ImGuiDropFlags_InsertAfter) {
                    //                m_object_drop_target++;
                    //            }
                    //            m_object_parent_uuid = node->getParent()->getUUID();
                    //        }
                    //    }
                    //    ImGui::EndDragDropTarget();
                    //}
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
                            regeneratePropertiesForObject(node);
                        }

                        m_selection_transforms_update_requested = true;
                    }

                    m_properties_render_handler = renderObjectProperties;
                }

                renderTableHierarchyContextMenu(display_name, index);

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

            if (m_requested_rail_scroll_y.has_value()) {
                const float window_height   = ImGui::GetWindowHeight();
                const float window_scroll_y = ImGui::GetScrollY();

                if (fabsf(window_scroll_y - m_requested_rail_scroll_y.value()) >
                    window_height / 2.0f) {
                    ImGui::SetScrollY(m_requested_rail_scroll_y.value());
                }
                m_requested_rail_scroll_y = std::nullopt;
            }

            const bool is_cut = false;

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, TreeNodeFramePaddingY});

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

                const bool this_node_is_view_ancestry =
                    !m_rail_selection_ancestry_for_view.empty() &&
                    std::any_of(
                        m_rail_selection_ancestry_for_view.begin(),
                        m_rail_selection_ancestry_for_view.end(),
                        [rail_index](const ModelIndex &other) { return other == rail_index; });

                if (this_node_is_view_ancestry) {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                    TOOLBOX_DEBUG_LOG("Forcing ancestor of scene view selection to be open!");
                }

                RailData::rail_ptr_t rail = m_rail_model->getRailRef(rail_index);

                const std::string uid_str = getNodeUID(rail);
                const ImGuiID rail_id     = static_cast<ImGuiID>(rail->getUUID());
                const UUID64 rail_uuid    = rail_index.getUUID();

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

                    // if (ImGui::BeginDragDropSource()) {
                    //     Toolbox::Buffer buffer;
                    //     saveMimeRail(buffer, i);
                    //     ImGui::SetDragDropPayload("toolbox/scene/rail", buffer.buf(),
                    //     buffer.size(),
                    //                               ImGuiCond_Once);
                    //     ImGui::Text("Rail: %s", rail->name().c_str());
                    //     ImGui::EndDragDropSource();
                    // }

                    // if (ImGui::BeginDragDropTarget()) {
                    //     if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                    //             "toolbox/scene/rail",
                    //             ImGuiDragDropFlags_AcceptBeforeDelivery |
                    //                 ImGuiDragDropFlags_SourceNoHoldToOpenOthers);
                    //         payload && payload->IsDelivery()) {
                    //         Toolbox::Buffer buffer;
                    //         buffer.setBuf(payload->Data, payload->DataSize);
                    //         buffer.copyTo(m_drop_target_buffer);

                    //        // Calculate index based on position relative to center
                    //        m_rail_drop_target = i;
                    //        if (mouse_pos.y > item_pos.y + (item_size.y / 2)) {
                    //            m_rail_drop_target++;
                    //        }
                    //    }
                    //    ImGui::EndDragDropTarget();
                    //}
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

                        m_selection_transforms_update_requested = true;

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

                                m_selection_transforms_update_requested = true;

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

            // for (const ISceneObject::RenderInfo &render_info : m_renderables) {
            //     ModelIndex obj_index = m_scene_object_model->getIndex(render_info.m_object);
            //     const bool is_selected = m_scene_selection_mgr.getState().isSelected(obj_index);
            //     for (RefPtr<J3DMaterial> material : render_info.m_model->GetMaterials()) {
            //         material->SetSelected(is_selected);
            //     }
            // }
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

            m_renderer.render(m_renderables, delta_time, !settings.m_is_rendering_simple);

            renderScenePeripherals(delta_time);
            renderPlaybackButtons(delta_time);

            if (m_wants_scene_context_menu) {
                if (m_scene_hierarchy_context_menu.tryOpen(0)) {
                    m_wants_scene_context_menu = false;
                }
            }

            if (m_wants_rail_context_menu) {
                if (m_rail_list_context_menu.tryOpen(0)) {
                    m_wants_rail_context_menu = false;
                }
            }

            if (m_scene_selection_mgr.getState().count() > 0) {
                m_scene_hierarchy_context_menu.tryRender(
                    m_scene_selection_mgr.getState().getLastSelected());
            } else if (m_table_selection_mgr.getState().count() > 0) {
                m_table_hierarchy_context_menu.tryRender(
                    m_table_selection_mgr.getState().getLastSelected());
            } else if (m_rail_selection_mgr.getState().count() > 0) {
                m_rail_list_context_menu.tryRender(
                    m_rail_selection_mgr.getState().getLastSelected());
            }

            // Stupid ImGui keeps crashing when SetCursorPos goes past the window
            ImGui::Dummy({0.0f, 0.0f});
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
                "Insert Node", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1;
                },
                [this](ModelIndex index) {
                    ModelIndex parent_index = m_rail_model->getParent(index);

                    Rail::Rail::node_ptr_t new_node = std::make_shared<Rail::RailNode>();

                    ModelIndex result;
                    if (m_rail_model->validateIndex(parent_index)) {
                        // This means a node is selected
                        int64_t sibling_row = m_rail_model->getRow(index);
                        result = m_rail_model->insertRailNode(new_node, sibling_row, parent_index);
                    } else {
                        // This means a rail is selected, so we insert at the end of the rail
                        int64_t child_count = m_rail_model->getRowCount(index);
                        result = m_rail_model->insertRailNode(new_node, child_count, index);
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
                "Insert Node At Camera",
                {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTALT, KeyCode::KEY_N},
                [this](ModelIndex index) -> bool {
                    return m_rail_selection_mgr.getState().count() == 1;
                },
                [this](ModelIndex index) {
                    ModelIndex parent_index = m_rail_model->getParent(index);

                    glm::vec3 translation;
                    m_renderer.getCameraTranslation(translation);
                    Rail::Rail::node_ptr_t new_node = std::make_shared<Rail::RailNode>(translation);

                    ModelIndex result;
                    if (m_rail_model->validateIndex(parent_index)) {
                        // This means a node is selected
                        int64_t sibling_row = m_rail_model->getRow(index);
                        result = m_rail_model->insertRailNode(new_node, sibling_row, parent_index);
                    } else {
                        // This means a rail is selected, so we insert at the end of the rail
                        int64_t child_count = m_rail_model->getRowCount(index);
                        result = m_rail_model->insertRailNode(new_node, child_count, index);
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
                    return selection.size() > 1 &&
                           std::all_of(
                               selection.begin(), selection.end(),
                               [this, &rail_uuid](const ModelIndex &sel_index) -> bool {
                                   const bool is_valid_node =
                                       m_rail_model->isIndexRailNode(sel_index);
                                   const bool is_same_rail =
                                       m_rail_model->getRailNodeRef(sel_index)->getRailUUID() ==
                                       rail_uuid;
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
                        auto result                     = rail->connectNodeToNext(sel_node);
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
                m_scene_object_model->setObjectKey(selected_index, new_unique_name);

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
                m_table_object_model->setObjectKey(selected_index, new_unique_name);

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
        m_create_rail_dialog.setActionOnAccept(
            [this](std::string_view name, u16 node_count, s16 node_distance, bool loop) {
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

                ModelIndex new_rail_index = m_rail_model->insertRail(
                    std::move(new_rail), m_rail_model->getRowCount(ModelIndex()));
                if (m_rail_model->validateIndex(new_rail_index)) {
                    m_rail_selection_mgr.actionSelectIndex(new_rail_index, true);
                    m_update_render_objs = true;
                }
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

                m_rail_model->setObjectKey(index, std::string(new_name));
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
        Rail::Rail rail("(null)");

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

        if (Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT)) {
            m_scene_selection_mgr.actionSelectIndex(new_obj_selection, !is_multi, false, true);

            ModelIndex tmp = new_obj_selection;
            m_scene_selection_ancestry_for_view.clear();
            while (m_scene_object_model->validateIndex(tmp)) {
                m_scene_selection_ancestry_for_view.emplace_back(tmp);
                tmp = m_scene_object_model->getParent(tmp);
            }
        }

        if (Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT)) {
            m_scene_selection_mgr.actionSelectIndexIfNew(new_obj_selection, true);

            ModelIndex tmp = new_obj_selection;
            m_scene_selection_ancestry_for_view.clear();
            while (m_scene_object_model->validateIndex(tmp)) {
                m_scene_selection_ancestry_for_view.emplace_back(tmp);
                tmp = m_scene_object_model->getParent(tmp);
            }
        }

        m_requested_object_scroll_y = calculateFocusScrollForSceneObjectSelection();

        if (!Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) &&
            !Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
            m_table_selection_mgr.getState().clearSelection();
        }

        m_wants_scene_context_menu =
            true;  // Tells the program to check for context menu on potential right click

        m_selected_properties.clear();

        if (m_scene_selection_mgr.getState().getSelection().size() == 1) {
            regeneratePropertiesForObject(node);
        }

        m_selection_transforms_update_requested = true;

        m_properties_render_handler = renderObjectProperties;

        TOOLBOX_DEBUG_LOG_V("Hit object {} ({})", node->type(), node->getNameRef().name());
    }

    void SceneWindow::processRailSelection(RailData::rail_ptr_t node, bool is_multi) {
        ImGuiID rail_id = static_cast<ImGuiID>(node->getUUID());

        ModelIndex new_rail_selection = m_rail_model->getIndex(node);

        m_scene_selection_mgr.getState().clearSelection();
        m_table_selection_mgr.getState().clearSelection();

        if (Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT)) {
            m_rail_selection_mgr.actionSelectIndex(new_rail_selection, !is_multi, false, true);

            ModelIndex tmp = new_rail_selection;
            m_rail_selection_ancestry_for_view.clear();
            while (m_rail_model->validateIndex(tmp)) {
                m_rail_selection_ancestry_for_view.emplace_back(tmp);
                tmp = m_rail_model->getParent(tmp);
            }
        }

        if (Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT)) {
            m_rail_selection_mgr.actionSelectIndexIfNew(new_rail_selection, true);

            ModelIndex tmp = new_rail_selection;
            m_rail_selection_ancestry_for_view.clear();
            while (m_rail_model->validateIndex(tmp)) {
                m_rail_selection_ancestry_for_view.emplace_back(tmp);
                tmp = m_rail_model->getParent(tmp);
            }
        }

        m_requested_rail_scroll_y = calculateFocusScrollForSceneRailSelection();

        m_wants_rail_context_menu = true;

        m_selection_transforms_update_requested = true;

        m_selected_properties.clear();
        m_properties_render_handler = renderRailProperties;

        TOOLBOX_DEBUG_LOG_V("Hit rail \"{}\"", node->name());
    }

    void SceneWindow::processRailNodeSelection(RefPtr<Rail::RailNode> node, bool is_multi) {
        ImGuiID rail_id = static_cast<ImGuiID>(node->getUUID());

        ModelIndex new_rail_selection = m_rail_model->getIndex(node);

        m_scene_selection_mgr.getState().clearSelection();
        m_table_selection_mgr.getState().clearSelection();

        if (Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT)) {
            m_rail_selection_mgr.actionSelectIndex(new_rail_selection, !is_multi, false, true);

            ModelIndex tmp = new_rail_selection;
            m_rail_selection_ancestry_for_view.clear();
            while (m_rail_model->validateIndex(tmp)) {
                m_rail_selection_ancestry_for_view.emplace_back(tmp);
                tmp = m_rail_model->getParent(tmp);
            }
        }

        if (Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT)) {
            m_rail_selection_mgr.actionSelectIndexIfNew(new_rail_selection, true);

            ModelIndex tmp = new_rail_selection;
            m_rail_selection_ancestry_for_view.clear();
            while (m_rail_model->validateIndex(tmp)) {
                m_rail_selection_ancestry_for_view.emplace_back(tmp);
                tmp = m_rail_model->getParent(tmp);
            }
        }

        m_requested_rail_scroll_y = calculateFocusScrollForSceneRailSelection();

        m_wants_rail_context_menu = true;

        m_selection_transforms_update_requested = true;

        m_selected_properties.clear();
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

    void SceneWindow::calcNewGizmoMatrixFromSelection() {
        Transform combined_transform     = {};
        combined_transform.m_translation = {0.0f, 0.0f, 0.0f};
        combined_transform.m_rotation    = {0.0f, 0.0f, 0.0f};
        combined_transform.m_scale       = {1.0f, 1.0f, 1.0f};

        const size_t total_selected_objects =
            m_scene_selection_mgr.getState().count() + m_table_selection_mgr.getState().count();

        const size_t total_selected_rails_and_nodes = m_rail_selection_mgr.getState().count();

        m_selection_transforms.clear();

        if (total_selected_objects > 0) {
            for (const ModelIndex &index : m_scene_selection_mgr.getState().getSelection()) {
                RefPtr<ISceneObject> obj               = m_scene_object_model->getObjectRef(index);
                std::optional<Transform> obj_transform = obj->getTransform();
                if (!obj_transform) {
                    continue;
                }

                const Transform &transform = obj_transform.value();
                m_selection_transforms[index.getUUID()] = transform;
                combined_transform.m_translation += transform.m_translation;
            }

            for (const ModelIndex &index : m_table_selection_mgr.getState().getSelection()) {
                RefPtr<ISceneObject> obj               = m_table_object_model->getObjectRef(index);
                std::optional<Transform> obj_transform = obj->getTransform();
                if (!obj_transform) {
                    continue;
                }

                const Transform &transform              = obj_transform.value();
                m_selection_transforms[index.getUUID()] = transform;
                combined_transform.m_translation += transform.m_translation;
            }

            combined_transform.m_translation /= total_selected_objects;
            m_renderer.setGizmoTransform(combined_transform);
        } else if (total_selected_rails_and_nodes > 0) {
            for (const ModelIndex &index : m_rail_selection_mgr.getState().getSelection()) {
                glm::vec3 center;
                if (m_rail_model->isIndexRail(index)) {
                    RailData::rail_ptr_t rail = m_rail_model->getRailRef(index);
                    center                    = rail->getCenteroid();
                } else {
                    Rail::Rail::node_ptr_t node = m_rail_model->getRailNodeRef(index);
                    center                      = node->getPosition();
                }
                Transform transform = Transform(center, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
                m_selection_transforms[index.getUUID()] = transform;
                combined_transform.m_translation += transform.m_translation;
            }

            combined_transform.m_translation /= total_selected_rails_and_nodes;
            m_renderer.setGizmoTransform(combined_transform);
        }
    }

    std::optional<float> SceneWindow::calculateFocusScrollForSceneObjectSelection() {
        if (m_scene_selection_ancestry_for_view.empty()) {
            return std::nullopt;
        }

        float desired_scroll = 0.0f;

        const float row_height  = ImGui::GetFontSize() + TreeNodeFramePaddingY * 2.0f;
        const ImGuiStyle &style = ImGui::GetStyle();

        std::function<void(ModelIndex)> recursive_calc = [&](ModelIndex root) {
            desired_scroll += row_height + style.ItemSpacing.y;

            const bool is_node_open =
                m_tree_node_open_map.contains(root) ? m_tree_node_open_map[root] : true;
            if (!is_node_open) {
                return;
            }

            const int64_t row_count = m_scene_object_model->getRowCount(root);
            for (int64_t i = 0; i < row_count; ++i) {
                ModelIndex child_index = m_scene_object_model->getIndex(i, 0, root);
                if (!m_scene_object_model->validateIndex(child_index)) {
                    return;
                }
                recursive_calc(child_index);
            }
        };

        for (const ModelIndex &ancestor : m_scene_selection_ancestry_for_view) {
            const int64_t row_of_group = m_scene_object_model->getRow(ancestor);
            ModelIndex group_index     = m_scene_object_model->getParent(ancestor);

            for (int64_t i = 0; i < row_of_group; ++i) {
                ModelIndex sibling_index = m_scene_object_model->getIndex(i, 0, group_index);
                if (!m_scene_object_model->validateIndex(sibling_index)) {
                    return desired_scroll;
                }
                recursive_calc(sibling_index);
            }
        }

        return desired_scroll;
    }

    std::optional<float> SceneWindow::calculateFocusScrollForSceneRailSelection() {
        if (m_rail_selection_ancestry_for_view.empty()) {
            return std::nullopt;
        }

        float desired_scroll = 0.0f;

        const float row_height  = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        const ImGuiStyle &style = ImGui::GetStyle();

        std::function<void(ModelIndex)> recursive_calc = [&](ModelIndex root) {
            desired_scroll += row_height + style.ItemSpacing.y;

            const bool is_node_open =
                m_tree_node_open_map.contains(root) ? m_tree_node_open_map[root] : true;
            if (!is_node_open) {
                return;
            }

            const int64_t row_count = m_rail_model->getRowCount(root);
            for (int64_t i = 0; i < row_count; ++i) {
                ModelIndex child_index = m_rail_model->getIndex(i, 0, root);
                if (!m_rail_model->validateIndex(child_index)) {
                    return;
                }
                recursive_calc(child_index);
            }
        };

        for (const ModelIndex &ancestor : m_rail_selection_ancestry_for_view) {
            const int64_t row_of_group = m_rail_model->getRow(ancestor);
            ModelIndex group_index     = m_rail_model->getParent(ancestor);

            for (int64_t i = 0; i < row_of_group; ++i) {
                ModelIndex sibling_index = m_rail_model->getIndex(i, 0, group_index);
                if (!m_rail_model->validateIndex(sibling_index)) {
                    return desired_scroll;
                }
                recursive_calc(sibling_index);
            }
        }

        return desired_scroll;
    }

    void SceneWindow::regeneratePropertiesForObject(RefPtr<ISceneObject> object) {
        for (auto &member : object->getMembers()) {
            member->syncArray();
            auto prop = createProperty(member);
            if (prop) {
                prop->onValueChanged(
                    [object](RefPtr<MetaMember> member) { object->refreshRenderState(); });
                m_selected_properties.push_back(std::move(prop));
            }
        }
    }

    void SceneWindow::_moveNode(const Rail::RailNode &node, size_t index, UUID64 rail_id,
                                size_t orig_index, UUID64 orig_id, bool is_internal) {
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
                const bool is_verifying = m_scene_verifier.tIsAlive();
                const bool is_repairing = m_scene_mender.tIsAlive();

                if (is_verifying || is_repairing) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::MenuItem("Verify Scene")) {
                    m_scene_verifier = ToolboxSceneVerifier(
                        m_scene_object_model, m_table_object_model, m_rail_model, false);
                    m_scene_verifier.tStart(true, nullptr);
                    m_scene_validator_result_opened = false;
                }

                if (ImGui::MenuItem("Verify Scene & Dependencies")) {
                    m_scene_verifier = ToolboxSceneVerifier(
                        m_scene_object_model, m_table_object_model, m_rail_model, true);
                    m_scene_verifier.tStart(true, nullptr);
                    m_scene_validator_result_opened = false;
                }

                if (ImGui::MenuItem("Repair Dependencies")) {
                    m_scene_mender = ToolboxSceneDependencyMender(
                        m_scene_object_model, m_table_object_model, m_rail_model);
                    m_scene_mender.tStart(true, nullptr);
                    m_scene_mender_result_opened = false;
                }

                if (ImGui::MenuItem("Prune Scene")) {
                    m_scene_pruner = ToolboxScenePruner(
                        m_scene_object_model, m_table_object_model, m_rail_model);
                    m_scene_pruner.tStart(true, nullptr);
                    m_scene_pruner_result_opened = false;
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
    if (!m_object_model || !m_table_model || !m_rail_model) {
        m_successful = false;
        return;
    }

    m_successful = ValidateScene(
        m_object_model, m_table_model, m_rail_model, m_check_dependencies,
        [this](double progress, const std::string &progress_text) {
            setProgress(progress);
            m_progress_text = progress_text;
        },
        [this](const std::string &error_msg) { m_errors.push_back(error_msg); });
}

void ToolboxSceneDependencyMender::tRun(void *param) {
    if (!m_object_model || !m_table_model || !m_rail_model) {
        m_successful = false;
        return;
    }

    m_successful = RepairScene(
        m_object_model, m_table_model, m_rail_model, true,
        [this](const std::string &progress_text) { m_progress_text = progress_text; },
        [this](const std::string &change_msg) { m_changes.push_back(change_msg); },
        [this](const std::string &error_msg) { m_errors.push_back(error_msg); });
}

void ToolboxScenePruner::tRun(void *param) {
    if (!m_object_model || !m_table_model || !m_rail_model) {
        m_successful = false;
        return;
    }

    m_successful = PruneScene(
        m_object_model, m_table_model, m_rail_model, m_operation_mutex, true,
        [this](const std::string &progress_text) { m_progress_text = progress_text; },
        [this](const std::string &change_msg) { m_changes.push_back(change_msg); },
        [this](const std::string &error_msg) { m_errors.push_back(error_msg); });
}

bool ToolboxSceneVerifier::ValidateScene(RefPtr<SceneObjModel> object_model,
                                         RefPtr<SceneObjModel> table_model,
                                         RefPtr<RailObjModel> rail_model, bool check_dependencies,
                                         validate_progress_cb progress_cb,
                                         validate_error_cb error_cb) {
    // The goal here is to test that the scene is loadable and all objects are valid.
    SceneValidator validate(object_model, table_model, rail_model, check_dependencies);
    validate.setProgressCallback(progress_cb);
    validate.setErrorCallback(error_cb);

    // clang-format off

        validate.scopeAssertObject("GroupObj", to_str(u8"全体シーン"))
                .scopeEnter()
                    .scopeAssertSize(VALIDATOR_GE(6))
                    .scopeAssertObject("GroupObj", to_str(u8"コンダクター初期化用"))
                    .scopeEnter()
                        .scopeAssertSize(VALIDATOR_GE(7))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"シャインマネージャー"))
                        .scopeAssertObject("MapObjManager", to_str(u8"地形オブジェマネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"木マネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"大型地形オブジェマネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"ファークリップ地形オブジェマネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"乗り物マネージャー"))
                        .scopeAssertObject("ItemManager", to_str(u8"アイテムマネージャー"))
                    .scopeEscape()
                    .scopeAssertObject("GroupObj", to_str(u8"鏡シーン"))
                    .scopeEnter()
                        .scopeAssertSize(VALIDATOR_GE(1))
                        .scopeAssertObject("MirrorCamera", to_str(u8"鏡カメラ"))
                    .scopeEscape()
                    .scopeAssertObject("MirrorModelManager", to_str(u8"鏡表示モデル管理"))
                    .scopeAssertObject("GroupObj", to_str(u8"スペキュラシーン"))
                    .scopeAssertObject("GroupObj", to_str(u8"インダイレクトシーン"))
                    .scopeAssertObject("MarScene", to_str(u8"通常シーン"))
                    .scopeEnter()
                        .scopeAssertSize(VALIDATOR_GE(4))
                        .scopeAssertObject("AmbAry", to_str(u8"Ambient Group"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(6))
                            .scopeAssertObject("AmbColor", to_str(u8"太陽アンビエント（プレイヤー）"))
                            .scopeAssertObject("AmbColor", to_str(u8"影アンビエント（プレイヤー）"))
                            .scopeAssertObject("AmbColor", to_str(u8"太陽アンビエント（オブジェクト）"))
                            .scopeAssertObject("AmbColor", to_str(u8"影アンビエント（オブジェクト）"))
                            .scopeAssertObject("AmbColor", to_str(u8"太陽アンビエント（敵）"))
                            .scopeAssertObject("AmbColor", to_str(u8"影アンビエント（敵）"))
                        .scopeEscape()
                        .scopeAssertObject("LightAry", to_str(u8"Light Group"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(15))
                            .scopeAssertObject("Light", to_str(u8"太陽（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"太陽サブ（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"影（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"影サブ（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"太陽スペキュラ（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"太陽（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"太陽サブ（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"影（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"影サブ（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"太陽スペキュラ（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"太陽（敵）"))
                            .scopeAssertObject("Light", to_str(u8"太陽サブ（敵）"))
                            .scopeAssertObject("Light", to_str(u8"影（敵）"))
                            .scopeAssertObject("Light", to_str(u8"影サブ（敵）"))
                            .scopeAssertObject("Light", to_str(u8"太陽スペキュラ（敵）"))
                        .scopeEscape()
                        .scopeAssertObject("Strategy", to_str(u8"ストラテジ"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(12))
                            .scopeAssertObject("IdxGroup", to_str(u8"マップグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Map", to_str(u8"マップ"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"空グループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Sky", to_str(u8"空"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"マネージャーグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(12))
                                .scopeAssertObject("SunMgr", to_str(u8"太陽マネージャー"))
                                .scopeAssertObject("CubeCamera", to_str(u8"キューブ（カメラ）"))
                                .scopeAssertObject("CubeMirror", to_str(u8"キューブ（鏡）"))
                                .scopeAssertObject("CubeWire", to_str(u8"キューブ（ワイヤー）"))
                                .scopeAssertObject("CubeStream", to_str(u8"キューブ（流れ）"))
                                .scopeAssertObject("CubeShadow", to_str(u8"キューブ（影）"))
                                .scopeAssertObject("CubeArea", to_str(u8"キューブ（エリア）"))
                                .scopeAssertObject("CubeFastA", to_str(u8"キューブ（高速Ａ）"))
                                .scopeAssertObject("CubeFastB", to_str(u8"キューブ（高速Ｂ）"))
                                .scopeAssertObject("CubeFastC", to_str(u8"キューブ（高速Ｃ）"))
                                .scopeAssertObject("CubeSoundChange", to_str(u8"キューブ（サウンド切り替え）"))
                                .scopeAssertObject("CubeSoundEffect", to_str(u8"キューブ（サウンドエフェクト）"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"オブジェクトグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"落書きグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Pollution", to_str(u8"落書き管理"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"アイテムグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"プレーヤーグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Mario", to_str(u8"マリオ"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"敵グループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"ボスグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"ＮＰＣグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"水パーティクルグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"初期化用グループ"))
                        .scopeEscape()
                        .scopeAssertObject("GroupObj", to_str(u8"Cameras"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(1))
                            .scopeAssertObject("PolarSubCamera", to_str(u8"camera 1"))
                        .scopeEscape()
                    .scopeEscape()
                .scopeEscape();

    // clang-format on

    // Now we check for dependencies on ALL objects
    validate.scopeForAll(object_model, [&validate](RefPtr<SceneObjModel> model, ModelIndex index) {
        validate.scopeValidateDependencies(model, index);
    });

    return static_cast<bool>(validate);
}

bool ToolboxSceneDependencyMender::RepairScene(
    RefPtr<SceneObjModel> object_model, RefPtr<SceneObjModel> table_model,
    RefPtr<RailObjModel> rail_model, bool check_dependencies, repair_progress_cb progress_cb,
    repair_change_cb change_cb, repair_error_cb error_cb) {
    // The goal here is to test that the scene is loadable and all objects are valid.
    SceneMender mender(object_model, table_model, rail_model, check_dependencies);
    mender.setProgressCallback(progress_cb);
    mender.setChangeCallback(change_cb);
    mender.setErrorCallback(error_cb);

    // clang-format off

    mender.scopeTryCreateObjectDefault("GroupObj", to_str(u8"全体シーン"))
            .scopeEnter()
                .scopeTryCreateObjectDefault("GroupObj", to_str(u8"コンダクター初期化用"))
                .scopeEnter()
                    .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"シャインマネージャー"))
                    .scopeTryCreateObjectDefault("MapObjManager", to_str(u8"地形オブジェマネージャー"))
                    .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"木マネージャー"))
                    .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"大型地形オブジェマネージャー"))
                    .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"ファークリップ地形オブジェマネージャー"))
                    .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"乗り物マネージャー"))
                    .scopeTryCreateObjectDefault("ItemManager", to_str(u8"アイテムマネージャー"))
                    .scopeTryCreateObjectDefault("PoolManager", to_str(u8"水場マネージャー"))
                .scopeEscape()
                .scopeTryCreateObjectDefault("GroupObj", to_str(u8"鏡シーン"))
                .scopeEnter()
                    .scopeTryCreateObjectDefault("MirrorCamera", to_str(u8"鏡カメラ"))
                .scopeEscape()
                .scopeTryCreateObjectDefault("MirrorModelManager", to_str(u8"鏡表示モデル管理"))
                .scopeTryCreateObjectDefault("GroupObj", to_str(u8"スペキュラシーン"))
                .scopeTryCreateObjectDefault("GroupObj", to_str(u8"インダイレクトシーン"))
                .scopeTryCreateObjectDefault("MarScene", to_str(u8"通常シーン"))
                .scopeEnter()
                    .scopeTryCreateObjectDefault("AmbAry", to_str(u8"Ambient Group"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("AmbColor", to_str(u8"太陽アンビエント（プレイヤー）"))
                        .scopeTryCreateObjectDefault("AmbColor", to_str(u8"影アンビエント（プレイヤー）"))
                        .scopeTryCreateObjectDefault("AmbColor", to_str(u8"太陽アンビエント（オブジェクト）"))
                        .scopeTryCreateObjectDefault("AmbColor", to_str(u8"影アンビエント（オブジェクト）"))
                        .scopeTryCreateObjectDefault("AmbColor", to_str(u8"太陽アンビエント（敵）"))
                        .scopeTryCreateObjectDefault("AmbColor", to_str(u8"影アンビエント（敵）"))
                    .scopeEscape()
                    .scopeTryCreateObjectDefault("LightAry", to_str(u8"Light Group"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽（プレイヤー）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽サブ（プレイヤー）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"影（プレイヤー）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"影サブ（プレイヤー）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽スペキュラ（プレイヤー）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽（オブジェクト）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽サブ（オブジェクト）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"影（オブジェクト）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"影サブ（オブジェクト）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽スペキュラ（オブジェクト）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽（敵）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽サブ（敵）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"影（敵）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"影サブ（敵）"))
                        .scopeTryCreateObjectDefault("Light", to_str(u8"太陽スペキュラ（敵）"))
                    .scopeEscape()
                    .scopeTryCreateObjectDefault("Strategy", to_str(u8"ストラテジ"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"マップグループ"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("Map", to_str(u8"マップ"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"空グループ"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("Sky", to_str(u8"空"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"マネージャーグループ"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("SunMgr", to_str(u8"太陽マネージャー"))
                            .scopeTryCreateObjectDefault("CubeCamera", to_str(u8"キューブ（カメラ）"))
                            .scopeTryCreateObjectDefault("CubeMirror", to_str(u8"キューブ（鏡）"))
                            .scopeTryCreateObjectDefault("CubeWire", to_str(u8"キューブ（ワイヤー）"))
                            .scopeTryCreateObjectDefault("CubeStream", to_str(u8"キューブ（流れ）"))
                            .scopeTryCreateObjectDefault("CubeShadow", to_str(u8"キューブ（影）"))
                            .scopeTryCreateObjectDefault("CubeArea", to_str(u8"キューブ（エリア）"))
                            .scopeTryCreateObjectDefault("CubeFastA", to_str(u8"キューブ（高速Ａ）"))
                            .scopeTryCreateObjectDefault("CubeFastB", to_str(u8"キューブ（高速Ｂ）"))
                            .scopeTryCreateObjectDefault("CubeFastC", to_str(u8"キューブ（高速Ｃ）"))
                            .scopeTryCreateObjectDefault("CubeSoundChange", to_str(u8"キューブ（サウンド切り替え）"))
                            .scopeTryCreateObjectDefault("CubeSoundEffect", to_str(u8"キューブ（サウンドエフェクト）"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"オブジェクトグループ"))
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"落書きグループ"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("Pollution", to_str(u8"落書き管理"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"アイテムグループ"))
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"プレーヤーグループ"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("Mario", to_str(u8"マリオ"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"敵グループ"))
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"ボスグループ"))
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"ＮＰＣグループ"))
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"水パーティクルグループ"))
                        .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"初期化用グループ"))
                    .scopeEscape()
                    .scopeTryCreateObjectDefault("GroupObj", to_str(u8"Cameras"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("PolarSubCamera", to_str(u8"camera 1"))
                    .scopeEscape()
                .scopeEscape()
            .scopeEscape();

    // Now we check for dependencies on ALL objects

    // We perform up to 3 resolutions (adding one dependency might need a chained dependency)
    for (size_t i = 0; i < 3; ++i) {
        mender.scopeForAll(object_model, [&mender](RefPtr<SceneObjModel> model, ModelIndex index) {
            mender.scopeFulfillManagerDependencies(model, index);
        });

        mender.scopeForAll(object_model, [&mender](RefPtr<SceneObjModel> model, ModelIndex index) {
            mender.scopeFulfillTableBinDependencies(model, index);
        });
    }

    mender.scopeForAll(object_model, [&mender](RefPtr<SceneObjModel> model, ModelIndex index) {
        mender.scopeFulfillRailDependencies(model, index);
    });

    mender.scopeForAll(object_model, [&mender](RefPtr<SceneObjModel> model, ModelIndex index) {
        mender.scopeFulfillRailDependencies(model, index);
    });

    mender.scopeForAll(object_model, [&mender](RefPtr<SceneObjModel> model, ModelIndex index) {
        mender.scopeFulfillAssetDependencies(model, index);
    });

    return static_cast<bool>(mender);
}

bool ToolboxScenePruner::PruneScene(RefPtr<SceneObjModel> object_model,
                                    RefPtr<SceneObjModel> table_model,
                                    RefPtr<RailObjModel> rail_model, std::mutex &operation_mutex, bool check_dependencies,
                                    prune_progress_cb progress_cb, prune_change_cb change_cb,
                                    prune_error_cb error_cb) {
    ScenePruner pruner(object_model, table_model, rail_model, check_dependencies);
    pruner.setProgressCallback(progress_cb);
    pruner.setChangeCallback(change_cb);
    pruner.setErrorCallback(error_cb);

    pruner.scopeForAll(object_model, [&pruner](RefPtr<SceneObjModel> model, ModelIndex index) {
        pruner.scopeCollectDependencies(model, index);
    });

    TemplateDependencies::ObjectInfo a;

    const QualifiedName root_name = QualifiedName(to_str(u8"全体シーン"));
    const QualifiedName manager_group_name = QualifiedName(to_str(u8"コンダクター初期化用"), root_name);
    
    pruner.addManagerDependency(TemplateDependencies::ObjectInfo {"MapObjBaseManager", to_str(u8"シャインマネージャー"),                 manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"MapObjManager",     to_str(u8"地形オブジェマネージャー"),              manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"MapObjBaseManager", to_str(u8"木マネージャー"),                       manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"MapObjBaseManager", to_str(u8"大型地形オブジェマネージャー"),          manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"MapObjBaseManager", to_str(u8"ファークリップ地形オブジェマネージャー"), manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"MapObjBaseManager", to_str(u8"乗り物マネージャー"),                   manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"ItemManager",       to_str(u8"アイテムマネージャー"),                 manager_group_name})
          .addManagerDependency(TemplateDependencies::ObjectInfo {"PoolManager",       to_str(u8"水場マネージャー"),                    manager_group_name});

    pruner.scopeAssertObject("GroupObj", to_str(u8"全体シーン"))
            .scopeEnter()
                .scopeAssertObject("GroupObj", to_str(u8"コンダクター初期化用"))
                .scopeEnter()
                    .scopeForAll(object_model, [&pruner](RefPtr<SceneObjModel> model, ModelIndex index) {
                        pruner.scopePruneManagerIfUnused(model, index);
                    })
                .scopeEscape()
            .scopeEscape();

    pruner.scopeForAll(rail_model, [&pruner](RefPtr<RailObjModel> model, ModelIndex index) {
                        pruner.scopePruneRailIfUnused(model, index);
                    });

    {
        std::unique_lock<std::mutex> operation_lock(operation_mutex);
        pruner.finalize();
    }

    return static_cast<bool>(pruner);
}

// clang-format on

#undef VALIDATOR_LT
#undef VALIDATOR_LE
#undef VALIDATOR_GT
#undef VALIDATOR_GE
#undef VALIDATOR_NE
#undef VALIDATOR_EQ

SceneValidator &SceneValidator::scopeAssertObject(const std::string &obj_type,
                                                  const std::string &obj_name) {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (m_parent_stack.empty()) {
        ModelIndex root_index = m_object_model->getIndex(0, 0);
        if (!m_object_model->validateIndex(root_index)) {
            m_valid = false;
            m_error_callback("Failed to find root object");
            return *this;
        }

        std::string root_type = m_object_model->getObjectType(root_index);
        std::string root_key  = m_object_model->getObjectKey(root_index);

        if (root_type != obj_type || root_key != obj_name) {
            m_valid = false;
            m_error_callback(std::format("Failed to find root object of type '{}' with name '{}'",
                                         obj_type, obj_name));
            return *this;
        }
        m_last_index = root_index;
        m_processed_objects += 1;
        if (m_progress_callback) {
            std::string progress_text = std::format("{} ({}) [{}/{}]", root_type, root_key,
                                                    static_cast<int>(m_processed_objects),
                                                    static_cast<int>(m_total_objects));
            m_progress_callback(m_processed_objects / m_total_objects, progress_text);
        }
        return *this;
    }

    ModelIndex group_index = m_parent_stack.top();
    if (!m_object_model->validateIndex(group_index)) {
        m_valid = false;
        m_error_callback("Somehow the parent stack was corrupted while validating "
                         "(concurrency?) Report this error to JoshuaMK");
        return *this;
    }

    ModelIndex child_index = m_object_model->getIndex(obj_name, group_index);
    if (!m_object_model->validateIndex(child_index)) {
        m_valid = false;
        m_error_callback(
            std::format("Failed to find child object of type '{}' with name '{}' in parent '{}'",
                        obj_type, obj_name, m_object_model->getObjectKey(group_index)));
        return *this;
    }

    std::string child_type = m_object_model->getObjectType(child_index);
    if (child_type != obj_type) {
        m_valid = false;
        m_error_callback(
            std::format("Failed to find child object of type '{}' with name '{}' in parent '{}'",
                        obj_type, obj_name, m_object_model->getObjectKey(group_index)));
        return *this;
    }

    m_last_index = child_index;
    m_processed_objects += 1;
    if (m_progress_callback) {
        std::string progress_text =
            std::format("{} ({}) [{}/{}]", obj_type, obj_name,
                        static_cast<int>(m_processed_objects), static_cast<int>(m_total_objects));
        m_progress_callback(m_processed_objects / m_total_objects, progress_text);
    }
    return *this;
}

SceneValidator &SceneValidator::scopeAssertSize(size_fn predicate) {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (m_parent_stack.empty()) {
        m_valid = false;
        return *this;
    }

    ModelIndex group_index = m_parent_stack.top();
    if (!m_object_model->validateIndex(group_index)) {
        m_valid = false;
        return *this;
    }

    m_valid = predicate(m_object_model->getRowCount(group_index));
    return *this;
}

SceneValidator &SceneValidator::scopeEnter() {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (!m_object_model->validateIndex(m_last_index)) {
        m_valid = false;
        m_error_callback("Tried to enter the scope of a leaf object");
        return *this;
    }

    m_parent_stack.push(m_last_index);
    return *this;
}

SceneValidator &SceneValidator::scopeEscape() {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (m_parent_stack.empty()) {
        m_valid = false;
        m_error_callback("Unbalanced scope when validating scene");
        return *this;
    }

    m_parent_stack.pop();
    return *this;
}

SceneValidator &SceneValidator::scopeValidateDependencies(RefPtr<SceneObjModel> model,
                                                          const ModelIndex &index) {
    if (!m_check_dependencies || !model->validateIndex(index)) {
        return *this;
    }

    RefPtr<ISceneObject> object = model->getObjectRef(index);
    std::string obj_type        = model->getObjectType(index);
    std::string obj_key         = model->getObjectKey(index);

    auto template_ = TemplateFactory::create(obj_type, true);
    if (!template_) {
        m_valid = false;
        m_error_callback(
            std::format("Object '{} ({})': Failed to load template!", obj_type, obj_key));
        return *this;
    }

    std::optional<TemplateWizard> wizard = template_.value()->getWizard(object->getWizardName());
    if (!wizard) {
        wizard = template_.value()->getWizard("Default");
        if (!wizard) {
            m_valid = false;
            m_error_callback(std::format("Object '{} ({})': Failed to load the wizard '{}'!",
                                         obj_type, obj_key, object->getWizardName()));
            return *this;
        }
    }

    const TemplateDependencies &dependencies = wizard->m_dependencies;
    for (const TemplateDependencies::ObjectInfo &manager : dependencies.m_managers) {
        ModelIndex group_index = m_object_model->getIndex(manager.m_ancestry);
        if (!m_object_model->validateIndex(group_index)) {
            // group_obj = m_scene_instance->getObjHierarchy()->findObject(manager.m_ancestry);
            m_valid = false;
            m_error_callback(std::format("Object '{} ({})': Failed to find required ancestor '{}' "
                                         "of manager dependency '{} ({})'!",
                                         obj_type, obj_key, manager.m_ancestry.toString(),
                                         manager.m_type, manager.m_name));
            continue;
        }
        ModelIndex manager_index =
            m_object_model->getIndex(manager.m_type, manager.m_name, group_index);
        if (!m_object_model->validateIndex(manager_index)) {
            m_valid = false;
            m_error_callback(
                std::format("Object '{} ({})': Failed to find required manager object '{} ({})'!",
                            obj_type, obj_key, manager.m_type, manager.m_name));
        }
    }

    for (const std::string &asset_path : dependencies.m_asset_paths) {
        // Assets for an object are found in the subdirectory at SceneAssets/{asset_path}/...
        const fs_path abs_asset_path =
            (Filesystem::current_path().value_or(".") / "SceneAssets" / asset_path)
                .lexically_normal();

        if (!Filesystem::is_directory(abs_asset_path).value_or(false)) {
            m_valid = false;
            m_error_callback(
                std::format("Object '{} ({})': Invalid asset path '{}' (does not exist)!", obj_type,
                            obj_key, asset_path));
            continue;
        }

        const fs_path &scene_root_path = m_object_model->getScenePath();

        for (const Filesystem::directory_entry dir_entry :
             Filesystem::recursive_directory_iterator(abs_asset_path)) {
            if (dir_entry.is_directory()) {
                continue;
            }

            const fs_path relative_path =
                Filesystem::relative(dir_entry.path(), abs_asset_path).value_or(dir_entry.path());
            const fs_path scene_path = scene_root_path / relative_path;

            if (!Filesystem::exists(scene_path).value_or(false)) {
                m_valid = false;
                m_error_callback(
                    std::format("Object '{} ({})': Failed to find required asset '{}'!", obj_type,
                                obj_key, relative_path.string()));
            }
        }
    }

    // Check for rail dependency
    {

        RefPtr<ISceneObject> object = model->getObjectRef(index);
        auto rail_member_result     = object->getMember("Rail");
        if (rail_member_result.value_or(nullptr)) {
            bool needs_rail = true;

            RefPtr<MetaMember> rail_member = rail_member_result.value();
            std::string rail_dependency    = getMetaValue<std::string>(rail_member).value();
            if (!rail_dependency.empty() && rail_dependency != "(null)") {
                const size_t rail_count = m_rail_model->getRowCount(ModelIndex());
                for (size_t i = 0; i < rail_count; ++i) {
                    ModelIndex rail_index = m_rail_model->getIndex(i, 0);
                    if (!m_rail_model->validateIndex(rail_index)) {
                        break;
                    }

                    std::string rail_name = m_rail_model->getRailKey(rail_index);
                    if (rail_dependency == rail_name) {
                        needs_rail = false;
                        break;
                    }
                }
            } else {
                needs_rail = false;
            }

            if (needs_rail) {
                m_valid = false;
                m_error_callback(std::format("Object '{} ({})': Failed to find required rail '{}'!",
                                             obj_type, obj_key, rail_dependency));
            }
        }
    }

    for (const TemplateDependencies::ObjectInfo &obj : dependencies.m_table_objs) {
        ModelIndex group_index = m_table_model->getIndex(obj.m_ancestry);
        if (!m_table_model->validateIndex(group_index)) {
            // group_obj = m_scene_instance->getObjHierarchy()->findObject(manager.m_ancestry);
            m_valid = false;
            m_error_callback(std::format("Object '{} ({})': Failed to find required ancestor '{}' "
                                         "of tables.bin dependency '{} ({})'!",
                                         obj_type, obj_key, obj.m_ancestry.toString(), obj.m_type,
                                         obj.m_name));
            continue;
        }
        ModelIndex table_index = m_table_model->getIndex(obj.m_type, obj.m_name, group_index);
        if (!m_table_model->validateIndex(table_index)) {
            m_valid = false;
            m_error_callback(std::format(
                "Object '{} ({})': Failed to find required tables.bin object '{} ({})'!", obj_type,
                obj_key, obj.m_type, obj.m_name));
        }
    }

    return *this;
}

static void forEach(RefPtr<SceneObjModel> model, ModelIndex parent,
                    SceneValidator::foreach_obj_fn fn) {
    const int64_t row_count = model->getRowCount(parent);
    for (int64_t row = 0; row < row_count; ++row) {
        ModelIndex child = model->getIndex(row, 0, parent);
        if (!model->validateIndex(child)) {
            return;
        }
        fn(model, child);
        if (model->validateIndex(child)) {
            forEach(model, child, fn);
        }
    }
}

SceneValidator &SceneValidator::scopeForAll(RefPtr<SceneObjModel> model, foreach_obj_fn fn) {
    forEach(model, m_parent_stack.empty() ? ModelIndex() : m_parent_stack.top(), fn);
    return *this;
}

SceneMender &SceneMender::scopeTryCreateObjectDefault(const std::string &obj_type,
                                                      const std::string &obj_name) {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    ModelIndex group_index;
    if (!m_parent_stack.empty()) {
        group_index = m_parent_stack.top();

        if (!m_object_model->validateIndex(group_index)) {
            m_valid = false;
            m_error_callback("Somehow the parent stack was corrupted while validating "
                             "(concurrency?) Report this error to JoshuaMK");
            return *this;
        }
    }

    bool needs_creation = true;

    ModelIndex child_index = m_object_model->getIndex(obj_name, group_index);
    if (m_object_model->validateIndex(child_index)) {
        std::string child_type = m_object_model->getObjectType(child_index);
        if (child_type == obj_type) {
            needs_creation = false;
        }
    }

    if (needs_creation) {
        auto template_ = TemplateFactory::create(obj_type, true);
        if (!template_) {
            m_valid = false;
            m_error_callback(
                std::format("Failed to load template for object type '{}'!", obj_type));
            return *this;
        }

        RefPtr<ISceneObject> new_obj;

        std::optional<TemplateWizard> specialized_wizard =
            template_.value()->getWizardByObjName(obj_name);
        if (specialized_wizard) {
            new_obj = ObjectFactory::create(*template_.value(), specialized_wizard.value().m_name,
                                            m_object_model->getScenePath());
        } else {
            new_obj = ObjectFactory::create(*template_.value(), "Default",
                                            m_object_model->getScenePath());
        }

        if (!new_obj) {
            m_valid = false;
            m_error_callback(
                std::format("Failed to create default object of type '{}' with name '{}'!",
                            obj_type, obj_name));
            return *this;
        }

        new_obj->setNameRef(NameRef(obj_name));

        std::string group_type = m_object_model->getObjectType(group_index);
        std::string group_key  = m_object_model->getObjectKey(group_index);

        const int64_t group_size = m_object_model->getRowCount(group_index);
        child_index              = m_object_model->insertObject(new_obj, group_size, group_index);
        if (!m_object_model->validateIndex(child_index)) {
            m_valid = false;
            m_error_callback(std::format("Failed to insert object [{} ({})] into [{} ({})]",
                                         obj_type, obj_name, group_type, group_key));
            return *this;
        }

        std::string change_text = std::format("Added object [{} ({})] to [{} ({})]", obj_type,
                                              obj_name, group_type, group_key);
        m_change_callback(change_text);
    }

    m_last_index = child_index;
    m_processed_objects += 1;
    if (m_progress_callback) {
        std::string progress_text =
            std::format("{} ({}) [{}]", obj_type, obj_name, m_processed_objects);
        m_progress_callback(progress_text);
    }
    return *this;
}

SceneMender &SceneMender::scopeEnter() {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (!m_object_model->validateIndex(m_last_index)) {
        m_valid = false;
        m_error_callback("Tried to enter the scope of a leaf object");
        return *this;
    }

    m_parent_stack.push(m_last_index);
    return *this;
}

SceneMender &SceneMender::scopeEscape() {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (m_parent_stack.empty()) {
        m_valid = false;
        m_error_callback("Unbalanced scope when validating scene");
        return *this;
    }

    m_parent_stack.pop();
    return *this;
}

static void forEachM(RefPtr<SceneObjModel> model, ModelIndex parent,
                     SceneMender::foreach_obj_fn fn) {
    const int64_t row_count = model->getRowCount(parent);
    for (int64_t row = 0; row < row_count; ++row) {
        ModelIndex child = model->getIndex(row, 0, parent);
        if (!model->validateIndex(child)) {
            return;
        }
        fn(model, child);
        if (model->validateIndex(child)) {
            forEachM(model, child, fn);
        }
    }
}

SceneMender &SceneMender::scopeFulfillManagerDependencies(RefPtr<SceneObjModel> model,
                                                          const ModelIndex &index) {
    if (!m_check_dependencies) {
        return *this;
    }

    RefPtr<ISceneObject> object = model->getObjectRef(index);
    const std::string &obj_type = object->type();

    auto template_ = TemplateFactory::create(obj_type, true);
    if (!template_) {
        m_valid = false;
        m_error_callback(std::format("Failed to load template for object type '{}'!", obj_type));
        return *this;
    }

    std::optional<TemplateWizard> wizard = template_.value()->getWizard(object->getWizardName());
    if (!wizard) {
        wizard = template_.value()->getWizard("Default");
        if (!wizard) {
            m_valid = false;
            m_error_callback(std::format("Failed to load the wizard '{}' for object type '{}'!",
                                         object->getWizardName(), obj_type));
            return *this;
        }
    }

    const TemplateDependencies &dependencies = wizard->m_dependencies;
    for (const TemplateDependencies::ObjectInfo &manager : dependencies.m_managers) {
        ModelIndex group_index = m_object_model->getIndex(manager.m_ancestry);
        if (!m_object_model->validateIndex(group_index)) {
            m_valid = false;
            m_error_callback(std::format("Provided manager ancestry '{}' is not a group object!",
                                         manager.m_ancestry.toString()));
            continue;
        }

        ModelIndex manager_index =
            m_object_model->getIndex(manager.m_type, manager.m_name, group_index);

        if (!m_object_model->validateIndex(manager_index)) {
            auto manager_template_ = TemplateFactory::create(manager.m_type, true);
            if (!manager_template_) {
                m_valid = false;
                m_error_callback(
                    std::format("Failed to load template for object type '{}'!", obj_type));
                return *this;
            }

            RefPtr<ISceneObject> manager_obj = ObjectFactory::create(
                *manager_template_.value(), "Default", m_object_model->getScenePath());
            if (!manager_obj) {
                m_valid = false;
                m_error_callback(std::format("Failed to create manager object {} ({})",
                                             manager.m_type, manager.m_name));
                continue;
            }

            manager_obj->setNameRef(NameRef(manager.m_name));

            const int64_t group_size = m_object_model->getRowCount(group_index);
            manager_index = m_object_model->insertObject(manager_obj, group_size, group_index);
            if (!m_object_model->validateIndex(manager_index)) {
                m_valid = false;
                m_error_callback(std::format("Failed to add manager '{}' to ancestry group '{}'",
                                             manager.m_name, manager.m_ancestry.toString()));
                continue;
            }

            std::string group_type = m_object_model->getObjectType(group_index);
            std::string group_key  = m_object_model->getObjectKey(group_index);

            std::string change_text =
                std::format("Added manager [{} ({})] to [{} ({})]", manager.m_type, manager.m_name,
                            group_type, group_key);
            m_change_callback(change_text);
        }
    }

    return *this;
}

SceneMender &SceneMender::scopeFulfillTableBinDependencies(RefPtr<SceneObjModel> model,
                                                           const ModelIndex &index) {
    if (!m_check_dependencies) {
        return *this;
    }

    RefPtr<ISceneObject> object = model->getObjectRef(index);
    const std::string &obj_type = object->type();

    auto template_ = TemplateFactory::create(obj_type, true);
    if (!template_) {
        m_valid = false;
        m_error_callback(std::format("Failed to load template for object type '{}'!", obj_type));
        return *this;
    }

    std::optional<TemplateWizard> wizard = template_.value()->getWizard(object->getWizardName());
    if (!wizard) {
        wizard = template_.value()->getWizard("Default");
        if (!wizard) {
            m_valid = false;
            m_error_callback(std::format("Failed to load the wizard '{}' for object type '{}'!",
                                         object->getWizardName(), obj_type));
            return *this;
        }
    }

    const TemplateDependencies &dependencies = wizard->m_dependencies;

    for (const TemplateDependencies::ObjectInfo &obj : dependencies.m_table_objs) {
        ModelIndex group_index = m_table_model->getIndex(obj.m_ancestry);
        if (!m_table_model->validateIndex(group_index)) {
            m_valid = false;
            m_error_callback(std::format("Provided tables.bin ancestry '{}' is not a group object!",
                                         obj.m_ancestry.toString()));
            continue;
        }

        ModelIndex table_obj_index = m_table_model->getIndex(obj.m_type, obj.m_name, group_index);

        if (!m_table_model->validateIndex(table_obj_index)) {
            auto table_obj_template_ = TemplateFactory::create(obj.m_type, true);
            if (!table_obj_template_) {
                m_valid = false;
                m_error_callback(
                    std::format("Failed to load template for object type '{}'!", obj_type));
                return *this;
            }

            RefPtr<ISceneObject> table_obj = ObjectFactory::create(
                *table_obj_template_.value(), "Default", m_table_model->getScenePath());
            if (!table_obj) {
                m_valid = false;
                m_error_callback(std::format("Failed to create tables.bin object {} ({})",
                                             obj.m_type, obj.m_name));
                continue;
            }

            table_obj->setNameRef(NameRef(obj.m_name));

            const int64_t group_size = m_table_model->getRowCount(group_index);
            table_obj_index = m_table_model->insertObject(table_obj, group_size, group_index);
            if (!m_table_model->validateIndex(table_obj_index)) {
                m_valid = false;
                m_error_callback(std::format("Failed to add tables.bin '{}' to ancestry group '{}'",
                                             obj.m_name, obj.m_ancestry.toString()));
                continue;
            }

            std::string group_type = m_table_model->getObjectType(group_index);
            std::string group_key  = m_table_model->getObjectKey(group_index);

            std::string change_text = std::format("Added tables.bin [{} ({})] to [{} ({})]",
                                                  obj.m_type, obj.m_name, group_type, group_key);
            m_change_callback(change_text);
        }
    }

    return *this;
}

SceneMender &SceneMender::scopeFulfillRailDependencies(RefPtr<SceneObjModel> model,
                                                       const ModelIndex &index) {
    if (!m_check_dependencies) {
        return *this;
    }

    RefPtr<ISceneObject> object = model->getObjectRef(index);
    auto rail_member_result     = object->getMember("Rail");
    if (!rail_member_result.value_or(nullptr)) {
        return *this;
    }

    RefPtr<MetaMember> rail_member = rail_member_result.value();
    std::string rail_dependency    = getMetaValue<std::string>(rail_member).value();
    if (rail_dependency.empty() || rail_dependency == "(null)") {
        return *this;
    }

    const size_t rail_count = m_rail_model->getRowCount(ModelIndex());
    for (size_t i = 0; i < rail_count; ++i) {
        ModelIndex rail_index = m_rail_model->getIndex(i, 0);
        if (!m_rail_model->validateIndex(rail_index)) {
            break;
        }

        std::string rail_name = m_rail_model->getRailKey(rail_index);
        if (rail_dependency == rail_name) {
            return *this;
        }
    }

    RefPtr<Rail::Rail> new_rail = make_referable<Rail::Rail>(rail_dependency);
    ModelIndex new_rail_index   = m_rail_model->insertRail(new_rail, rail_count);
    if (!m_rail_model->validateIndex(new_rail_index)) {
        m_valid = false;
        m_error_callback(std::format("Failed to create rail {} as dependency of object '{} ({})'!",
                                     rail_dependency, object->type(), object->getNameRef().name()));
        return *this;
    }

    std::string change_text =
        std::format("Created rail {} as dependency of object '{} ({})'!", rail_dependency,
                    object->type(), object->getNameRef().name());
    m_change_callback(change_text);
    return *this;
}

SceneMender &SceneMender::scopeFulfillAssetDependencies(RefPtr<SceneObjModel> model,
                                                        const ModelIndex &index) {
    if (!m_check_dependencies) {
        return *this;
    }

    RefPtr<ISceneObject> object = model->getObjectRef(index);
    const std::string &obj_type = object->type();

    auto template_ = TemplateFactory::create(obj_type, true);
    if (!template_) {
        m_valid = false;
        m_error_callback(std::format("Failed to load template for object type '{}'!", obj_type));
        return *this;
    }

    std::optional<TemplateWizard> wizard = template_.value()->getWizard(object->getWizardName());
    if (!wizard) {
        wizard = template_.value()->getWizard("Default");
        if (!wizard) {
            m_valid = false;
            m_error_callback(std::format("Failed to load the wizard '{}' for object type '{}'!",
                                         object->getWizardName(), obj_type));
            return *this;
        }
    }

    const TemplateDependencies &dependencies = wizard->m_dependencies;

    for (const std::string &asset_path : dependencies.m_asset_paths) {
        // Assets for an object are found in the subdirectory at SceneAssets/{asset_path}/...
        const fs_path abs_asset_path =
            (Filesystem::current_path().value_or(".") / "SceneAssets" / asset_path)
                .lexically_normal();
        for (const Filesystem::directory_entry dir_entry :
             Filesystem::recursive_directory_iterator(abs_asset_path)) {
            const fs_path relative_path =
                Filesystem::relative(dir_entry.path(), abs_asset_path).value_or(dir_entry.path());
            const fs_path scene_path = m_object_model->getScenePath() / relative_path;

            if (dir_entry.is_directory()) {
                if (!Filesystem::exists(scene_path).value_or(false)) {
                    auto res = Filesystem::create_directories(scene_path);
                    if (!res) {
                        m_valid = false;
                        m_error_callback(
                            std::format("Failed to create directories for asset path '{}'",
                                        scene_path.string()));
                    }
                }
                continue;
            }

            if (Filesystem::is_regular_file(scene_path).value_or(false)) {
                continue;
            }

            auto res = Filesystem::copy_file(dir_entry.path(), scene_path,
                                             Filesystem::copy_options::overwrite_existing);
            if (!res) {
                m_valid = false;
                m_error_callback(
                    std::format("Failed to copy file for asset path '{}'", scene_path.string()));
            }

            std::string change_text =
                std::format("Added asset path {} for object [{} ({})]", relative_path.string(),
                            object->type(), object->getNameRef().name());
            m_change_callback(change_text);
        }
    }

    return *this;
}

static void forEachP(RefPtr<SceneObjModel> model, ModelIndex parent,
                     ScenePruner::foreach_obj_fn fn) {
    const int64_t row_count = model->getRowCount(parent);
    for (int64_t row = 0; row < row_count; ++row) {
        ModelIndex child = model->getIndex(row, 0, parent);
        if (!model->validateIndex(child)) {
            return;
        }
        fn(model, child);
        if (model->validateIndex(child)) {
            forEachP(model, child, fn);
        }
    }
}

static void forEachP(RefPtr<RailObjModel> model, ModelIndex parent,
                     ScenePruner::foreach_rail_fn fn) {
    const int64_t row_count = model->getRowCount(parent);
    for (int64_t row = 0; row < row_count; ++row) {
        ModelIndex child = model->getIndex(row, 0, parent);
        if (!model->validateIndex(child)) {
            return;
        }
        fn(model, child);
        if (model->validateIndex(child)) {
            forEachP(model, child, fn);
        }
    }
}

ScenePruner &ScenePruner::finalize() {
    for (const ModelIndex &index : m_indexes_to_prune) {
        if (m_object_model->validateIndex(index)) {
            const std::string manager_type = m_object_model->getObjectType(index);
            const std::string manager_name = m_object_model->getObjectKey(index);

            if (m_object_model->removeIndex(index)) {
                std::string change_text =
                    std::format("Pruned unused manager '{} ({})'", manager_type, manager_name);
                m_change_callback(change_text);
            } else {
                m_valid                 = false;
                std::string failed_text = std::format("Failed to prune unused manager '{} ({})'",
                                                      manager_type, manager_name);
                m_error_callback(failed_text);
            }
            continue;
        }

        if (m_rail_model->validateIndex(index)) {
            const std::string rail_name = m_rail_model->getRailKey(index);

            if (m_rail_model->removeIndex(index)) {
                std::string change_text = std::format("Pruned unused rail '{}'", rail_name);
                m_change_callback(change_text);
            } else {
                m_valid = false;
                std::string failed_text =
                    std::format("Failed to prune unused rail '{}'", rail_name);
                m_error_callback(failed_text);
            }
            continue;
        }
    }

    std::vector<fs_path> asset_paths_to_remove;

    const fs_path scene_root_path = m_object_model->getScenePath();
    for (const Filesystem::directory_entry dir_entry :
         Filesystem::recursive_directory_iterator(scene_root_path)) {
        const fs_path &scene_path = dir_entry.path().lexically_normal();
        if (!m_dependencies.m_asset_paths.contains(scene_path)) {
            const fs_path rel_path =
                Filesystem::relative(scene_path, scene_root_path).value_or(fs_path());

            bool is_map_path = false;
            fs_path test_path = rel_path;
            while (!test_path.empty()) {
                if (test_path.filename().string() == "map") {
                    is_map_path = true;
                    break;
                }
                test_path = test_path.parent_path();
            }

            if (is_map_path) {
                continue;
            }

            asset_paths_to_remove.push_back(rel_path);
        }
    }

    for (const fs_path& remove_path : asset_paths_to_remove) {
        if (!Filesystem::remove_all(remove_path).value_or(false)) {
            m_valid = false;
            std::string failed_text =
                std::format("Failed to prune unused asset path '{}'", remove_path.string());
            m_error_callback(failed_text);
            continue;
        }
        std::string change_text =
            std::format("Pruned unused asset path '{}'", remove_path.string());
        m_change_callback(change_text);
    }

    return *this;
}

ScenePruner &
ScenePruner::addManagerDependency(const TemplateDependencies::ObjectInfo &manager_info) {
    m_dependencies.m_managers.push_back(manager_info);
    return *this;
}

ScenePruner &ScenePruner::addRailDependency(const std::string &rail_name) {
    m_dependencies.m_rails.insert(rail_name);
    return *this;
}

ScenePruner &ScenePruner::scopeAssertObject(const std::string &obj_type,
                                            const std::string &obj_name) {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (m_parent_stack.empty()) {
        ModelIndex root_index = m_object_model->getIndex(0, 0);
        if (!m_object_model->validateIndex(root_index)) {
            m_valid = false;
            m_error_callback("Failed to find root object");
            return *this;
        }

        std::string root_type = m_object_model->getObjectType(root_index);
        std::string root_key  = m_object_model->getObjectKey(root_index);

        if (root_type != obj_type || root_key != obj_name) {
            m_valid = false;
            m_error_callback(std::format("Failed to find root object of type '{}' with name '{}'",
                                         obj_type, obj_name));
            return *this;
        }
        m_last_index = root_index;
        m_processed_objects += 1;
        if (m_progress_callback) {
            std::string progress_text = std::format("{} ({}) [{}]", root_type, root_key,
                                                    static_cast<int>(m_processed_objects));
            m_progress_callback(progress_text);
        }
        return *this;
    }

    ModelIndex group_index = m_parent_stack.top();
    if (!m_object_model->validateIndex(group_index)) {
        m_valid = false;
        m_error_callback("Somehow the parent stack was corrupted while validating "
                         "(concurrency?) Report this error to JoshuaMK");
        return *this;
    }

    ModelIndex child_index = m_object_model->getIndex(obj_name, group_index);
    if (!m_object_model->validateIndex(child_index)) {
        m_valid = false;
        m_error_callback(
            std::format("Failed to find child object of type '{}' with name '{}' in parent '{}'",
                        obj_type, obj_name, m_object_model->getObjectKey(group_index)));
        return *this;
    }

    std::string child_type = m_object_model->getObjectType(child_index);
    if (child_type != obj_type) {
        m_valid = false;
        m_error_callback(
            std::format("Failed to find child object of type '{}' with name '{}' in parent '{}'",
                        obj_type, obj_name, m_object_model->getObjectKey(group_index)));
        return *this;
    }

    m_last_index = child_index;
    m_processed_objects += 1;
    if (m_progress_callback) {
        std::string progress_text =
            std::format("{} ({}) [{}]", obj_type, obj_name, static_cast<int>(m_processed_objects));
        m_progress_callback(progress_text);
    }
    return *this;
}

ScenePruner &ScenePruner::scopeEnter() {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (!m_object_model->validateIndex(m_last_index)) {
        m_valid = false;
        m_error_callback("Tried to enter the scope of a leaf object");
        return *this;
    }

    m_parent_stack.push(m_last_index);
    return *this;
}

ScenePruner &ScenePruner::scopeEscape() {
    if (!m_error_callback) {
        m_valid = false;
        return *this;
    }

    if (m_parent_stack.empty()) {
        m_valid = false;
        m_error_callback("Unbalanced scope when validating scene");
        return *this;
    }

    m_parent_stack.pop();
    return *this;
}

SceneMender &SceneMender::scopeForAll(RefPtr<SceneObjModel> model, foreach_obj_fn fn) {
    forEachP(model, m_parent_stack.empty() ? ModelIndex() : m_parent_stack.top(), fn);
    return *this;
}

ScenePruner &ScenePruner::scopeCollectDependencies(RefPtr<SceneObjModel> model,
                                                   const ModelIndex &index) {
    if (!m_check_dependencies || !model->validateIndex(index)) {
        return *this;
    }

    RefPtr<ISceneObject> object = model->getObjectRef(index);
    std::string obj_type        = model->getObjectType(index);
    std::string obj_key         = model->getObjectKey(index);

    auto template_ = TemplateFactory::create(obj_type, true);
    if (!template_) {
        m_valid = false;
        m_error_callback(
            std::format("Object '{} ({})': Failed to load template!", obj_type, obj_key));
        return *this;
    }

    std::optional<TemplateWizard> wizard = template_.value()->getWizard(object->getWizardName());
    if (!wizard) {
        wizard = template_.value()->getWizard("Default");
        if (!wizard) {
            m_valid = false;
            m_error_callback(std::format("Object '{} ({})': Failed to load the wizard '{}'!",
                                         obj_type, obj_key, object->getWizardName()));
            return *this;
        }
    }

    m_dependencies.m_managers.insert(m_dependencies.m_managers.end(),
                                     wizard->m_dependencies.m_managers.begin(),
                                     wizard->m_dependencies.m_managers.end());

    auto rail_member_result = object->getMember("Rail");
    if (rail_member_result.value_or(nullptr)) {
        RefPtr<MetaMember> rail_member = rail_member_result.value();
        std::string rail_dependency    = getMetaValue<std::string>(rail_member).value();
        if (!rail_dependency.empty() && rail_dependency != "(null)") {
            m_dependencies.m_rails.insert(rail_dependency);
        }
    }

    for (const fs_path &asset_path : wizard->m_dependencies.m_asset_paths) {
        // Assets for an object are found in the subdirectory at SceneAssets/{asset_path}/...
        const fs_path abs_asset_path =
            (Filesystem::current_path().value_or(".") / "SceneAssets" / asset_path)
                .lexically_normal();

        if (!Filesystem::is_directory(abs_asset_path).value_or(false)) {
            continue;
        }

        const fs_path &scene_root_path = m_object_model->getScenePath();

        for (const Filesystem::directory_entry dir_entry :
             Filesystem::recursive_directory_iterator(abs_asset_path)) {

            const fs_path relative_path =
                Filesystem::relative(dir_entry.path(), abs_asset_path).value_or(dir_entry.path());
            const fs_path scene_path = scene_root_path / relative_path;

            if (!Filesystem::exists(scene_path).value_or(false)) {
                continue;
            }

            m_dependencies.m_asset_paths.insert(scene_path.lexically_normal());
        }
    }
    return *this;
}

ScenePruner &ScenePruner::scopePruneManagerIfUnused(RefPtr<SceneObjModel> model,
                                                    const ModelIndex &index) {
    if (!m_valid) {
        return *this;
    }

    m_processed_objects += 1;

    for (const TemplateDependencies::ObjectInfo &manager : m_dependencies.m_managers) {
        ModelIndex group_index = m_object_model->getIndex(manager.m_ancestry);
        if (!m_object_model->validateIndex(group_index)) {
            m_valid = false;
            m_error_callback(std::format("Provided manager ancestry '{}' is not a group object!",
                                         manager.m_ancestry.toString()));
            continue;
        }

        ModelIndex manager_index =
            m_object_model->getIndex(manager.m_type, manager.m_name, group_index);

        if (!m_object_model->validateIndex(manager_index)) {
            continue;
        }

        // Manager matches a running dependency
        if (manager_index == index) {
            return *this;
        }
    }

    // Remove the manager since it is not depended upon
    m_indexes_to_prune.push_back(index);

    return *this;
}

ScenePruner &ScenePruner::scopePruneRailIfUnused(RefPtr<RailObjModel> model,
                                                 const ModelIndex &index) {
    if (!model->isIndexRail(index)) {
        return *this;
    }

    const std::string rail_name = model->getRailKey(index);
    if (m_dependencies.m_rails.contains(rail_name)) {
        return *this;
    }

    m_indexes_to_prune.push_back(index);

    return *this;
}

ScenePruner &ScenePruner::scopeForAll(RefPtr<SceneObjModel> model, foreach_obj_fn fn) {
    forEachP(model, m_parent_stack.empty() ? ModelIndex() : m_parent_stack.top(), fn);
    return *this;
}

ScenePruner &ScenePruner::scopeForAll(RefPtr<RailObjModel> model, foreach_rail_fn fn) {
    forEachP(model, m_parent_stack.empty() ? ModelIndex() : m_parent_stack.top(), fn);
    return *this;
}