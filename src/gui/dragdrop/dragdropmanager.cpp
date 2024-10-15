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
        m_drag_thread.join();
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

        STDMETHODIMP_(ULONG) AddRef() { return ++m_refCount; }

        STDMETHODIMP_(ULONG) Release() {
            if (--m_refCount == 0) {
                delete this;
                return 0;
            }
            return m_refCount;
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
        ULONG m_refCount = 1;
    };

    // Create an IDataObject with CF_HDROP for file drag-and-drop
    class FileDropDataObject : public IDataObject {
    public:
        FileDropDataObject(LPCWSTR filePath) {
            m_refCount = 1;
            m_filePath = filePath;
        }

        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
            if (riid == IID_IUnknown || riid == IID_IDataObject) {
                *ppvObject = static_cast<IDataObject *>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() { return ++m_refCount; }

        STDMETHODIMP_(ULONG) Release() {
            if (--m_refCount == 0) {
                delete this;
                return 0;
            }
            return m_refCount;
        }

        STDMETHODIMP GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) {
            if (pFormatEtc->cfFormat == CF_HDROP && pFormatEtc->tymed & TYMED_HGLOBAL) {
                // Allocate global memory for the file list
                SIZE_T filePathSize = (wcslen(m_filePath) + 1) * sizeof(WCHAR);
                SIZE_T totalSize    = sizeof(DROPFILES) + filePathSize;

                HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, totalSize);
                if (!hGlobal)
                    return E_OUTOFMEMORY;

                // Fill in the DROPFILES structure
                DROPFILES *pDropFiles = (DROPFILES *)GlobalLock(hGlobal);
                pDropFiles->pFiles    = sizeof(DROPFILES);  // Offset to the file list
                pDropFiles->fWide     = TRUE;               // Using Unicode (WCHAR)

                // Copy the file path to the memory block
                WCHAR *pDst = (WCHAR *)((BYTE *)pDropFiles + sizeof(DROPFILES));
                wcscpy(pDst, m_filePath);

                GlobalUnlock(hGlobal);

                // Set the STGMEDIUM
                pMedium->tymed          = TYMED_HGLOBAL;
                pMedium->hGlobal        = hGlobal;
                pMedium->pUnkForRelease = nullptr;
                return S_OK;
            }

            return DV_E_FORMATETC;  // Unsupported format
        }

        STDMETHODIMP GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) { return E_NOTIMPL; }

        STDMETHODIMP QueryGetData(FORMATETC *pFormatEtc) {
            if (pFormatEtc->cfFormat == CF_HDROP && pFormatEtc->tymed & TYMED_HGLOBAL) {
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
            return E_NOTIMPL;
        }

        STDMETHODIMP DAdvise(FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink,
                             DWORD *pdwConnection) {
            return E_NOTIMPL;
        }

        STDMETHODIMP DUnadvise(DWORD dwConnection) { return E_NOTIMPL; }

        STDMETHODIMP EnumDAdvise(IEnumSTATDATA **ppEnumAdvise) { return E_NOTIMPL; }

    private:
        ULONG m_refCount;
        LPCWSTR m_filePath;
    };

    Result<void, BaseError> DragDropManager::createSystemDragDropSource(MimeData &&data) {
        std::string paths = data.get_urls().value_or("");

        std::vector<PIDLIST_ABSOLUTE> pidls;
        std::vector<std::string_view> lines = splitLines(paths);

        size_t next_newline_pos = 0;
        for (const std::string_view &line : lines) {
            if (line.empty()) {
                continue;
            }

            std::wstring wpath = std::wstring(line.begin(), line.end());

            PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(wpath.c_str());
            if (pidl == NULL) {
                for (PIDLIST_ABSOLUTE pidl : pidls) {
                    ILFree(pidl);
                }
                return make_error<void>("DRAG_DROP", "Failed to create PIDL from path");
            }

            pidls.push_back(pidl);
        }

        IDataObject *data_object = nullptr;
        {
            HRESULT hr = SHCreateDataObject(NULL, static_cast<UINT>(pidls.size()),
                                            (PCIDLIST_ABSOLUTE *)pidls.data(), nullptr,
                                            IID_IDataObject, (void **)&data_object);

            for (PIDLIST_ABSOLUTE pidl : pidls) {
                ILFree(pidl);
            }

            if (FAILED(hr)) {
                return make_error<void>("DRAG_DROP", "Failed to create data object");
            }
        }

        DropSource *drop_source = new DropSource();

        {
            m_drag_thread = std::thread([data_object, drop_source]() {
                DWORD effect;
                DoDragDrop(data_object, drop_source, DROPEFFECT_MOVE, &effect);
                drop_source->Release();
                data_object->Release();
            });
        }

        m_data_object = data_object;
        m_drop_source = drop_source;
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