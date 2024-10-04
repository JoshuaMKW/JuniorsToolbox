#pragma once

#include "core/memory.hpp"
#include "core/mimedata/mimedata.hpp"
#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"

namespace Toolbox::UI {
        
    RefPtr<DragAction> DragDropManager::createDragAction() {
        return make_referable<DragAction>();
    }

    void DragDropManager::initialize() {}

}  // namespace Toolbox::UI