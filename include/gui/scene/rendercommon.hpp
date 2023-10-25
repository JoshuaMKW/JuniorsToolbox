
#include <cstdint>

namespace Toolbox::UI::Render {
    bool CompileShader(const char *vertex_shader_src, const char *geometry_shader_src,
                       const char *fragment_shader_src, uint32_t &program_id_out);
}