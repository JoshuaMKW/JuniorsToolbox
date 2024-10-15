#pragma once

#include "core/mimedata/mimedata.hpp"
#include "image/imagehandle.hpp"

namespace Toolbox::UI {

    enum class DropType {
        ACTION_NONE,
        ACTION_COPY = (1 << 0),
        ACTION_MOVE = (1 << 1),
        ACTION_LINK = (1 << 2),

        // Windows only
        ACTION_TARGET_MOVE = 0x8002,
    };
    TOOLBOX_BITWISE_ENUM(DropType);

    using DropTypes = DropType;

}