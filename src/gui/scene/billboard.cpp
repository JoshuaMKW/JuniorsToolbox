#include "gui/scene/billboard.hpp"
#include <filesystem>
#include <gui/scene/renderer.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "gui/stb_image.h"
#include <glad/glad.h>

namespace Toolbox::UI {
    const char *bb_vtx_shader_source = "#version 330\n\
        layout (location = 0) in vec3 position;\n\
        layout (location = 1) in int tex;\n\
        layout (location = 2) in int size;\n\
        flat out int tex_idx;\n\
        uniform mat4 gpu_ModelViewProjectionMatrix;\n\
        void main()\n\
        {\n\
            gl_Position = gpu_ModelViewProjectionMatrix * vec4(position, 1.0);\n\
            gl_PointSize = min(size * 1000, size * 1000 / gl_Position.w);\n\
            tex_idx = tex;\n\
        }\
    ";

    const char *bb_frg_shader_source = "#version 330\n\
        uniform sampler2DArray spriteTexture;\n\
        flat in int tex_idx;\n\
        void main()\n\
        {\n\
            gl_FragColor = texture(spriteTexture, vec3(gl_PointCoord, tex_idx));\n\
            if(gl_FragColor.a < 1.0 / 255.0) discard;\n\
        }\
    ";

    bool BillboardRenderer::initBillboardRenderer(int billboardResolution,
                                                  int billboardImageCount) {

        m_billboard_resolution = billboardResolution;
        m_texture_count        = billboardImageCount;

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, billboardResolution, billboardResolution,
                     billboardImageCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

        // compile shaders
        if (!Toolbox::UI::Render::CompileShader(bb_vtx_shader_source, nullptr, bb_frg_shader_source,
                                                m_program)) {
            return false;
        }

        m_mvp_uniform = glGetUniformLocation(m_program, "gpu_ModelViewProjectionMatrix");

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Billboard),
                              (void *)offsetof(Billboard, m_position));
        glEnableVertexAttribArray(1);
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(Billboard),
                               (void *)offsetof(Billboard, m_texture_idx));
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Billboard),
                               (void *)offsetof(Billboard, m_sprite_size));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    BillboardRenderer::BillboardRenderer() {}

    BillboardRenderer::~BillboardRenderer() { glDeleteTextures(1, &m_texture); }

    void BillboardRenderer::loadBillboardTexture(std::filesystem::path imagePath,
                                                 int textureIndex) {

        if (textureIndex > m_texture_count || !std::filesystem::exists(imagePath)) {
            // show an error
            return;
        }

        int x, y, n;
        unsigned char *img = stbi_load(imagePath.string().c_str(), &x, &y, &n, 0);

        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture);

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, textureIndex, m_billboard_resolution,
                        m_billboard_resolution, 1, GL_RGBA, GL_UNSIGNED_BYTE, img);

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

        stbi_image_free(img);
    }

    void BillboardRenderer::drawBillboards(Camera *camera) {
        if (m_billboards.size() == 0)
            return;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_PROGRAM_POINT_SIZE);

        glm::mat4 mvp = camera->getProjMatrix() * camera->getViewMatrix() * glm::mat4(1.0);

        glUseProgram(0);
        glBindVertexArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Billboard) * m_billboards.size(), m_billboards.data(),
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glUseProgram(m_program);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture);

        glBindVertexArray(m_vao);

        glUniformMatrix4fv(m_mvp_uniform, 1, 0, &mvp[0][0]);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_billboards.size()));

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glBindVertexArray(0);
    }
}  // namespace Toolbox::UI