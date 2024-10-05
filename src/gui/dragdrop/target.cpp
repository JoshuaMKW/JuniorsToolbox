#include "gui/dragdrop/target.hpp"
#include "gui/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/event/dragevent.hpp"
#include "gui/event/dropevent.hpp"
#include "gui/window.hpp"

namespace Toolbox::UI {

    static UUID64 searchDropTarget(const ImVec2 &mouse_pos) {
        std::vector<RefPtr<ImWindow>> windows = GUIApplication::instance().getWindows();

        for (RefPtr<ImWindow> window : windows) {
            ImRect rect = {window->getPos(), window->getPos() + window->getSize()};
            if (rect.Contains(mouse_pos)) {
                return window->getUUID();
            }
        }

        return 0;
    }

#ifdef TOOLBOX_PLATFORM_WINDOWS

#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

    static MimeData createMimeDataFromDataObject(IDataObject *pDataObj) {
        IEnumFORMATETC *enum_format_etc = nullptr;
        {
            HRESULT hr = pDataObj->EnumFormatEtc(DATADIR_GET, &enum_format_etc);
            if (FAILED(hr)) {
                TOOLBOX_ERROR_V("Failed to enumerate data formats: {}", hr);
                return MimeData();
            }
        }

        MimeData mime_data;

        FORMATETC format_etc;
        while (enum_format_etc->Next(1, &format_etc, nullptr) == S_OK) {
            if (format_etc.cfFormat == CF_HDROP) {
                STGMEDIUM stg_medium;
                {
                    HRESULT hr = pDataObj->GetData(&format_etc, &stg_medium);
                    if (FAILED(hr)) {
                        TOOLBOX_ERROR_V("Failed to get data for format: {}", hr);
                        return MimeData();
                    }
                }

                std::string uri_paths;

                HDROP hdrop = (HDROP)GlobalLock(stg_medium.hGlobal);

                if (hdrop) {
                    UINT num_files = DragQueryFile(hdrop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT i = 0; i < num_files; ++i) {
                        TCHAR file_path[MAX_PATH];
                        if (UINT length = DragQueryFile(hdrop, i, file_path, MAX_PATH)) {
                            uri_paths +=
                                std::format("file:///{}\n", std::string(file_path, length));
                        }
                    }
                }

                GlobalUnlock(stg_medium.hGlobal);

                mime_data.set_urls(uri_paths);
            } else if (format_etc.cfFormat == CF_UNICODETEXT) {
                STGMEDIUM stg_medium;
                {
                    HRESULT hr = pDataObj->GetData(&format_etc, &stg_medium);
                    if (FAILED(hr)) {
                        TOOLBOX_ERROR_V("Failed to get data for format: {}", hr);
                        return MimeData();
                    }
                }

                std::string text_data;

                if ((format_etc.tymed & TYMED_HGLOBAL) == TYMED_HGLOBAL) {
                    LPWSTR text = (LPWSTR)GlobalLock(stg_medium.hGlobal);
                    if (text) {
                        std::wstring wdata = std::wstring(text);
                        text_data          = std::string(wdata.begin(), wdata.end());
                        GlobalUnlock(stg_medium.hGlobal);
                    }
                }

                mime_data.set_text(text_data);
            } else if (format_etc.cfFormat == CF_TEXT) {
                STGMEDIUM stg_medium;
                {
                    HRESULT hr = pDataObj->GetData(&format_etc, &stg_medium);
                    if (FAILED(hr)) {
                        TOOLBOX_ERROR_V("Failed to get data for format: {}", hr);
                        return MimeData();
                    }
                }

                std::string text_data;

                if ((format_etc.tymed & TYMED_HGLOBAL) == TYMED_HGLOBAL) {
                    LPSTR text = (LPSTR)GlobalLock(stg_medium.hGlobal);
                    if (text) {
                        text_data = text;
                        GlobalUnlock(stg_medium.hGlobal);
                    }
                }

                mime_data.set_text(text_data);
            }
        }

        return mime_data;
    }

    class WindowsDragDropTargetDelegate;

    class WindowsDragDropTarget : public IDropTarget {
    public:
        WindowsDragDropTarget(WindowsDragDropTargetDelegate *delegate, Platform::LowWindow window)
            : m_delegate(delegate), m_window_handle(window), m_refCount() {}

        ~WindowsDragDropTarget() = default;

        Platform::LowWindow GetWindowHandle() const { return m_window_handle; }

        HRESULT Initialize() { return RegisterDragDrop(m_window_handle, this); }
        HRESULT Shutdown() { return RevokeDragDrop(m_window_handle); }

