#pragma once

#include "core/memory.hpp"
#include "core/mimedata/mimedata.hpp"
#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"
#include "gui/dragdrop/source.hpp"
#include "gui/dragdrop/target.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#endif

namespace Toolbox::UI {

    class DragDropManager {
    public:
        static DragDropManager &instance() {
            static DragDropManager _instance;
            return _instance;
        }

        bool initialize();
        void shutdown();

        void setSystemAction(bool is_system) { m_is_system_action = is_system; }

        RefPtr<DragAction> createDragAction(UUID64 source_uuid, Platform::LowWindow low_window, MimeData &&data, bool system_level = true);
        RefPtr<DragAction> getCurrentDragAction() const { return m_current_drag_action; }
        void destroyDragAction(RefPtr<DragAction> action);

    protected:
        DragDropManager() = default;

        Result<void, BaseError> createSystemDragDropSource(MimeData &&data);
        Result<void, BaseError> destroySystemDragDropSource();

    private:
#ifdef TOOLBOX_PLATFORM_WINDOWS
        IDataObject *m_data_object = nullptr;
        IDropSource *m_drop_source = nullptr;
        std::thread m_drag_thread;
        bool m_is_thread_running = false;
#elif defined(TOOLBOX_PLATFORM_LINUX)
#endif

        RefPtr<DragAction> m_current_drag_action;
        bool m_is_system_action = false;
    };

    class DragDropDelegateFactory {
    public:
        static ScopePtr<IDragDropSourceDelegate> createDragDropSourceDelegate();
        static ScopePtr<IDragDropTargetDelegate> createDragDropTargetDelegate();
    };

}  // namespace Toolbox::UI