#pragma once

#include <expected>
#include <string_view>

#include "fsystem.hpp"

namespace Toolbox::Platform {

    enum class SystemSound { S_NOTIFICATION, S_ERROR, S_WARNING, S_SUCCESS };

    Result<void, BaseError> PlayAudio(const fs_path &path, bool loop);
    Result<void, BaseError> PlaySystemSound(SystemSound sound_type);

}  // namespace Toolbox::Platform