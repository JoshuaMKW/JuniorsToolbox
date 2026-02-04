#pragma once

#include <unordered_map>

#include <J3D/Data/J3DModelInstance.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <unordered_set>

#include "core/time/timestep.hpp"
#include "core/types.hpp"
#include "gui/appmain/scene/ImGuizmo.h"
#include "gui/appmain/scene/billboard.hpp"
#include "gui/appmain/scene/camera.hpp"
#include "path.hpp"
#include "scene/raildata.hpp"
#include "scene/scene.hpp"

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

        void initializeData(const Scene::SceneInstance &scene);

        [[nodiscard]] bool isUniqueRailColors() { return m_path_renderer.isUniqueColors(); }
        void setUniqueRailColors(bool is_colors) { m_path_renderer.setUniqueColors(is_colors); }

        void updatePaths(const RailData &rail_data, std::unordered_map<UUID64, bool> visible_map) {
            initializePaths(rail_data, visible_map);
        }

        void markDirty() { m_is_view_dirty = true; }

        void getCameraTranslation(glm::vec3 &translation) { m_camera.getPos(translation); }
        void setCameraOrientation(const glm::vec3 &up, const glm::vec3 &translation,
                                  const glm::vec3 &look_at) {
            m_camera.setOrientAndPosition(up, look_at, translation);
            m_camera.updateCamera();
        }

        f32 getCameraFOV() const { return m_camera_fov; }
        void setCameraFOV(f32 fov) {
            m_camera_fov = fov;
            m_camera.setFOV(fov);
        }

        f32 getCameraNearPlane() const { return m_camera_near_plane; }
        void setCameraNearPlane(f32 near_plane) {
            m_camera_near_plane = near_plane;
            m_camera.setNearDist(near_plane);
        }

        f32 getCameraFarPlane() const { return m_camera_far_plane; }
        void setCameraFarPlane(f32 far_plane) {
            m_camera_far_plane = far_plane;
            m_camera.setFarDist(far_plane);
        }

        bool getGizmoVisible() const { return m_render_gizmo; }
        void setGizmoVisible(bool visible) {
            m_render_gizmo = visible;
            ImGuizmo::Enable(visible);
        }

        bool isGizmoActive() const { return m_gizmo_active; }
        bool isGizmoManipulated() const { return m_gizmo_updated; }

        const glm::mat4x4 &getGizmoTransform() const { return m_gizmo_matrix; }
        glm::mat4x4 getGizmoFrameDelta() const {
            return m_gizmo_matrix * glm::inverse(m_gizmo_matrix_prev);
        }
        glm::mat4x4 getGizmoTotalDelta() const {
            return m_gizmo_matrix * glm::inverse(m_gizmo_matrix_start);
        }

        void setGizmoTransform(const glm::mat4x4 &mtx) {
            m_gizmo_matrix_prev = mtx;
            m_gizmo_matrix      = mtx;
        }
        void setGizmoTransform(const Transform &transform);

        void setGizmoOperation(ImGuizmo::OPERATION op) { m_gizmo_op = op; }

        bool inputUpdate(TimeStep delta_time);

        using selection_variant_t =
            std::variant<RefPtr<ISceneObject>, RefPtr<Rail::RailNode>, std::nullopt_t>;

        selection_variant_t findSelection(std::vector<ISceneObject::RenderInfo> renderables,
                                          std::vector<RefPtr<Rail::RailNode>> rail_nodes,
                                          bool &should_reset);

        void render(std::vector<ISceneObject::RenderInfo> renderables, TimeStep delta_time);

    protected:
        void initializePaths(const RailData &rail_data,
                             std::unordered_map<UUID64, bool> visible_map);
        void initializeBillboards();

        void viewportBegin();
        void viewportEnd();

        RefPtr<ISceneObject>
        findObjectByJ3DPicking(std::vector<ISceneObject::RenderInfo> renderables, int selection_x,
                               int selection_y, float &intersection_z,
                               const std::unordered_set<std::string> &exclude_set);

        RefPtr<ISceneObject>
        findObjectByOBBIntersection(std::vector<ISceneObject::RenderInfo> renderables,
                                    int selection_x, int selection_y, float &intersection_z,
                                    const std::unordered_set<std::string> &exclude_set);

    private:
        u32 m_fbo_id, m_tex_id, m_rbo_id;

        bool m_is_window_hovered    = false;
        bool m_is_window_focused    = false;
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
        bool m_gizmo_active;
        ImGuizmo::MODE m_gizmo_mode = ImGuizmo::MODE::WORLD;
        ImGuizmo::OPERATION m_gizmo_op;
        glm::mat4x4 m_gizmo_matrix;
        glm::mat4x4 m_gizmo_matrix_prev;
        glm::mat4x4 m_gizmo_matrix_start;

        f32 m_camera_fov;
        f32 m_camera_near_plane;
        f32 m_camera_far_plane;
    };
}  // namespace Toolbox::UI