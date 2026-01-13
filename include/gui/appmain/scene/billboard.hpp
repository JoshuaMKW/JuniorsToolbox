#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <gui/appmain/scene/camera.hpp>
#include <vector>

namespace Toolbox::UI {

    typedef struct {
        glm::vec3 m_position;
        int32_t m_sprite_size;
        int32_t m_texture_idx;
    } Billboard;

    class BillboardRenderer {
        uint32_t m_program;
        uint32_t m_texture;
        uint32_t m_mvp_uniform;
        int m_billboard_resolution, m_texture_count;

        uint32_t m_vao;
        uint32_t m_vbo;

    public:
        std::vector<Billboard> m_billboards;

        void loadBillboardTexture(std::filesystem::path imagePath, int textureIndex);
        void drawBillboards(Camera *camera);

        [[nodiscard]] bool initBillboardRenderer(int billboardResolution, int billboardImageCount);

        BillboardRenderer();
        ~BillboardRenderer();
    };
}  // namespace Toolbox::UI