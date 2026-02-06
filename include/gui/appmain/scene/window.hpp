#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "core/threaded.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/appmain/property/property.hpp"
#include "gui/appmain/scene/billboard.hpp"
#include "gui/appmain/scene/camera.hpp"
#include "gui/appmain/scene/events.hpp"
#include "gui/appmain/scene/nodeinfo.hpp"
#include "gui/appmain/scene/objdialog.hpp"
#include "gui/appmain/scene/path.hpp"
#include "gui/appmain/scene/renderer.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/selection.hpp"
#include "gui/window.hpp"

#include "model/objmodel.hpp"
#include "model/selection.hpp"

#include "raildialog.hpp"
#include <gui/context_menu.hpp>
#include <imgui.h>

class ToolboxSceneVerifier : public TaskThread<void> {
public:
    ToolboxSceneVerifier() = delete;
    ToolboxSceneVerifier(RefPtr<const Scene::SceneInstance> scene, bool check_dependencies)
        : m_scene(scene), m_check_dependencies(check_dependencies) {}

    void tRun(void *param) override;

    bool isValid() const { return m_successful; }
    std::vector<std::string> getErrors() const { return m_errors; }
    std::string getProgressText() const { return m_progress_text; }

private:
    RefPtr<const Scene::SceneInstance> m_scene;
    bool m_check_dependencies = true;

    std::string m_progress_text;
    std::vector<std::string> m_errors;
    bool m_successful = true;
};

class ToolboxSceneDependencyMender : public TaskThread<void> {
public:
    ToolboxSceneDependencyMender() = delete;
    ToolboxSceneDependencyMender(RefPtr<const Scene::SceneInstance> scene) : m_scene(scene) {}

    void tRun(void *param) override;

    bool isValid() const { return m_successful; }
    std::vector<std::string> getChanges() const { return m_changes; }
    std::vector<std::string> getErrors() const { return m_errors; }
    std::string getProgressText() const { return m_progress_text; }

private:
    RefPtr<const Scene::SceneInstance> m_scene;

    std::string m_progress_text;
    std::vector<std::string> m_changes;
    std::vector<std::string> m_errors;
    bool m_successful = true;
};

namespace Toolbox::UI {

    class SceneWindow final : public ImWindow {
    public:
        SceneWindow(const std::string &name);
        ~SceneWindow() = default;

        enum class EditorWindow {
            NONE,
            OBJECT_TREE,
            PROPERTY_EDITOR,
            RAIL_TREE,
            RENDER_VIEW,
        };

        using render_layer_cb =
            std::function<void(TimeStep delta_time, std::string_view layer_name, int width,
                               int height, const glm::mat4x4 &vp_mtx, UUID64 window_uuid)>;

        void registerOverlay(const std::string &layer_name, render_layer_cb cb) {
            m_render_layers[layer_name] = cb;
        }

        void deregisterOverlay(const std::string &layer_name) { m_render_layers.erase(layer_name); }

        void initToBasic() { m_current_scene = Scene::SceneInstance::BasicScene(); }
        void setIOContextPath(const fs_path &path) { m_io_context_path = path; }

        void setStageScenario(u8 stage, u8 scenario);

    protected:
        ImGuiID onBuildDockspace() override;
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderSanitizationSteps();

        void renderHierarchy();
        void renderRailEditor();
        void renderScene(TimeStep delta_time);
        void renderDolphin(TimeStep delta_time);
        void renderPlaybackButtons(TimeStep delta_time);
        void renderScenePeripherals(TimeStep delta_time);

        void renderSceneObjectTree(const ModelIndex &index);
        void renderTableObjectTree(const ModelIndex &index);
        void renderSceneHierarchyContextMenu(std::string str_id, const ModelIndex &obj_index);
        void renderTableHierarchyContextMenu(std::string str_id, const ModelIndex &obj_index);

        void renderRailContextMenu(std::string str_id, SelectionNodeInfo<Rail::Rail> &info);
        void renderRailNodeContextMenu(std::string str_id, SelectionNodeInfo<Rail::RailNode> &info);

        void renderProperties();
        static bool renderEmptyProperties(SceneWindow &window) { return false; }
        static bool renderObjectProperties(SceneWindow &window);
        static bool renderRailProperties(SceneWindow &window);
        static bool renderRailNodeProperties(SceneWindow &window);

        void calcDolphinVPMatrix();
        void reassignAllActorPtrs(u32 param);

        void buildContextMenuSceneObj();

        void buildContextMenuRail();
        void buildContextMenuMultiRail();
        void buildContextMenuRailNode();
        void buildContextMenuMultiRailNode();

        void buildCreateObjDialog();
        void buildRenameObjDialog();
        void buildCreateRailDialog();
        void buildRenameRailDialog();

        void saveMimeObject(Buffer &buffer, size_t index, RefPtr<ISceneObject> parent);
        void saveMimeRail(Buffer &buffer, size_t index);
        void saveMimeRailNode(Buffer &buffer, size_t index, RefPtr<Rail::Rail> parent);

        void loadMimeObject(Buffer &buffer, size_t index, UUID64 parent_id);
        void loadMimeRail(Buffer &buffer, size_t index);
        void loadMimeRailNode(Buffer &buffer, size_t index, UUID64 rail_id);

        void processObjectSelection(RefPtr<Object::ISceneObject> node, bool is_multi);
        void processRailSelection(RefPtr<Rail::Rail> node, bool is_multi);
        void processRailNodeSelection(RefPtr<Rail::RailNode> node, bool is_multi);

