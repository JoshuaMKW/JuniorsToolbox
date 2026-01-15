#include <chrono>

#include "gui/appmain/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/dragdrop/source.hpp"
#include "gui/event/dragevent.hpp"
#include "gui/event/dropevent.hpp"
#include "gui/window.hpp"

#include "gui/dragdrop/p_windataobj.hpp"

#include "image/imagebuilder.hpp"

#include "p_common.hpp"

#include <imgui.h>

using namespace std::chrono_literals;

namespace Toolbox::UI {

#ifdef TOOLBOX_PLATFORM_WINDOWS

#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

    static Input::MouseButtons s_last_mouse_buttons;

    enum class WindowsOLEDragDropMessage {
        NONE,
        DRAGSTART,
        DROPFILES,
        DRAGENTER,
        DRAGLEAVE,
        DRAGOVER,
        DRAGDROP
    };

    static DWORD convertDropTypes(DropTypes type) {
        DWORD drop_type = DROPEFFECT_NONE;
        if ((type & DropType::ACTION_COPY) != DropType::ACTION_NONE) {
            drop_type |= DROPEFFECT_COPY;
        }
        if ((type & DropType::ACTION_MOVE) != DropType::ACTION_NONE) {
            drop_type |= DROPEFFECT_MOVE;
        }
        if ((type & DropType::ACTION_LINK) != DropType::ACTION_NONE) {
            drop_type |= DROPEFFECT_LINK;
        }
        return drop_type;
    }

