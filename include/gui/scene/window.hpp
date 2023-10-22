#pragma once

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

#include "gui/scene/camera.hpp"
#include "gui/window.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class SceneWindow final : public DockWindow {
    public:
        SceneWindow();
        ~SceneWindow();

    protected:
        void setLights();

        void buildDockspace(ImGuiID dockspace_id) override;
        void renderMenuBar() override;
        void renderBody(f32 delta_time) override;

    public:
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

        [[nodiscard]] bool loadData(const std::filesystem::path &path) override {
            if (!Toolbox::exists(path)) {
                return false;
            }

            if (Toolbox::is_directory(path)) {
                if (path.filename() != "scene") {
                    return false;
                }
                m_current_scene = std::make_unique<Toolbox::Scene::SceneInstance>(path);
                return true;
            }

            // TODO: Implement opening from archives.
            return false;
        }

        [[nodiscard]] bool saveData(const std::filesystem::path &path) override {
            // TODO: Implement saving to archive or folder.
            return false;
        }

        bool update(f32 delta_time) override;

    private:
        std::unique_ptr<Toolbox::Scene::SceneInstance> m_current_scene;

        std::vector<std::shared_ptr<J3DModelInstance>> m_renderables;

        uint32_t m_gizmo_operation{0};

        Camera m_camera;

        uint32_t m_dock_space_id          = 0;
        uint32_t m_dock_node_up_left_id   = 0;
        uint32_t m_dock_node_left_id      = 0;
        uint32_t m_dock_node_down_left_id = 0;

        std::string m_selected_add_zone{""};

        bool m_options_open{false};

        bool m_is_docking_set_up{false};
        bool m_is_file_dialog_open{false};
        bool m_is_save_dialog_open{false};
        bool m_set_lights{false};
        bool m_text_editor_active{false};
    };
}  // namespace Toolbox::UI