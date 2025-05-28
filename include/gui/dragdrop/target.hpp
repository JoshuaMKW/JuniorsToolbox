#pragma once

#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"
#include "platform/process.hpp"

namespace Toolbox::UI {

    class IDragDropTargetDelegate {
    public:
        virtual ~IDragDropTargetDelegate() = default;

        virtual void onDragEnter(RefPtr<DragAction> action) = 0;
        virtual void onDragLeave(RefPtr<DragAction> action) = 0;
        virtual void onDragMove(RefPtr<DragAction> action)  = 0;
        virtual void onDrop(RefPtr<DragAction> action)      = 0;

        virtual bool initializeForWindow(Platform::LowWindow window) = 0;
        virtual void shutdownForWindow(Platform::LowWindow window)   = 0;
    };

}  // namespace Toolbox::UI