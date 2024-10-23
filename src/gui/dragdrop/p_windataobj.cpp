#include "p_windataobj.hpp"

namespace Toolbox::UI {

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::QueryInterface(REFIID riid, void **ppvObject) {
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG __stdcall) WindowsOleDataObject::AddRef() {
        return InterlockedIncrement(&m_ref_count);
    }

    STDMETHODIMP_(ULONG __stdcall) WindowsOleDataObject::Release() {
        ULONG ref_count = InterlockedDecrement(&m_ref_count);
        if (ref_count == 0) {
            delete this;
        }
        return ref_count;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) {
        if (pformatetcIn->tymed != TYMED_HGLOBAL) {
            return DV_E_TYMED;
        }

        if (pformatetcIn->cfFormat == CF_HDROP) {
            // TODO: Copy URIs from mime data to HDROP
        }

        if (pformatetcIn->cfFormat == CF_BITMAP) {
            // TODO: Copy image from mime data to HBITMAP
        }

        return S_OK;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium) {
        return DATA_E_FORMATETC;
    }

    STDMETHODIMP_(HRESULT __stdcall) WindowsOleDataObject::QueryGetData(FORMATETC *pformatetc) {
        return E_NOTIMPL;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) {
        pformatetcOut->ptd = nullptr;
        return E_NOTIMPL;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) {
        if (pformatetc->tymed != TYMED_HGLOBAL) {
            return DV_E_TYMED;
        }

        if (pformatetc->cfFormat == CF_HDROP) {
            // TODO: Copy URIs from mime data to HDROP
        }

        if (pformatetc->cfFormat == CF_BITMAP) {
            // TODO: Copy image from mime data to HBITMAP
        }

        return S_OK;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) {
        // TODO: Fill enumeration with supported formats
        return E_NOTIMPL;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink,
                                  DWORD *pdwConnection) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP_(HRESULT __stdcall) WindowsOleDataObject::DUnadvise(DWORD dwConnection) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::EnumDAdvise(IEnumSTATDATA **ppenumAdvise) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

}  // namespace Toolbox::UI