        // IUnknown Methods
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override {
            if (riid == IID_IUnknown || riid == IID_IDropTarget) {
                *ppvObject = static_cast<IDropTarget *>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refCount); }

        ULONG STDMETHODCALLTYPE Release() override {
            ULONG refCount = InterlockedDecrement(&m_refCount);
            if (refCount == 0) {
                delete this;
            }
            return refCount;
        }

        // IDropTarget
        HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt,
                                            DWORD *pdwEffect) override;
        HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
        HRESULT STDMETHODCALLTYPE DragLeave() override;
        HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt,
                                       DWORD *pdwEffect) override;

    private:
        WindowsDragDropTargetDelegate *m_delegate = nullptr;
        Platform::LowWindow m_window_handle       = nullptr;
        LONG m_refCount;  // Reference count for COM object
    };

    class WindowsDragDropTargetDelegate : public IDragDropTargetDelegate {
    public:
        friend class WindowsDragDropTarget;

        WindowsDragDropTargetDelegate() = default;
        ~WindowsDragDropTargetDelegate() override { m_targets.clear(); }

        void onDragEnter(RefPtr<DragAction> action) override;
        void onDragLeave(RefPtr<DragAction> action) override;
        void onDragMove(RefPtr<DragAction> action) override;
        void onDrop(RefPtr<DragAction> action) override;

        bool initializeForWindow(Platform::LowWindow window) override;
        void shutdownForWindow(Platform::LowWindow window) override;

    private:
        std::vector<WindowsDragDropTarget *> m_targets = {};
    };

#elif defined(TOOLBOX_PLATFORM_LINUX)
    class LinuxDragDropTargetDelegate : public IDragDropTargetDelegate {
    public:
        LinuxDragDropTargetDelegate() {}
        ~LinuxDragDropTargetDelegate() override = default;

        void onDragEnter(RefPtr<DragAction> action) override;
        void onDragLeave(RefPtr<DragAction> action) override;
        void onDragMove(RefPtr<DragAction> action) override;
        void onDrop(RefPtr<DragAction> action) override;

        bool initializeForWindow(Platform::LowWindow window) override;
        void shutdownForWindow(Platform::LowWindow window) override;

    private:
        Platform::LowWindow m_window_handle = nullptr;
    };
#endif

    ScopePtr<IDragDropTargetDelegate> DragDropTargetFactory::createDragDropTargetDelegate() {
#ifdef TOOLBOX_PLATFORM_WINDOWS
        return make_scoped<WindowsDragDropTargetDelegate>();
#elif defined(TOOLBOX_PLATFORM_LINUX)
        return make_scoped<LinuxDragDropTargetDelegate>();
#else
        return nullptr;
#endif
    }

