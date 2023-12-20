#pragma once

#include <imgui.h>

#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "path.hpp"
#include "types.hpp"
#include <J3D/Data/J3DModelInstance.hpp>
#include <glad/glad.h>
#include <imgui_internal.h>
#include <scene/raildata.hpp>
#include <scene/scene.hpp>

using namespace Toolbox::Scene;

namespace Toolbox::UI {
    namespace Render {
        bool CompileShader(const char *vertex_shader_src, const char *geometry_shader_src,
                           const char *fragment_shader_src, uint32_t &program_id_out);
    }

    class Renderer {
    public:
        Renderer();
        ~Renderer() {
            glDeleteFramebuffers(1, &m_fbo_id);
            glDeleteRenderbuffers(1, &m_rbo_id);
            glDeleteTextures(1, &m_tex_id);
        }

        void initializeData(const SceneInstance &scene);

        void updatePaths(const RailData &rail_data) {
            initializePaths(rail_data);
        }

        void markDirty() { m_is_dirty = true; }
        void getCameraTranslation(glm::vec3 &translation) { m_camera.getPos(translation); }
        void setCameraOrientation(const glm::vec3 &up, const glm::vec3 &translation,
                                  const glm::vec3 &look_at) {
            m_camera.setOrientAndPosition(up, translation, look_at);
            m_camera.updateCamera();
        }

        bool inputUpdate();
        void render(std::vector<std::shared_ptr<J3DModelInstance>> renderables, f32 delta_time);

    protected:
        void initializePaths(const RailData &rail_data);
        void initializeBillboards();

        void viewportBegin();
        void viewportEnd();

    private:
        u32 m_fbo_id, m_tex_id, m_rbo_id;

        bool m_is_dirty          = true;
        bool m_is_window_hovered = false;
        bool m_is_window_focused = false;

        BillboardRenderer m_billboard_renderer;
        PathRenderer m_path_renderer;
        Camera m_camera = {};

        uint32_t m_gizmo_operation{0};

        ImVec2 m_render_size;
        ImRect m_window_rect;
        ImVec2 m_window_size;
        ImVec2 m_window_size_prev;
    };
}  // namespace Toolbox::UI