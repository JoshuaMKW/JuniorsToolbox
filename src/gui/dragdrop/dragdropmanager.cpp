#pragma once

#include "gui/dragdrop/dragdropmanager.hpp"
#include "core/clipboard.hpp"
#include "core/memory.hpp"
#include "core/mimedata/mimedata.hpp"
#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#endif

namespace Toolbox::UI {

    RefPtr<DragAction> DragDropManager::createDragAction(UUID64 source_uuid, MimeData &&data,
                                                         bool system_level) {
        m_current_drag_action = make_referable<DragAction>(source_uuid);
        m_current_drag_action->setPayload(data);
        if (system_level) {
            createSystemDragDropSource(std::move(data));
        }
        return m_current_drag_action;
    }

    void DragDropManager::destroyDragAction(RefPtr<DragAction> action) {
        m_current_drag_action.reset();
    }

#ifdef TOOLBOX_PLATFORM_WINDOWS
    bool DragDropManager::initialize() { return OleInitialize(nullptr) >= 0; }

    void DragDropManager::shutdown() {
        OleUninitialize();
        if (m_is_thread_running) {
            m_drag_thread.join();
        }
    }

    static std::vector<std::string_view> splitLines(std::string_view s) {
        std::vector<std::string_view> result;
        size_t last_pos         = 0;
        size_t next_newline_pos = s.find('\n', 0);
        while (next_newline_pos != std::string::npos) {
            if (s[next_newline_pos - 1] == '\r') {
                result.push_back(s.substr(last_pos, next_newline_pos - last_pos - 1));
            } else {
                result.push_back(s.substr(last_pos, next_newline_pos - last_pos));
            }
            last_pos         = next_newline_pos + 1;
            next_newline_pos = s.find('\n', last_pos);
        }
        if (last_pos < s.size()) {
            if (s[s.size() - 1] == '\r') {
                result.push_back(s.substr(last_pos, s.size() - last_pos - 1));
            } else {
                result.push_back(s.substr(last_pos));
            }
        }
        return result;
    }

    class DropSource : public IDropSource {
    public:
        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
            if (riid == IID_IUnknown || riid == IID_IDropSource) {
                *ppvObject = static_cast<IDropSource *>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() { return ++m_ref_count; }

        STDMETHODIMP_(ULONG) Release() {
            if (--m_ref_count == 0) {
                delete this;
                return 0;
            }
            return m_ref_count;
        }

        STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
            if (fEscapePressed)
                return DRAGDROP_S_CANCEL;
            if (!(grfKeyState & MK_LBUTTON))
                return DRAGDROP_S_DROP;
            return S_OK;
        }

        STDMETHODIMP GiveFeedback(DWORD dwEffect) {
            return DRAGDROP_S_USEDEFAULTCURSORS;  // Use default drag-and-drop cursors
        }

    private:
        ULONG m_ref_count = 1;
    };

    // Create an IDataObject with CF_HDROP for file drag-and-drop
    class FileDropDataObject : public IDataObject {
    public:
        FileDropDataObject(const std::vector<std::string> &file_paths) : m_ref_count(1) {
            size_t total_path_size = 2;  // Double null-terminator
            for (const std::string &path : file_paths) {
                total_path_size += path.size() + 1;  // +1 for null-terminator
            }

            m_format_etc.cfFormat = CF_HDROP;
            m_format_etc.ptd      = nullptr;
            m_format_etc.dwAspect = DVASPECT_CONTENT;
            m_format_etc.lindex   = -1;
            m_format_etc.tymed    = TYMED_HGLOBAL;

            m_stg_medium.tymed = TYMED_HGLOBAL;
            m_stg_medium.hGlobal =
                GlobalAlloc(GHND, sizeof(DROPFILES) + total_path_size * sizeof(WCHAR));
            m_stg_medium.pUnkForRelease = nullptr;
            
            DROPFILES *drop_files = static_cast<DROPFILES *>(GlobalLock(m_stg_medium.hGlobal));
            if (!drop_files) {
                TOOLBOX_ERROR("Failed to lock global memory for DROPFILES structure");
                return;
            }

            drop_files->pFiles    = sizeof(DROPFILES);
            drop_files->fWide     = TRUE;

            WCHAR *file_path_buffer =
                reinterpret_cast<WCHAR *>((CHAR *)drop_files + drop_files->pFiles);
            for (const std::string &file_path : file_paths) {
                size_t path_size = file_path.size();
                std::mbstowcs(file_path_buffer, file_path.c_str(), path_size);
                file_path_buffer += path_size;
                *file_path_buffer++ = L'\0';
            }

            *file_path_buffer = L'\0';

            GlobalUnlock(m_stg_medium.hGlobal);
        }

