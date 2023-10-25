#include <glad/glad.h>
#include <gui/scene/rendercommon.hpp>
#include <iostream>

namespace Toolbox::UI::Render {
    bool CompileShader(const char *vertex_shader_src, const char *geometry_shader_src,
                       const char *fragment_shader_src, uint32_t &program_id_out) {

        char glErrorLogBuffer[4096];
        uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
        // uint32_t gs = glCreateShader(GL_GEOMETRY_SHADER);  // TODO: add this
        uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vs, 1, &vertex_shader_src, nullptr);
        glShaderSource(fs, 1, &fragment_shader_src, nullptr);

        glCompileShader(vs);

        int32_t status;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
        if (status == 0) {
            GLint infoLogLength;
            glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &infoLogLength);

            glGetShaderInfoLog(vs, infoLogLength, nullptr, glErrorLogBuffer);

            std::cout << "Compile failure in vertex shader:" << glErrorLogBuffer << std::endl;

            return false;
        }

        glCompileShader(fs);

        glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
        if (status == 0) {
            GLint infoLogLength;
            glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &infoLogLength);

            glGetShaderInfoLog(fs, infoLogLength, nullptr, glErrorLogBuffer);

            std::cout << "Compile failure in fragment shader: " << glErrorLogBuffer << std::endl;

            return false;
        }

        program_id_out = glCreateProgram();

        glAttachShader(program_id_out, vs);
        glAttachShader(program_id_out, fs);

        glLinkProgram(program_id_out);

        glGetProgramiv(program_id_out, GL_LINK_STATUS, &status);
        if (status == 0) {
            GLint logLen;
            glGetProgramiv(program_id_out, GL_INFO_LOG_LENGTH, &logLen);
            glGetProgramInfoLog(program_id_out, logLen, nullptr, glErrorLogBuffer);

            std::cout << "Shader Program Linking Error: " << glErrorLogBuffer << std::endl;

            return false;
        }

        glDetachShader(program_id_out, vs);
        glDetachShader(program_id_out, fs);

        glDeleteShader(vs);
        glDeleteShader(fs);

        return true;
    }
}  // namespace Toolbox::UI::Render