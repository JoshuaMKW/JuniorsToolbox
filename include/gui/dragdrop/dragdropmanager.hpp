#pragma once

#include "core/memory.hpp"
#include "core/mimedata/mimedata.hpp"
#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"

namespace Toolbox::UI {

    class DragDropManager {
    public:
        static DragDropManager &instance() {
            static DragDropManager _instance;
            return _instance;
        }

        void initialize();
        
        RefPtr<DragAction> createDragAction();


    protected:
        DragDropManager() = default;
    };

}  // namespace Toolbox::UI