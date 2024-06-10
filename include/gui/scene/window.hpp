#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/property/property.hpp"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "gui/scene/objdialog.hpp"
#include "gui/scene/path.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/window.hpp"

#include "raildialog.hpp"
#include <gui/context_menu.hpp>
#include <imgui.h>

namespace Toolbox::UI {

#define SCENE_CREATE_RAIL_EVENT     100
#define SCENE_DISABLE_CONTROL_EVENT 101
#define SCENE_ENABLE_CONTROL_EVENT  102

    class SceneCreateRailEvent : public BaseEvent {
    private:
        SceneCreateRailEvent() = default;

    public:
        SceneCreateRailEvent(const SceneCreateRailEvent &)     = default;
        SceneCreateRailEvent(SceneCreateRailEvent &&) noexcept = default;

        SceneCreateRailEvent(const UUID64 &target_id, const Rail::Rail &rail);

        [[nodiscard]] const Rail::Rail &getRail() const noexcept { return m_rail; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        SceneCreateRailEvent &operator=(const SceneCreateRailEvent &)     = default;
        SceneCreateRailEvent &operator=(SceneCreateRailEvent &&) noexcept = default;

    private:
        Rail::Rail m_rail;
    };

    class SceneWindow final : public ImWindow {
    public:
        SceneWindow(const std::string &name);
        ~SceneWindow();

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

    protected:
        ImGuiID onBuildDockspace() override;
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderHierarchy();
        void renderTree(RefPtr<Object::ISceneObject> node);
        void renderRailEditor();
        void renderScene(TimeStep delta_time);
        void renderDolphin(TimeStep delta_time);
        void renderPlaybackButtons(TimeStep delta_time);
        void renderScenePeripherals(TimeStep delta_time);
        void renderHierarchyContextMenu(std::string str_id,
                                        SelectionNodeInfo<Object::ISceneObject> &info);
        void renderRailContextMenu(std::string str_id, SelectionNodeInfo<Rail::Rail> &info);
        void renderRailNodeContextMenu(std::string str_id, SelectionNodeInfo<Rail::RailNode> &info);

        void renderProperties();
        static bool renderEmptyProperties(SceneWindow &window) { return false; }
        static bool renderObjectProperties(SceneWindow &window);
        static bool renderRailProperties(SceneWindow &window);
        static bool renderRailNodeProperties(SceneWindow &window);

        void calcDolphinVPMatrix();
        void reassignAllActorPtrs(u32 param);

        void buildContextMenuVirtualObj();
        void buildContextMenuGroupObj();
        void buildContextMenuPhysicalObj();
        void buildContextMenuMultiObj();

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

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override;
        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override;

        void onAttach() override;
        void onDetach() override;

        void onImGuiUpdate(TimeStep delta_time) override;
        void onImGuiPostUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;
        void onEvent(RefPtr<BaseEvent> ev) override;

    private:
        ScopePtr<Toolbox::SceneInstance> m_current_scene;

        // Hierarchy view
        ImGuiTextFilter m_hierarchy_filter;
        std::vector<SelectionNodeInfo<Object::ISceneObject>> m_hierarchy_selected_nodes = {};
        ContextMenu<SelectionNodeInfo<Object::ISceneObject>> m_hierarchy_virtual_node_menu;
        ContextMenu<SelectionNodeInfo<Object::ISceneObject>> m_hierarchy_physical_node_menu;
        ContextMenu<SelectionNodeInfo<Object::ISceneObject>> m_hierarchy_group_node_menu;
        ContextMenu<std::vector<SelectionNodeInfo<Object::ISceneObject>>>
            m_hierarchy_multi_node_menu;

        // Property editor
        std::function<bool(SceneWindow &)> m_properties_render_handler;
        std::vector<ScopePtr<IProperty>> m_selected_properties = {};

        // Object modals
        CreateObjDialog m_create_obj_dialog;
        RenameObjDialog m_rename_obj_dialog;

        // Render view
        bool m_update_render_objs    = false;
        bool m_is_render_window_open = false;
        Renderer m_renderer;
        std::vector<ISceneObject::RenderInfo> m_renderables = {};
        ResourceCache m_resource_cache;

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
    };
}  // namespace Toolbox::UI