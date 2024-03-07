#pragma once

#include <imgui.h>
#include <unordered_map>

#include "gui/scene/ImGuizmo.h"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "path.hpp"
#include "core/types.hpp"
#include <J3D/Data/J3DModelInstance.hpp>
#include <imgui_internal.h>
#include <scene/raildata.hpp>
#include <scene/scene.hpp>

using namespace Toolbox;

namespace Toolbox::UI {
    namespace Render {
        bool CompileShader(const char *vertex_shader_src, const char *geometry_shader_src,
                           const char *fragment_shader_src, uint32_t &program_id_out);
    }

    class Renderer {
    public:
        Renderer();
        ~Renderer();

        void initializeData(const SceneInstance &scene);

        void updatePaths(const RailData &rail_data,
                         std::unordered_map<UUID64, bool> visible_map) {
            initializePaths(rail_data, visible_map);
        }

        void markDirty() { m_is_view_dirty = true; }

        void getCameraTranslation(glm::vec3 &translation) { m_camera.getPos(translation); }
        void setCameraOrientation(const glm::vec3 &up, const glm::vec3 &translation,
                                  const glm::vec3 &look_at) {
            m_camera.setOrientAndPosition(up, look_at, translation);
            m_camera.updateCamera();
        }

        void setGizmoVisible(bool visible) { m_render_gizmo = visible; }
        bool isGizmoManipulated() const { return m_gizmo_updated; }
        glm::mat4x4 getGizmoTransform() const { return m_gizmo_matrix; }
        void setGizmoTransform(const glm::mat4x4 &mtx) { m_gizmo_matrix = mtx; }
        void setGizmoOperation(ImGuizmo::OPERATION op) { m_gizmo_op = op; }

        bool inputUpdate(f32 delta_time);

        using selection_variant_t =
            std::variant<RefPtr<ISceneObject>, RefPtr<Rail::RailNode>, std::nullopt_t>;

        selection_variant_t findSelection(std::vector<ISceneObject::RenderInfo> renderables,
                                          std::vector<RefPtr<Rail::RailNode>> rail_nodes,
                                          bool &should_reset);

        void render(std::vector<ISceneObject::RenderInfo> renderables, f32 delta_time);

    protected:
        void initializePaths(const RailData &rail_data,
                             std::unordered_map<UUID64, bool> visible_map);
        void initializeBillboards();

        void viewportBegin();
        void viewportEnd();

    private:
        u32 m_fbo_id, m_tex_id, m_rbo_id;

        bool m_is_window_hovered = false;
        bool m_is_window_focused = false;
        bool m_is_view_manipulating = false;
        bool m_is_view_dirty        = true;

        BillboardRenderer m_billboard_renderer;
        PathRenderer m_path_renderer;
        Camera m_camera = {};

        ImRect m_window_rect;
        ImVec2 m_window_size;
        ImVec2 m_window_size_prev;
        ImRect m_render_rect;
        ImVec2 m_render_size;

        // Gizmo data
        bool m_render_gizmo;
        bool m_gizmo_updated;
        ImGuizmo::MODE m_gizmo_mode = ImGuizmo::MODE::WORLD;
        ImGuizmo::OPERATION m_gizmo_op;
        glm::mat4x4 m_gizmo_matrix;
    };
}  // namespace Toolbox::UI