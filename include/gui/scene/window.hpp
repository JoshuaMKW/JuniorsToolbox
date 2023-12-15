#pragma once

// #include <glad/glad.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "clone.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"

#include "gui/clipboard.hpp"
#include "gui/property/property.hpp"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "gui/scene/objdialog.hpp"
#include "gui/scene/path.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/window.hpp"

#include <gui/context_menu.hpp>
#include <imgui.h>

namespace Toolbox::UI {

    class SceneWindow final : public DockWindow {
    public:
        SceneWindow();
        ~SceneWindow();

    protected:
        void buildDockspace(ImGuiID dockspace_id) override;
        void renderMenuBar() override;
        void renderBody(f32 delta_time) override;
        void renderHierarchy();
        void renderTree(std::shared_ptr<Object::ISceneObject> node);
        void renderProperties();
        void renderScene(f32 delta_time);
        void renderContextMenu(std::string str_id, NodeInfo &info);

        void buildContextMenuVirtualObj();
        void buildContextMenuGroupObj();
        void buildContextMenuPhysicalObj();
        void buildContextMenuMultiObj();
        void buildCreateObjDialog();
        void buildRenameObjDialog();

    public:
        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow              = ImGui::GetCurrentWindow();
            m_window_class.ClassId                  = ImGui::GetID(title().c_str());
            m_window_class.ParentViewportId         = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed    = false;
            m_window_class.DockingAlwaysTabBar      = false;
            m_window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingOverMe;
            return &m_window_class;
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
        [[nodiscard]] std::string name() const override { return "Scene Editor"; }
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override {
            return {"", "arc", "szs"};
        }

        [[nodiscard]] bool loadData(const std::filesystem::path &path) override;

        [[nodiscard]] bool saveData(const std::filesystem::path &path) override {
            // TODO: Implement saving to archive or folder.
            return false;
        }

        bool update(f32 delta_time) override;

    private:
        void viewportBegin(bool is_dirty);
        void viewportEnd();

        ContextMenu<NodeInfo> m_virtual_node_menu;
        ContextMenu<NodeInfo> m_physical_node_menu;
        ContextMenu<NodeInfo> m_group_node_menu;
        ContextMenu<std::vector<NodeInfo>> m_multi_node_menu;

        ImGuiTextFilter m_hierarchy_filter;

        std::vector<NodeInfo> m_selected_nodes                        = {};
        std::vector<std::unique_ptr<IProperty>> m_selected_properties = {};

        CreateObjDialog m_create_obj_dialog;
        RenameObjDialog m_rename_obj_dialog;

        std::unique_ptr<Toolbox::Scene::SceneInstance> m_current_scene;

        std::vector<std::shared_ptr<J3DModelInstance>> m_renderables;
        ResourceCache m_resource_cache;

        bool m_is_render_window_open;
        Renderer m_renderer;

        uint32_t m_dock_space_id          = 0;
        uint32_t m_dock_node_up_left_id   = 0;
        uint32_t m_dock_node_left_id      = 0;
        uint32_t m_dock_node_down_left_id = 0;

        std::string m_selected_add_zone{""};

        bool m_options_open{false};

        bool m_is_docking_set_up{false};
        bool m_is_file_dialog_open{false};
        bool m_is_save_dialog_open{false};
        bool m_text_editor_active{false};
    };
}  // namespace Toolbox::UI