        ~FileDropDataObject() {
            if (m_stg_medium.hGlobal) {
                GlobalFree(m_stg_medium.hGlobal);
            }
        }

        STDMETHODIMP
        QueryInterface(REFIID riid, void **ppvObject) {
            if (riid == IID_IUnknown || riid == IID_IDataObject) {
                *ppvObject = static_cast<IDataObject *>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() { return ++m_ref_count; }

        STDMETHODIMP_(ULONG) Release() {
            if (--m_ref_count == 0) {
                delete this;
                return 0;
            }
            return m_ref_count;
        }

        STDMETHODIMP GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) {
            if ((pFormatEtc->tymed & TYMED_HGLOBAL) == 0) {
                return DV_E_TYMED;  // Unsupported medium
            }

            if (pFormatEtc->cfFormat == CF_HDROP) {
                pMedium->tymed          = TYMED_HGLOBAL;
                pMedium->hGlobal         = m_stg_medium.hGlobal;
                pMedium->pUnkForRelease = nullptr;
                return S_OK;
            }

            return DV_E_FORMATETC;  // Unsupported format
        }

        STDMETHODIMP GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) { return E_NOTIMPL; }

        STDMETHODIMP QueryGetData(FORMATETC *pFormatEtc) {
            if ((pFormatEtc->tymed & TYMED_HGLOBAL) == 0) {
                return DV_E_TYMED;
            }
            if (pFormatEtc->cfFormat == CF_HDROP ||
                pFormatEtc->cfFormat == RegisterClipboardFormat(CFSTR_FILECONTENTS) ||
                pFormatEtc->cfFormat == RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR)) {
                return S_OK;
            }
            return DV_E_FORMATETC;
        }

        STDMETHODIMP GetCanonicalFormatEtc(FORMATETC *pFormatEtcIn, FORMATETC *pFormatEtcOut) {
            return DATA_S_SAMEFORMATETC;
        }

        STDMETHODIMP SetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium, BOOL fRelease) {
            return E_NOTIMPL;
        }

        STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc) {
            return OLE_S_USEREG;
        }

        STDMETHODIMP DAdvise(FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink,
                             DWORD *pdwConnection) {
            return E_NOTIMPL;
        }

        STDMETHODIMP DUnadvise(DWORD dwConnection) { return E_NOTIMPL; }

        STDMETHODIMP EnumDAdvise(IEnumSTATDATA **ppEnumAdvise) { return E_NOTIMPL; }

    private:
        ULONG m_ref_count;
        FORMATETC m_format_etc;
        STGMEDIUM m_stg_medium;
    };

    Result<void, BaseError> DragDropManager::createSystemDragDropSource(MimeData &&data) {
        auto maybe_paths = data.get_urls();
        if (!maybe_paths) {
            return make_error<void>("DRAG_DROP", "Passed Mimedata did not include urls");
        }

        IDataObject *data_object = new FileDropDataObject(std::move(maybe_paths.value()));
        IDropSource *drop_source = new DropSource();

        #if 0
        {
            if (m_is_thread_running) {
                m_drag_thread.join();
            }

            m_drag_thread = std::thread([data_object, drop_source]() {
                DWORD effect;
                DoDragDrop(data_object, drop_source, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
                drop_source->Release();
                data_object->Release();
            });

            m_data_object = data_object;
            m_drop_source = drop_source;
        }
        #elif 0
        {
            DWORD effect;
            DoDragDrop(data_object, drop_source, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
            drop_source->Release();
            data_object->Release();
        }
        #else
        #endif
        return {};
    }

    Result<void, BaseError> DragDropManager::destroySystemDragDropSource() {
        if (m_data_object) {
            m_data_object->Release();
        }
        if (m_drop_source) {
            m_drop_source->Release();
        }
        return {};
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)

    bool DragDropManager::initialize() { return false; }

    void DragDropManager::shutdown() {}

    Result<void, BaseError> DragDropManager::createSystemDragDropSource(MimeData &&data) {
        return {};
    }

    Result<void, BaseError> DragDropManager::destroySystemDragDropSource() { return {}; }

#endif

}  // namespace Toolbox::UI