        void calcNewGizmoMatrixFromSelection();

    private:
        void _moveNode(const Rail::RailNode &node, size_t index, UUID64 rail_id, size_t orig_index,
                       UUID64 orig_id, bool is_internal);

    public:
        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_MenuBar;
        }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow           = ImGui::GetCurrentWindow();
            m_window_class.ClassId               = (ImGuiID)getUUID();
            m_window_class.ParentViewportId      = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed = true;
            m_window_class.DockingAlwaysTabBar   = false;
            return nullptr;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {800, 700}
            };
        }
        std::optional<ImVec2> maxSize() const override { return std::nullopt; }

        [[nodiscard]] std::string context() const override {
            if (!m_current_scene)
                return "(unknown)";
            return m_current_scene->rootPath() ? m_current_scene->rootPath().value().string()
                                               : "(unknown)";
        }
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override {
            return {"", "arc", "szs"};
        }

        [[nodiscard]] bool onLoadData(const fs_path &path) override;
        [[nodiscard]] bool onSaveData(std::optional<fs_path> path) override;

        void onAttach() override;
        void onDetach() override;

        void onImGuiUpdate(TimeStep delta_time) override;
        void onImGuiPostUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;
        void onEvent(RefPtr<BaseEvent> ev) override;

    private:
        u8 m_stage = 0xFF, m_scenario = 0xFF;
        RefPtr<Scene::SceneInstance> m_current_scene;

        fs_path m_io_context_path;
        bool m_repack_io_busy = false;

        // Hierarchy view
        ImGuiTextFilter m_hierarchy_filter;

        RefPtr<SceneObjModel> m_scene_object_model;
        RefPtr<SceneObjModel> m_table_object_model;

        ModelSelectionManager m_scene_selection_mgr;
        ModelSelectionManager m_table_selection_mgr;

        ContextMenu<ModelIndex> m_scene_hierarchy_context_menu;
        ContextMenu<ModelIndex> m_table_hierarchy_context_menu;

        // Property editor
        std::function<bool(SceneWindow &)> m_properties_render_handler;
        std::vector<ScopePtr<IProperty>> m_selected_properties = {};

        // Object modals
        CreateObjDialog m_create_scene_obj_dialog;
        RenameObjDialog m_rename_scene_obj_dialog;

        CreateObjDialog m_create_table_obj_dialog;
        RenameObjDialog m_rename_table_obj_dialog;

        // Render view
        bool m_update_render_objs    = false;
        bool m_is_render_window_open = false;
        Renderer m_renderer;
        std::vector<ISceneObject::RenderInfo> m_renderables = {};
        ResourceCache m_resource_cache;

        std::vector<Transform> m_selection_transforms;
        bool m_selection_transforms_needs_update = false;
        bool m_gizmo_maniped                     = false;

        // Docking facilities
        ImGuiID m_dock_space_id          = 0;
        ImGuiID m_dock_node_up_left_id   = 0;
        ImGuiID m_dock_node_left_id      = 0;
        ImGuiID m_dock_node_down_left_id = 0;

        // Rail editor
        std::unordered_map<UUID64, bool> m_rail_visible_map = {};
        bool m_connections_open                             = true;

        std::vector<SelectionNodeInfo<Rail::Rail>> m_rail_list_selected_nodes = {};
        ContextMenu<SelectionNodeInfo<Rail::Rail>> m_rail_list_single_node_menu;
        ContextMenu<std::vector<SelectionNodeInfo<Rail::Rail>>> m_rail_list_multi_node_menu;

        std::vector<SelectionNodeInfo<Rail::RailNode>> m_rail_node_list_selected_nodes = {};
        ContextMenu<SelectionNodeInfo<Rail::RailNode>> m_rail_node_list_single_node_menu;
        ContextMenu<std::vector<SelectionNodeInfo<Rail::RailNode>>>
            m_rail_node_list_multi_node_menu;

        // Rail modals
        CreateRailDialog m_create_rail_dialog;
        RenameRailDialog m_rename_rail_dialog;

        EditorWindow m_focused_window = EditorWindow::NONE;

        ImGuiWindow *m_hierarchy_window = nullptr;
        ImGuiWindow *m_rail_list_window = nullptr;

        std::string m_selected_add_zone{""};

        bool m_options_open = false;

        bool m_is_save_default_ready  = false;
        bool m_is_save_as_dialog_open = false;
        bool m_is_verify_open         = false;

        bool m_is_game_edit_mode = false;

        ImageHandle m_dolphin_image;
        ImagePainter m_dolphin_painter;

        glm::mat4x4 m_dolphin_vp_mtx = {};
        std::map<std::string, render_layer_cb> m_render_layers;

        // Event Stuff
        bool m_control_disable_requested = false;

        // Drag drop stuff
        UUID64 m_object_parent_uuid;
        int m_object_drop_target = -1;

        int m_rail_drop_target = -1;

        UUID64 m_rail_node_rail_uuid;
        int m_rail_node_drop_target = -1;

        Toolbox::Buffer m_drop_target_buffer;

        ScopePtr<ToolboxSceneVerifier> m_scene_verifier;
        ScopePtr<ToolboxSceneDependencyMender> m_scene_mender;
        bool m_scene_validator_result_opened = false;
        bool m_scene_mender_result_opened    = false;
    };
}  // namespace Toolbox::UI