#ifdef TOOLBOX_PLATFORM_WINDOWS

    HRESULT __stdcall WindowsDragDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState,
                                                       POINTL pt, DWORD *pdwEffect) {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        if (!action) {
            MimeData mime_data = createMimeDataFromDataObject(pDataObj);
            action = DragDropManager::instance().createDragAction(0, std::move(mime_data));
        }

        action->setHotSpot(ImVec2(pt.x, pt.y));

        DropTypes supported_drop_types = DropType::ACTION_NONE;
        if ((*pdwEffect & DROPEFFECT_COPY)) {
            supported_drop_types |= DropType::ACTION_COPY;
        }
        if ((*pdwEffect & DROPEFFECT_MOVE)) {
            supported_drop_types |= DropType::ACTION_MOVE;
        }
        if ((*pdwEffect & DROPEFFECT_LINK)) {
            supported_drop_types |= DropType::ACTION_LINK;
        }
        action->setSupportedDropTypes(supported_drop_types);

        m_delegate->onDragEnter(action);
        return S_OK;
    }

    HRESULT __stdcall WindowsDragDropTarget::DragOver(DWORD grfKeyState, POINTL pt,
                                                      DWORD *pdwEffect) {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        action->setHotSpot(ImVec2(pt.x, pt.y));
        m_delegate->onDragMove(action);
        return S_OK;
    }

    HRESULT __stdcall WindowsDragDropTarget::DragLeave() {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        m_delegate->onDragLeave(action);
        return S_OK;
    }

    HRESULT __stdcall WindowsDragDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState,
                                                  POINTL pt, DWORD *pdwEffect) {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        if (!action) {
            MimeData mime_data = createMimeDataFromDataObject(pDataObj);
            action = DragDropManager::instance().createDragAction(0, std::move(mime_data));
        }

        action->setHotSpot(ImVec2(pt.x, pt.y));

        DropTypes supported_drop_types = DropType::ACTION_NONE;
        if ((*pdwEffect & DROPEFFECT_COPY)) {
            supported_drop_types |= DropType::ACTION_COPY;
        }
        if ((*pdwEffect & DROPEFFECT_MOVE)) {
            supported_drop_types |= DropType::ACTION_MOVE;
        }
        if ((*pdwEffect & DROPEFFECT_LINK)) {
            supported_drop_types |= DropType::ACTION_LINK;
        }
        action->setSupportedDropTypes(supported_drop_types);

        m_delegate->onDrop(action);
        return S_OK;
    }

    void WindowsDragDropTargetDelegate::onDragEnter(RefPtr<DragAction> action) {
        double mouse_x, mouse_y;
        Input::GetMouseViewportPosition(mouse_x, mouse_y);

        ImVec2 mouse_pos = ImVec2(mouse_x, mouse_y);
        action->setTargetUUID(searchDropTarget(mouse_pos));

        GUIApplication::instance().dispatchEvent<DragEvent, true>(EVENT_DRAG_ENTER, mouse_x,
                                                                  mouse_y, action);
    }

    void WindowsDragDropTargetDelegate::onDragLeave(RefPtr<DragAction> action) {
        double mouse_x, mouse_y;
        Input::GetMouseViewportPosition(mouse_x, mouse_y);

        ImVec2 mouse_pos = ImVec2(mouse_x, mouse_y);
        action->setTargetUUID(searchDropTarget(mouse_pos));

        GUIApplication::instance().dispatchEvent<DragEvent, true>(EVENT_DRAG_LEAVE, mouse_x,
                                                                  mouse_y, action);
    }

    void WindowsDragDropTargetDelegate::onDragMove(RefPtr<DragAction> action) {
        double mouse_x, mouse_y;
        Input::GetMouseViewportPosition(mouse_x, mouse_y);

        ImVec2 mouse_pos = ImVec2(mouse_x, mouse_y);
        action->setTargetUUID(searchDropTarget(mouse_pos));

        GUIApplication::instance().dispatchEvent<DragEvent, true>(EVENT_DRAG_MOVE, mouse_x, mouse_y,
                                                                  action);
    }

    void WindowsDragDropTargetDelegate::onDrop(RefPtr<DragAction> action) {
        double mouse_x, mouse_y;
        Input::GetMouseViewportPosition(mouse_x, mouse_y);

        ImVec2 mouse_pos = ImVec2(mouse_x, mouse_y);
        action->setTargetUUID(searchDropTarget(mouse_pos));

        GUIApplication::instance().dispatchEvent<DropEvent, true>(mouse_pos, action);
    }

    bool WindowsDragDropTargetDelegate::initializeForWindow(Platform::LowWindow window) {
        if (std::find_if(m_targets.begin(), m_targets.end(),
                         [window](WindowsDragDropTarget *target) {
                             return target->GetWindowHandle() == window;
                         }) != m_targets.end()) {
            return true;
        }

        WindowsDragDropTarget *target = new WindowsDragDropTarget(this, window);
        if (FAILED(target->Initialize())) {
            TOOLBOX_ERROR("Failed to initialize drag drop target");
            delete target;
            return false;
        }

        m_targets.push_back(target);
        return true;
    }

    void WindowsDragDropTargetDelegate::shutdownForWindow(Platform::LowWindow window) {

        auto target_it = std::find_if(m_targets.begin(), m_targets.end(),
                                      [window](WindowsDragDropTarget *target) {
                                          return target->GetWindowHandle() == window;
                                      });
        if (target_it == m_targets.end()) {
            return;
        }

        WindowsDragDropTarget *target = *target_it;
        if (FAILED(target->Shutdown())) {
            TOOLBOX_ERROR("Failed to shutdown drag drop target");
        }

        m_targets.erase(target_it);
        delete target;
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)

    void LinuxDragDropTargetDelegate::onDragEnter(RefPtr<DragAction> action) {}

    void LinuxDragDropTargetDelegate::onDragLeave(RefPtr<DragAction> action) {}

    void LinuxDragDropTargetDelegate::onDragMove(RefPtr<DragAction> action) {}

    void LinuxDragDropTargetDelegate::onDrop(RefPtr<DragAction> action) {}

    bool LinuxDragDropTargetDelegate::initializeWindow(Platform::LowWindow window) { return false; }

#endif

}  // namespace Toolbox::UI