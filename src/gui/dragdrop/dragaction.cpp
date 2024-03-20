#pragma once

#include "gui/dragdrop/dragaction.hpp"

namespace Toolbox::UI {

    void DragAction::cancel() {}

    DropType DragAction::execute(DropTypes supported_drop_types) { return DropType::ACTION_NONE; }

    DropType DragAction::execute(DropTypes supported_drop_types, DropType default_drop_type) {
        return default_drop_type;
    }

}  // namespace Toolbox::UI