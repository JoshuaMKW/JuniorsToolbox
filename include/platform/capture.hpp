#pragma once

#include "platform/process.hpp"
#include <glad/glad.h>

namespace Toolbox::Platform {

    GLuint CaptureWindowTexture(const ProcessInformation &process, std::string_view name_selector,
                                int width, int height);

}