    static MimeData createMimeDataFromDataObject(IDataObject *pDataObj) {
        // Enumerate the formats available in the IDataObject
        IEnumFORMATETC *enum_format_etc = nullptr;
        {
            HRESULT hr = pDataObj->EnumFormatEtc(DATADIR_GET, &enum_format_etc);
            if (FAILED(hr)) {
                TOOLBOX_ERROR_V("Failed to enumerate data formats: {}", hr);
                return MimeData();
            }
        }

        MimeData mime_data;

        // Iterate through the formats and extract data
        FORMATETC format_etc;
        while (enum_format_etc->Next(1, &format_etc, nullptr) == S_OK) {
            if ((format_etc.dwAspect & DVASPECT_CONTENT) == 0) {
                continue;
            }

            if (format_etc.cfFormat == CF_HDROP) {
                // If the format is a file drop (CF_HDROP), we handle it specially

                // Get the medium for the CF_HDROP format
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

                // If we successfully locked the HDROP, we can query the files
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
                // If the format is Unicode text (CF_UNICODETEXT), we handle it

                // Get the medium for the CF_UNICODETEXT format
                STGMEDIUM stg_medium;
                {
                    HRESULT hr = pDataObj->GetData(&format_etc, &stg_medium);
                    if (FAILED(hr)) {
                        TOOLBOX_ERROR_V("Failed to get data for format: {}", hr);
                        return MimeData();
                    }
                }

                std::string text_data;

                // If the medium is a global memory handle, we can lock it and read the text
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
                // If the format is ANSI text (CF_TEXT), we handle it

                // Get the medium for the CF_TEXT format
                STGMEDIUM stg_medium;
                {
                    HRESULT hr = pDataObj->GetData(&format_etc, &stg_medium);
                    if (FAILED(hr)) {
                        TOOLBOX_ERROR_V("Failed to get data for format: {}", hr);
                        return MimeData();
                    }
                }

                std::string text_data;

                // If the medium is a global memory handle, we can lock it and read the text
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

    static HRESULT WindowsOLEMessageHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_DROPFILES) {
            HDROP hDrop = (HDROP)wParam;
            if (hDrop) {
                // Handle the drop files message
                UINT num_files = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
                std::vector<std::string> file_paths;
                file_paths.reserve(num_files);
                for (UINT i = 0; i < num_files; ++i) {
                    TCHAR file_path[MAX_PATH];
                    if (UINT length = DragQueryFile(hDrop, i, file_path, MAX_PATH)) {
                        file_paths.push_back(std::string(file_path, length));
                    }
                }
                DragFinish(hDrop);
                return S_OK;
            }
        }
        return E_NOTIMPL;
    }

    static HRESULT StartDragDrop(HWND wnd, IDataObject *data_object, IDropSource *drop_source,
                                 DWORD dw_ok_effects, LPDWORD effect_out) {
        bool started = false;

        while (true) {
            MSG msg = {};
            if (::GetMessage(&msg, wnd, 0, 0) <= 0) {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (msg.message == WM_QUIT) {
                return S_OK;  // Exit the drag-and-drop operation
            }

            if (msg.message == WM_MOUSEMOVE) {
                // Process mouse move events

                bool button_held = (msg.wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON |
                                                  MK_XBUTTON1 | MK_XBUTTON2)) != 0;
                if (!started && button_held) {
                    // return E_FAIL;
                }

                return ::DoDragDrop(data_object, drop_source, dw_ok_effects, effect_out);
            }

            if (msg.message == WM_POINTERUPDATE) {
                UINT pointer_id = GET_POINTERID_WPARAM(msg.wParam);

                POINTER_INFO pointer_info = {};
                if (::GetPointerInfo(pointer_id, &pointer_info) == 0) {
                    TOOLBOX_ERROR_V("Failed to get pointer info: {}",
                                    HRESULT_FROM_WIN32(GetLastError()));
                    return E_FAIL;
                }

                if ((pointer_info.pointerType & POINTER_FLAG_PRIMARY) != 0) {
                    DWORD flags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;
                    if (IS_POINTER_FIRSTBUTTON_WPARAM(msg.wParam)) {
                        flags |= MOUSEEVENTF_LEFTDOWN;
                    } else if (IS_POINTER_SECONDBUTTON_WPARAM(msg.wParam)) {
                        flags |= MOUSEEVENTF_RIGHTDOWN;
                    } else if (IS_POINTER_THIRDBUTTON_WPARAM(msg.wParam)) {
                        flags |= MOUSEEVENTF_MIDDLEDOWN;
                    }

                    if (!started) {
                        POINT pt = {};
                        if (::GetCursorPos(&pt)) {
                            bool mouse_moved = pt.x != pointer_info.ptPixelLocation.x ||
                                               pt.y != pointer_info.ptPixelLocation.y;

                            if ((flags & (MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_RIGHTDOWN |
                                          MOUSEEVENTF_MIDDLEDOWN)) &&
                                mouse_moved) {
                                // Convert the pointer location to virtual screen coordinates
                                const int orig_x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
                                const int orig_y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
                                const int virt_w = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
                                const int virt_h = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
                                const int virt_x = pointer_info.ptPixelLocation.x - orig_x;
                                const int virt_y = pointer_info.ptPixelLocation.y - orig_y;

                                INPUT input      = {};
                                input.type       = INPUT_MOUSE;
                                input.mi.dx      = static_cast<LONG>(virt_x * 65536 / virt_w);
                                input.mi.dy      = static_cast<LONG>(virt_y * 65536 / virt_h);
                                input.mi.dwFlags = flags;

                                if (::SendInput(1, &input, sizeof(INPUT)) == 0) {
                                    TOOLBOX_ERROR_V("Failed to send input: {}",
                                                    HRESULT_FROM_WIN32(GetLastError()));
                                    return E_FAIL;
                                }
                                started = true;  // Mark the drag-and-drop as started
                            }
                        }
                    }
                }
            }
        }
    }

    class WindowsDragDropSourceDelegate;

    class WindowsDragDropSource : public IDropSource {
    public:
        WindowsDragDropSource(WindowsDragDropSourceDelegate *delegate, Platform::LowWindow window)
            : m_delegate(delegate), m_window_handle(window), m_ref_count() {}

        ~WindowsDragDropSource() = default;

        Platform::LowWindow GetWindowHandle() const { return m_window_handle; }

        // IUnknown Methods
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override {
            if (riid == IID_IUnknown || riid == IID_IDropSource) {
                *ppvObject = static_cast<IDropSource *>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }
        //
        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref_count); }
        //
        ULONG STDMETHODCALLTYPE Release() override {
            ULONG ref_count = InterlockedDecrement(&m_ref_count);
            if (ref_count == 0) {
                delete this;
            }
            return ref_count;
        }
        //
        // IDropSource
        HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed,
                                                    DWORD grfKeyState) override;
        HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect) override;

        HRESULT Initialize() { return S_OK; }
        HRESULT Shutdown() { return S_OK; }

    private:
        WindowsDragDropSourceDelegate *m_delegate = nullptr;
        Platform::LowWindow m_window_handle       = nullptr;
        LONG m_ref_count;  // Reference count for COM object
    };

    class WindowsDragDropSourceDelegate : public IDragDropSourceDelegate {
    public:
        friend class WindowsDragDropSource;

        WindowsDragDropSourceDelegate();
        ~WindowsDragDropSourceDelegate() override;

        bool startDragDrop(Platform::LowWindow source, const MimeData &data,
                           DropTypes allowed_types, DropType *result_type_out);

        DragDropSourceState queryActiveDrag() override;
        ImGuiMouseCursor provideCursor() override;

        bool needsSynthesizedReleaseEvent() const;
        void emitSynthesizedReleaseEvent(Platform::LowWindow low_window);

        bool initializeForWindow(Platform::LowWindow window) override;
        void shutdownForWindow(Platform::LowWindow window) override;

    protected:
        struct DragDropMessage {
            WindowsOLEDragDropMessage type;
            IDataObject *data_object;
            WindowsDragDropSource *drop_source;
            DropTypes allowed_types;
            DropType *effect_out;
        };

        static HRESULT windowsOLEDragDropMessageHandler(WindowsDragDropSourceDelegate *delegate,
                                                        DragDropMessage &message);

    private:
        std::vector<WindowsDragDropSource *> m_sources = {};

        bool m_esc_key_pressed = false;
        DWORD m_qrf_key_state  = 0;

        Input::MouseButtons m_current_buttons = {};

        DropTypes m_drop_types = DropType::ACTION_NONE;

        // Resources for the OLE thread
        std::thread m_ole_thread;
        std::atomic<bool> m_is_thread_running{false};
        std::mutex m_thread_mutex;
        std::condition_variable m_thread_cv;
        std::queue<DragDropMessage> m_message_queue;
    };

