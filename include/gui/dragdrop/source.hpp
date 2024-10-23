#pragma once

#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"
#include "platform/process.hpp"

namespace Toolbox::UI {

    class IDragDropSourceDelegate {
    public:
        virtual ~IDragDropSourceDelegate() = default;

        enum class DragDropSourceState {
            STATE_ACTIVE,
            STATE_CANCEL,
            STATE_DROP,
        };

        virtual bool startDragDrop(Platform::LowWindow source, MimeData &&data,
                                   DropTypes allowed_types, DropType *result_type_out) = 0;
        virtual DragDropSourceState queryActiveDrag()                                  = 0;
        virtual ImGuiMouseCursor provideCursor()                                       = 0;

        virtual bool initializeForWindow(Platform::LowWindow window) = 0;
        virtual void shutdownForWindow(Platform::LowWindow window)   = 0;
    };

}  // namespace Toolbox::UI