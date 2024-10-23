#include "gui/dragdrop/source.hpp"
#include "gui/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/event/dragevent.hpp"
#include "gui/event/dropevent.hpp"
#include "gui/window.hpp"

#include "image/imagebuilder.hpp"

#include <imgui.h>

namespace Toolbox::UI {

#ifdef TOOLBOX_PLATFORM_WINDOWS

#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "p_windataobj.hpp"

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

        WindowsDragDropSourceDelegate()           = default;
        ~WindowsDragDropSourceDelegate() override = default;

        bool startDragDrop(Platform::LowWindow source, MimeData &&data, DropTypes allowed_types,
                           DropType *result_type_out);
        DragDropSourceState queryActiveDrag() override { return DragDropSourceState::STATE_ACTIVE; }
        ImGuiMouseCursor provideCursor() override { return ImGuiMouseCursor_Hand; };

        bool initializeForWindow(Platform::LowWindow window) override;
        void shutdownForWindow(Platform::LowWindow window) override;

    private:
        std::vector<WindowsDragDropSource *> m_sources = {};

        bool m_esc_key_pressed = false;
        DWORD m_qrf_key_state  = 0;
        DropTypes m_drop_types = DropType::ACTION_NONE;
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
            return DRAGDROP_S_CANCEL;
        case IDragDropSourceDelegate::DragDropSourceState::STATE_DROP:
            return DRAGDROP_S_DROP;
        default:
            GUIApplication::instance().processEvents();
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

    bool WindowsDragDropSourceDelegate::startDragDrop(Platform::LowWindow src_window,
                                                      MimeData &&data, DropTypes allowed_types,
                                                      DropType *result_type_out) {
        if (m_sources.empty()) {
            TOOLBOX_ERROR("No drag drop sources initialized");
            return false;
        }

        auto source_it = std::find_if(m_sources.begin(), m_sources.end(),
                                      [src_window](WindowsDragDropSource *source) {
                                          return source->GetWindowHandle() == src_window;
                                      });
        if (source_it == m_sources.end()) {
            TOOLBOX_ERROR("No drag drop source found for window");
            return false;
        }

        WindowsOleDataObject data_object;
        data_object.setMimeData(std::move(data));

        DWORD dw_effect;
        ::DoDragDrop(&data_object, *source_it, convertDropTypes(allowed_types), &dw_effect);

        if (result_type_out) {
            *result_type_out = convertDropEffect(dw_effect);
        }

        return true;
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

#elif defined(TOOLBOX_PLATFORM_LINUX)

#endif

}  // namespace Toolbox::UI