#elif defined(TOOLBOX_PLATFORM_LINUX)

#endif

    ScopePtr<IDragDropSourceDelegate> DragDropDelegateFactory::createDragDropSourceDelegate() {
#ifdef TOOLBOX_PLATFORM_WINDOWS
        return make_scoped<WindowsDragDropSourceDelegate>();
#elif defined(TOOLBOX_PLATFORM_LINUX)
        return make_scoped<LinuxDragDropSourceDelegate>();
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

    HRESULT __stdcall WindowsDragDropSource::QueryContinueDrag(BOOL fEscapePressed,
                                                               DWORD grfKeyState) {
        m_delegate->m_esc_key_pressed                      = fEscapePressed;
        m_delegate->m_qrf_key_state                        = grfKeyState;
        IDragDropSourceDelegate::DragDropSourceState state = m_delegate->queryActiveDrag();

        switch (state) {
        case IDragDropSourceDelegate::DragDropSourceState::STATE_CANCEL:
            if (m_delegate->needsSynthesizedReleaseEvent()) {
                m_delegate->emitSynthesizedReleaseEvent(m_window_handle);
            }

            return DRAGDROP_S_CANCEL;
        case IDragDropSourceDelegate::DragDropSourceState::STATE_DROP:
            if (m_delegate->needsSynthesizedReleaseEvent()) {
                m_delegate->emitSynthesizedReleaseEvent(m_window_handle);
            }

            return DRAGDROP_S_DROP;
        default:
            MainApplication::instance().processEvents();
            return S_OK;
        }
    }

    HRESULT __stdcall WindowsDragDropSource::GiveFeedback(DWORD dwEffect) {
        m_delegate->m_drop_types = convertDropEffect(dwEffect);
        ImGuiMouseCursor cursor  = m_delegate->provideCursor();
        switch (cursor) {
        case ImGuiMouseCursor_None:
            return DRAGDROP_S_USEDEFAULTCURSORS;
        default:
            ImGui::SetMouseCursor(cursor);
            return S_OK;
        }
    }

    WindowsDragDropSourceDelegate::WindowsDragDropSourceDelegate() {
        m_is_thread_running = false;
        m_esc_key_pressed   = false;
        m_qrf_key_state     = 0;
        m_drop_types        = DropType::ACTION_NONE;

        // Initialize the OLE thread
        m_ole_thread = std::thread([this]() {
#if 0
            HRESULT hr = ::CoInitializeEx(NULL, ::COINIT_APARTMENTTHREADED | ::COINIT_DISABLE_OLE1DDE);
            if (FAILED(hr)) {
                TOOLBOX_ERROR("CoInitializeEx failed when setting up DragDrop message thread");
                return;
            }
#else
            HRESULT hr = ::OleInitialize(NULL);
            if (FAILED(hr)) {
                TOOLBOX_ERROR("OleInitialize failed when setting up DragDrop message thread");
                return;
            }
#endif

            m_is_thread_running = true;
            while (m_is_thread_running) {
                std::unique_lock<std::mutex> lock(m_thread_mutex);

                while (!m_message_queue.empty()) {
                    DragDropMessage msg = m_message_queue.front();
                    m_message_queue.pop();

                    // Handle the message
                    windowsOLEDragDropMessageHandler(this, msg);
                }

                m_thread_cv.wait(
                    lock, [this]() { return !m_is_thread_running || !m_message_queue.empty(); });
            }

#if 0
            ::CoUninitialize();
#else
            ::OleUninitialize();
#endif
        });
    }

    WindowsDragDropSourceDelegate::~WindowsDragDropSourceDelegate() {
        m_is_thread_running = false;
        m_thread_cv.notify_all();
        if (m_ole_thread.joinable()) {
            m_ole_thread.join();
        }
        // Clean up all sources
        for (WindowsDragDropSource *source : m_sources) {
            if (FAILED(source->Shutdown())) {
                TOOLBOX_ERROR("Failed to shutdown drag drop source");
            }
            delete source;
        }
        m_sources.clear();
    }

    bool WindowsDragDropSourceDelegate::startDragDrop(Platform::LowWindow src_window,
                                                      const MimeData &data, DropTypes allowed_types,
                                                      DropType *result_type_out) {
        if (m_sources.empty()) {
            TOOLBOX_ERROR("No drag drop sources initialized");
            return false;
        }

        if (!src_window) {
            src_window = ::GetFocus();
            if (!src_window) {
                TOOLBOX_ERROR("No source window handle provided and no focused window found");
                return false;
            }
        }

        auto source_it = std::find_if(m_sources.begin(), m_sources.end(),
                                      [src_window](WindowsDragDropSource *source) {
                                          return source->GetWindowHandle() == src_window;
                                      });
        if (source_it == m_sources.end()) {
            TOOLBOX_ERROR("No drag drop source found for window");
            return false;
        }

        WindowsOleDataObject *data_object = new WindowsOleDataObject();
        data_object->setMimeData(data);

        DWORD dw_effect;
        //::DoDragDrop(&data_object, *source_it, convertDropTypes(allowed_types), &dw_effect);

#if 0
        m_message_queue.push({WindowsOLEDragDropMessage::DRAGSTART, data_object, *source_it,
                              allowed_types, result_type_out});
#else
        HRESULT hr = StartDragDrop(src_window, data_object, *source_it,
                                   convertDropTypes(allowed_types), &dw_effect);

#endif

        m_thread_cv.notify_all();

        return true;
    }

    WindowsDragDropSourceDelegate::DragDropSourceState
    WindowsDragDropSourceDelegate::queryActiveDrag() {
        DragDropSourceState state   = {};
        Input::MouseButtons buttons = Input::GetPressedMouseButtons();

        bool drag_input_active = Input::GetMouseButton(Input::MouseButton::BUTTON_LEFT, true) ||
                                 Input::GetMouseButton(Input::MouseButton::BUTTON_RIGHT, true) ||
                                 Input::GetMouseButton(Input::MouseButton::BUTTON_MIDDLE, true);

        if (m_esc_key_pressed) {
            state = IDragDropSourceDelegate::DragDropSourceState::STATE_CANCEL;
        } else if (drag_input_active) {
            if (!buttons.empty() && m_current_buttons.empty()) {
                m_current_buttons = buttons;
            } else if (buttons != m_current_buttons) {
                state = IDragDropSourceDelegate::DragDropSourceState::STATE_DROP;
            } else {
                state = IDragDropSourceDelegate::DragDropSourceState::STATE_ACTIVE;
            }
        }

        return state;
    }

    // TODO: Provide a more specific cursor based on the drag-and-drop operation
    ImGuiMouseCursor WindowsDragDropSourceDelegate::provideCursor() {
        return ImGuiMouseCursor_Arrow;  // Default cursor for drag-and-drop
    }

    bool WindowsDragDropSourceDelegate::needsSynthesizedReleaseEvent() const {
        Input::MouseButtons buttons = Input::GetPressedMouseButtons();

        if (m_esc_key_pressed == false && buttons != s_last_buttons) {
            return true;
        }

        return false;
    }

    void
    WindowsDragDropSourceDelegate::emitSynthesizedReleaseEvent(Platform::LowWindow low_window) {
        RefPtr<ImWindow> window =
            MainApplication::instance().getImWindowFromPlatformWindow(low_window);

        // Get the local mouse position
        // ---
        double x, y;
        Input::GetMouseViewportPosition(x, y);
        ImVec2 position = {(float)x, (float)y};

        // Synthesize a mouse release event
        // ---
        MainApplication::instance().dispatchEvent<MouseEvent, true>(
            window->getUUID(), EVENT_MOUSE_RELEASE, position, Input::MouseButton::BUTTON_LEFT,
            Input::MouseButtonState::STATE_RELEASE);
    }

    bool WindowsDragDropSourceDelegate::initializeForWindow(Platform::LowWindow window) {
        if (std::find_if(m_sources.begin(), m_sources.end(),
                         [window](WindowsDragDropSource *source) {
                             return source->GetWindowHandle() == window;
                         }) != m_sources.end()) {
            return true;
        }

        WindowsDragDropSource *source = new WindowsDragDropSource(this, window);
        if (FAILED(source->Initialize())) {
            TOOLBOX_ERROR("Failed to initialize drag drop source");
            delete source;
            return false;
        }

        m_sources.push_back(source);
        return true;
    }

    void WindowsDragDropSourceDelegate::shutdownForWindow(Platform::LowWindow window) {
        auto source_it = std::find_if(m_sources.begin(), m_sources.end(),
                                      [window](WindowsDragDropSource *source) {
                                          return source->GetWindowHandle() == window;
                                      });
        if (source_it == m_sources.end()) {
            return;
        }

        WindowsDragDropSource *source = *source_it;
        if (FAILED(source->Shutdown())) {
            TOOLBOX_ERROR("Failed to shutdown drag drop source");
        }

        m_sources.erase(source_it);
        delete source;
    }

    HRESULT WindowsDragDropSourceDelegate::windowsOLEDragDropMessageHandler(
        WindowsDragDropSourceDelegate *delegate, DragDropMessage &msg) {

        // Handle the drag start message
        if (!msg.data_object || !msg.drop_source) {
            TOOLBOX_ERROR("Invalid data object or drop source");
            return E_INVALIDARG;
        }

        switch (msg.type) {
        case WindowsOLEDragDropMessage::DRAGSTART: {
            // Start the drag-and-drop operation
            DWORD dw_effect;

            Platform::LowWindow wnd = msg.drop_source->GetWindowHandle();

            HRESULT hr = StartDragDrop(wnd, msg.data_object, msg.drop_source,
                                       convertDropTypes(msg.allowed_types), &dw_effect);

            if (FAILED(hr)) {
                TOOLBOX_ERROR_V("Failed to start drag-and-drop operation: {}", hr);
                return hr;
            }

            if (msg.effect_out) {
                *msg.effect_out = convertDropEffect(dw_effect);
            }

            break;
        }
        }

        return S_OK;
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)

#endif

}  // namespace Toolbox::UI
