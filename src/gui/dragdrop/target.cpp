#include "gui/appmain/application.hpp"
#include "gui/dragdrop/target.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/event/dragevent.hpp"
#include "gui/event/dropevent.hpp"
#include "gui/logging/errors.hpp"
#include "gui/window.hpp"

#include "image/imagebuilder.hpp"

#include "p_common.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    static std::pair<UUID64, Platform::LowWindow> searchDropTarget(const ImVec2 &mouse_pos) {
        std::vector<RefPtr<ImWindow>> windows = GUIApplication::instance().getWindows();

        std::pair<UUID64, Platform::LowWindow> target = {0, 0};
        int target_z_order                            = std::numeric_limits<int>::max();
        int target_im_index                           = std::numeric_limits<int>::min();

        for (RefPtr<ImWindow> window : windows) {
            ImRect rect = {window->getPos(), window->getPos() + window->getSize()};
            if (rect.Contains(mouse_pos)) {
                if (window->getZOrder() < target_z_order) {
                    target.first    = window->getUUID();
                    target.second   = window->getLowHandle();
                    target_z_order  = window->getZOrder();
                    target_im_index = window->getImOrder();
                } else if (window->getZOrder() == target_z_order) {
                    if (window->getImOrder() >= target_im_index) {
                        target.first    = window->getUUID();
                        target.second   = window->getLowHandle();
                        target_im_index = window->getImOrder();
                    }
                }
            }
        }

        return target;
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
            if ((format_etc.dwAspect & DVASPECT_CONTENT) == 0) {
                continue;
            }

            if (format_etc.cfFormat == CF_HDROP) {
                STGMEDIUM stg_medium;
                {
                    HRESULT hr = pDataObj->GetData(&format_etc, &stg_medium);
                    if (FAILED(hr)) {
                        TOOLBOX_ERROR_V("Failed to get data for format: {}", hr);
                        return MimeData();
                    }
                }

                std::vector<std::string> uri_paths;

                HDROP hdrop = (HDROP)GlobalLock(stg_medium.hGlobal);

                if (hdrop) {
                    UINT num_files = DragQueryFile(hdrop, 0xFFFFFFFF, nullptr, 0);
                    uri_paths.reserve(num_files);
                    for (UINT i = 0; i < num_files; ++i) {
                        TCHAR file_path[MAX_PATH];
                        if (UINT length = DragQueryFile(hdrop, i, file_path, MAX_PATH)) {
                            uri_paths.push_back(std::string(file_path, length));
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

        bool m_block_implicit_drag_events = false;
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

    ScopePtr<IDragDropTargetDelegate> DragDropDelegateFactory::createDragDropTargetDelegate() {
#ifdef TOOLBOX_PLATFORM_WINDOWS
        return make_scoped<WindowsDragDropTargetDelegate>();
#elif defined(TOOLBOX_PLATFORM_LINUX)
        return make_scoped<LinuxDragDropTargetDelegate>();
#else
        return nullptr;
#endif
    }

#ifdef TOOLBOX_PLATFORM_WINDOWS

    static DropTypes convertDropEffect(DWORD dwEffect) {
        DropTypes drop_type = DropType::ACTION_NONE;
        if (dwEffect & DROPEFFECT_COPY) {
            drop_type |= DropType::ACTION_COPY;
        }
        if (dwEffect & DROPEFFECT_MOVE) {
            drop_type |= DropType::ACTION_MOVE;
        }
        if (dwEffect & DROPEFFECT_LINK) {
            drop_type |= DropType::ACTION_LINK;
        }
        return drop_type;
    }

    HRESULT __stdcall WindowsDragDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState,
                                                       POINTL pt, DWORD *pdwEffect) {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        if (!action) {
            // If during DragEnter the action is nullptr, that means
            // that Windows signaled DragEnter itself, mneaning we can
            // assume that the following operations are based on
            // system data (consider: Windows Explorer drag action).
            // ---
            // Thus we construct a new drag action with the
            // assumption that it is from the system and not
            // internal to the application.
            // ---
            std::pair<UUID64, Platform::LowWindow> target = searchDropTarget(ImVec2((float)pt.x, (float)pt.y));

            // Flag the manager that this is a system action we are making
            DragDropManager::instance().setSystemAction(true);
            MimeData mime_data = createMimeDataFromDataObject(pDataObj);

            action = DragDropManager::instance().createDragAction(target.first, target.second, std::move(mime_data),
                                                                  false);
            DragDropManager::instance().setSystemAction(false);

            action->setRender([action](const ImVec2 &pos, const ImVec2 &size) {
                auto maybe_urls = action->getPayload().get_urls();
                if (!maybe_urls) {
                    return;
                }
                std::vector<std::string> urls = maybe_urls.value();

                ImGuiStyle &style = ImGui::GetStyle();

                size_t num_files       = urls.size();
                std::string file_count = std::format("{}", num_files);
                ImVec2 text_size       = ImGui::CalcTextSize(file_count.c_str());

                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                ImVec2 center         = pos + (size / 2.0f);

                ImGui::DrawSquare(
                    (size / 2.0f), size.x / 5.0f, IM_COL32_WHITE,
                    ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_HeaderActive]), 1.0f);

                draw_list->AddText(pos + (size / 2.0f) - (text_size / 2.0f), IM_COL32_WHITE,
                                   file_count.c_str(), file_count.c_str() + file_count.size());
            });
        }

        s_last_buttons = Input::GetPressedMouseButtons();

        action->setHotSpot(ImVec2((float)pt.x, (float)pt.y));

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
        if (!action) {
            BaseError err = make_error<HRESULT>("OLE_DRAG_OVER", "DragAction was nullptr!").error();
            UI::LogError(err);
            return E_POINTER;
        }

        action->setHotSpot(ImVec2((float)pt.x, (float)pt.y));
        m_delegate->onDragMove(action);
        return S_OK;
    }

    HRESULT __stdcall WindowsDragDropTarget::DragLeave() {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        if (!action) {
            BaseError err = make_error<HRESULT>("OLE_DRAG_LEAVE", "DragAction was nullptr!").error();
            UI::LogError(err);
            return E_POINTER;
        }

        s_last_buttons = Input::GetPressedMouseButtons();

        m_delegate->onDragLeave(action);
        return S_OK;
    }

    HRESULT __stdcall WindowsDragDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState,
                                                  POINTL pt, DWORD *pdwEffect) {
        RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
        if (!action) {
            std::pair<UUID64, Platform::LowWindow> target =
                searchDropTarget(ImVec2((float)pt.x, (float)pt.y));

            MimeData mime_data = createMimeDataFromDataObject(pDataObj);
            action = DragDropManager::instance().createDragAction(target.first, target.second,
                                                                  std::move(mime_data));

            action->setRender([action](const ImVec2 &pos, const ImVec2 &size) {
                auto maybe_urls = action->getPayload().get_urls();
                if (!maybe_urls) {
                    return;
                }

                size_t num_files = maybe_urls.value().size();

                ImDrawList *draw_list = ImGui::GetBackgroundDrawList();
                ImVec2 center         = pos + (size / 2.0f);

                ImGui::DrawSquare(center, size.x / 6.0f, IM_COL32_BLACK, IM_COL32_WHITE, 2.0f);

                std::string file_count = std::format("{}", num_files);
                ImVec2 text_size       = ImGui::CalcTextSize(file_count.c_str());

                draw_list->AddText(pos + (size / 2.0f) - (text_size / 2.0f), IM_COL32_WHITE,
                                   file_count.c_str(), file_count.c_str() + file_count.size());
            });
        }

        s_last_buttons = Input::GetPressedMouseButtons();

        action->setHotSpot(ImVec2((float)pt.x, (float)pt.y));

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
        ImVec2 mouse_pos = action->getHotSpot();
        UUID64 prev_uuid = action->getTargetUUID();
        UUID64 new_uuid  = searchDropTarget(mouse_pos).first;

        // If the target UUID has changed, we need to dispatch
        // drag enter/leave events accordingly.
        if (prev_uuid != new_uuid) {
            // If we are blocking implicit drag events, we don't
            // dispatch drag leave events for the previous target.
            if (prev_uuid != 0) {
                GUIApplication::instance().dispatchEvent<DragEvent, true>(
                    EVENT_DRAG_LEAVE, mouse_pos.x, mouse_pos.y, action);
            }

            action->setTargetUUID(new_uuid);

            // If the new target UUID is valid, we dispatch a drag enter event.
            if (new_uuid != 0) {
                GUIApplication::instance().dispatchEvent<DragEvent, true>(
                    EVENT_DRAG_ENTER, mouse_pos.x, mouse_pos.y, action);
            }
        }
    }

    void WindowsDragDropTargetDelegate::onDragLeave(RefPtr<DragAction> action) {
        ImVec2 mouse_pos = action->getHotSpot();
        UUID64 prev_uuid = action->getTargetUUID();
        UUID64 new_uuid  = searchDropTarget(mouse_pos).first;

        // If the target UUID has changed, we need to dispatch
        // drag leave events accordingly.
        if (prev_uuid != new_uuid && prev_uuid != 0) {
            GUIApplication::instance().dispatchEvent<DragEvent, true>(EVENT_DRAG_LEAVE, mouse_pos.x,
                                                                      mouse_pos.y, action);
        }

        m_block_implicit_drag_events = true;
    }

    void WindowsDragDropTargetDelegate::onDragMove(RefPtr<DragAction> action) {
        ImVec2 mouse_pos = action->getHotSpot();
        UUID64 prev_uuid = action->getTargetUUID();
        UUID64 new_uuid  = searchDropTarget(mouse_pos).first;

        // If the target UUID has changed, we need to dispatch
        // drag enter/leave events accordingly.
        if (prev_uuid != new_uuid) {
            // If we are blocking implicit drag events, we don't
            // dispatch drag leave events for the previous target.
            if (!m_block_implicit_drag_events && prev_uuid != 0) {
                GUIApplication::instance().dispatchEvent<DragEvent, true>(
                    EVENT_DRAG_LEAVE, mouse_pos.x, mouse_pos.y, action);
            }

            m_block_implicit_drag_events = false;
            action->setTargetUUID(new_uuid);

            // If the new target UUID is valid, we dispatch a drag enter event.
            if (new_uuid != 0) {
                GUIApplication::instance().dispatchEvent<DragEvent, true>(
                    EVENT_DRAG_ENTER, mouse_pos.x, mouse_pos.y, action);
            }
        }

        // Update the target UUID regardless of whether it has changed or not.
        action->setTargetUUID(new_uuid);

        // Regardless of whether the target UUID has changed or not,
        // we always dispatch a drag move event to update the
        // position of the drag action.
        if (new_uuid != 0) {
            GUIApplication::instance().dispatchEvent<DragEvent, true>(EVENT_DRAG_MOVE, mouse_pos.x,
                                                                      mouse_pos.y, action);
        }
    }

    void WindowsDragDropTargetDelegate::onDrop(RefPtr<DragAction> action) {
        ImVec2 mouse_pos = action->getHotSpot();

        UUID64 new_uuid = searchDropTarget(mouse_pos).first;
        action->setTargetUUID(new_uuid);

        if (new_uuid != 0) {
            GUIApplication::instance().dispatchEvent<DropEvent, true>(mouse_pos, action);
        }
    }

    bool WindowsDragDropTargetDelegate::initializeForWindow(Platform::LowWindow window) {
        if (std::find_if(m_targets.begin(), m_targets.end(),
                         [window](WindowsDragDropTarget *target) {
                             return target->GetWindowHandle() == window;
                         }) != m_targets.end()) {
            return true;
        }

        // Create a new drag drop target for the window
        // This will register the window as a drop target
        // with the Windows OLE system.
        // ---
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

        // When shutting down the target, the internal
        // method Release() gets called within Shutdown()
        // by Windows, so we don't have to delete target
        // since Release() does it for us...
        // ---
        WindowsDragDropTarget *target = *target_it;
        if (FAILED(target->Shutdown())) {
            TOOLBOX_ERROR("Failed to shutdown drag drop target");
        }

        m_targets.erase(target_it);
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)

    void LinuxDragDropTargetDelegate::onDragEnter(RefPtr<DragAction> action) {}

    void LinuxDragDropTargetDelegate::onDragLeave(RefPtr<DragAction> action) {}

    void LinuxDragDropTargetDelegate::onDragMove(RefPtr<DragAction> action) {}

    void LinuxDragDropTargetDelegate::onDrop(RefPtr<DragAction> action) {}

    bool LinuxDragDropTargetDelegate::initializeForWindow(Platform::LowWindow window) {
        return false;
    }

    void LinuxDragDropTargetDelegate::shutdownForWindow(Platform::LowWindow window) {}

#endif

}  // namespace Toolbox::UI
